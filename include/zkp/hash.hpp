#pragma once

#include <concepts>
#include <exception>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <sodium.h>
#include <string_view>
#include <type_traits>
#include <span>

namespace ligero::vm::zkp {

using byte_view = std::span<const uint8_t>;

struct hash_error : std::runtime_error { using std::runtime_error::runtime_error; };

template <typename Hash>
concept IsHashImpl = requires(Hash h, byte_view span, typename Hash::digest d) {
    { Hash::digest_size } -> std::convertible_to<size_t>;
    { h << span }         -> std::convertible_to<Hash&>;
    { h << d    }         -> std::convertible_to<Hash&>;
};

template <typename Derive>
struct overload_hash {
    overload_hash() : derive_(static_cast<Derive&>(*this)) { }

    template <typename T> requires std::is_fundamental_v<T>
    overload_hash& operator<<(T val) {
        derive_ << byte_view{ reinterpret_cast<const uint8_t*>(&val), sizeof(T) };
        return *this;
    }

    template <typename T> requires std::is_enum_v<T>
    overload_hash& operator<<(T val) {
        *this << static_cast<std::underlying_type_t<T>>(val);
        return *this;
    }

    template <typename T, size_t N>
    overload_hash& operator<<(const T (&arr)[N]) {
        for (size_t i = 0; i < N; i++)
            *this << arr[i];
        return *this;
    }

    template <typename T, size_t N>
    overload_hash& operator<<(const std::array<T, N>& arr) {
        for (const auto& v : arr)
            *this << v;
        return *this;
    }

    overload_hash& operator<<(const std::string& str) {
        derive_ << byte_view{ reinterpret_cast<const uint8_t*>(str.data()), str.size() };
        return *this;
    }
    
    template <typename T, typename Alloc>
    overload_hash& operator<<(const std::vector<T, Alloc>& vec) {
        for (const auto& v : vec)
            *this << v;
        return *this;
    }

    template <typename T, typename V>
    overload_hash& operator<<(const std::pair<T, V>& pair) {
        *this << pair.first;
        *this << pair.second;
        return *this;
    }

    template <typename... Args>
    overload_hash& operator<<(const std::tuple<Args...>& tuple) {
        std::apply([this](const auto&... args) {
            ((*this << args), ...);
        }, tuple);
        return *this;
    }

    /* If an object is serializable, then it's also hashable */
    template <typename T>
    requires requires(T t, overload_hash<Derive> op, unsigned int version) {
        { t.serialize(op, version) } -> std::same_as<void>;
    }
    overload_hash& operator<<(const T& val) {
        const_cast<T&>(val).serialize(*this, 0);
        return *this;
    }

    template <typename... Args>
    overload_hash& operator&(const Args&... args) {
        return (*this << ... << args);
    }

protected:
    Derive& derive_;
};

template <typename Hash>
concept IsHashScheme = IsHashImpl<Hash> && std::derived_from<Hash, overload_hash<Hash>>;

template <typename T, typename Hash>
concept Hashable = IsHashScheme<Hash> && requires(T t, Hash h) {
    { h << t };
};

/* ------------------------------------------------------------ */

/* Suppoert for older OpenSSL version (1.0.x or 1.1.x) */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#  define EVP_MD_CTX_new   EVP_MD_CTX_create
#  define EVP_MD_CTX_free  EVP_MD_CTX_destroy
#endif

template <typename T>
concept OpenSSLHash = requires (T t) {
    { T::digest_size } -> std::convertible_to<size_t>;
    { t() } -> std::convertible_to<const EVP_MD*>;
};

struct openssl_sha2_256_t {
    constexpr static size_t digest_size = SHA256_DIGEST_LENGTH;
    const EVP_MD* operator()() const { return EVP_sha256(); }
};

struct openssl_sha3_256_t {
    constexpr static size_t digest_size = SHA256_DIGEST_LENGTH;
    const EVP_MD* operator()() const { return EVP_sha3_256(); }
};

template <OpenSSLHash Hash>
struct openssl_hash : public overload_hash<openssl_hash<Hash>> {
    using overload_hash<openssl_hash<Hash>>::operator<<;
    constexpr static size_t digest_size = Hash::digest_size;

