#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>

#include <zkp/hash.hpp>

#include <openssl/evp.h>
#include <openssl/aes.h>

namespace ligero::vm::zkp {

template <typename T, size_t buffer_size = 16 * 1024>
struct aes256ctr_engine {
    using result_type = T;
    
    constexpr static size_t key_size = 32;
    constexpr static size_t block_size = AES_BLOCK_SIZE;

    constexpr static size_t num_elements = buffer_size / sizeof(T);

    constexpr static result_type min() { return std::numeric_limits<result_type>::min(); }
    constexpr static result_type max() { return std::numeric_limits<result_type>::max(); }

    struct deleter { void operator()(EVP_CIPHER_CTX *ctx) { EVP_CIPHER_CTX_free(ctx); } };
    
    aes256ctr_engine(unsigned char (&key)[key_size], unsigned char (&iv)[block_size])
        : ctx_(EVP_CIPHER_CTX_new())
    {
        int success = EVP_EncryptInit(ctx_.get(), EVP_aes_256_ctr(), key, iv);
        if (!success) {
            throw std::runtime_error("Cannot initialize AES context");
        }
        fill_buf();
    }

    void fill_buf() {
        int size = 0;
        unsigned char *ptr = reinterpret_cast<unsigned char*>(buffer_);
        EVP_EncryptUpdate(ctx_.get(), ptr, &size, data_, buffer_size);
    }

    result_type operator()() {
        if (pos_ >= num_elements) {
            fill_buf();
            pos_ = 0;
        }
        return reinterpret_cast<result_type*>(buffer_)[pos_++];
    }

    alignas(block_size) unsigned char buffer_[buffer_size];
    alignas(block_size) const unsigned char data_[buffer_size] = { 0 };
    std::unique_ptr<EVP_CIPHER_CTX, deleter> ctx_;
    size_t pos_ = 0;
};

template <IsHashScheme Hasher>
struct hash_random_engine {
    using result_type = uint8_t;
    using seed_type = typename Hasher::digest;
    
    constexpr static int cycle_size = Hasher::digest_size;

    constexpr static result_type min() { return std::numeric_limits<result_type>::min(); }
    constexpr static result_type max() { return std::numeric_limits<result_type>::max(); }

    hash_random_engine() { }
    // hash_random_engine(result_type i) { seed(i); }
    // template <typename SeedSeq>
    // hash_random_engine(SeedSeq seq) { seed(seq); }
    hash_random_engine(const seed_type& d) : seed_(d) { }

    void seed() {
        state_ = 0;
        offset_ = -1;
        std::fill_n(seed_.begin(), cycle_size, uint8_t{0});
    }

    void seed(const seed_type& d) {
        seed_ = d;
    }

    void discard(size_t i) {
        if (i <= offset_ + 1) {
            offset_ -= i;
        }
        else {
            int remind = i - (offset_ + 1);
            int q = remind / cycle_size, r = remind % cycle_size;

            hash_ << state_ + q - 1;
            buffer_ = hash_.flush_digest();
            hash_ << seed_;
            state_++;
            offset_ = cycle_size - 1 - r;
        }
    }

    result_type operator()() {
        if (offset_ < 0 || offset_ >= cycle_size) {
            hash_ << state_++;
            buffer_ = hash_.flush_digest();
            hash_ << seed_;
            offset_ = cycle_size - 1;
            
        }
        return buffer_.data[offset_--];
    }

protected:
    Hasher hash_;
    typename Hasher::digest seed_;
    typename Hasher::digest buffer_;
    size_t state_ = 0;
    int offset_ = -1;
};

}  // namespace ligero::zkp