    struct digest {
        alignas(32) uint8_t data[digest_size] = { 0 };

        bool operator==(const digest& o) const {
            return std::equal(std::begin(data), std::end(data), std::begin(o.data));
        }

        auto begin() { return std::begin(data); }
        auto end() { return std::end(data); }
        size_t size() const { return digest_size; }
    };
    
    openssl_hash() : ctx_(nullptr) { init(); }
    // openssl_hash(byte_view seed) : ctx_(nullptr), seed_(seed) { init(); }
    
    ~openssl_hash() {
        EVP_MD_CTX_free(ctx_);
    }

    digest flush_digest() {
        digest d;
        if (!EVP_DigestFinal_ex(ctx_, d.data, nullptr))
            throw hash_error("Unable to flush digest");
        reset();
        return d;
    }

    openssl_hash& operator<<(byte_view view) {
        if (!EVP_DigestUpdate(ctx_, view.data(), view.size()))
            throw hash_error("Unable to update digest");
        return *this;
    }

    openssl_hash& operator<<(const digest& d) {
        *this << d.data;
        return *this;
    }

private:
    void init() {
        if (ctx_ = EVP_MD_CTX_new(); ctx_ == nullptr)
            throw hash_error("Cannot allocate OpenSSL EVP context");
        reset();
    }

    void reset() {
        if (!EVP_DigestInit_ex(ctx_, Hash{}(), nullptr))
            throw hash_error("Cannot initialize OpenSSL EVP context");
        // *this << seed_;
    }

private:
    EVP_MD_CTX *ctx_;
    // byte_view seed_;
};

using sha256 = openssl_hash<openssl_sha2_256_t>;
using sha3_256 = openssl_hash<openssl_sha3_256_t>;

/* ------------------------------------------------------------ */

struct sodium_blake2b : public overload_hash<sodium_blake2b> {
    using overload_hash<sodium_blake2b>::operator<<;
    constexpr static size_t digest_size = crypto_generichash_BYTES;

    struct digest {
        alignas(32) uint8_t data[digest_size];

        bool operator==(const digest& o) const {
            return std::equal(std::begin(data), std::end(data), std::begin(o.data));
        }

        auto begin() { return std::begin(data); }
        auto end() { return std::end(data); }
        size_t size() const { return digest_size; }
    };

    sodium_blake2b() : ctx_() { init(); }
    // sodium_blake2b(byte_view view) : ctx_(), seed_(view) { init(); }

    digest flush_digest() {
        digest d;
        crypto_generichash_final(&ctx_, d.data, d.size());
        init();
        return d;
    }

    sodium_blake2b& operator<<(byte_view view) {
        crypto_generichash_update(&ctx_, view.data(), view.size());
        return *this;
    }

    sodium_blake2b& operator<<(const digest& d) {
        *this << d.data;
        return *this;
    }

private:
    void init() {
        if (sodium_init() < 0) {
            throw hash_error("Cannot initialize LibSodium");
        }
        /* Initialize with a seed (key) */
        // crypto_generichash_init(&ctx_, seed_.data(), seed_.size(), digest_size);
        crypto_generichash_init(&ctx_, nullptr, 0, digest_size);
    }

protected:
    crypto_generichash_state ctx_;  // Hash context
    // byte_view seed_;                // Non-owning view of seed for space-efficiency
};

template <IsHashScheme Hasher, typename... Args>
typename Hasher::digest hash(const Args&... args) {
    Hasher h;
    ((h << args), ...);
    return h.flush_digest();
}

}  // namespace ligero::zkp
