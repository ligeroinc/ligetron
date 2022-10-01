#pragma once

#include <array>

#include <sodium.h>

#include <zkp/hash.hpp>

namespace ligero::vm::zkp {

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

    // void seed(result_type i) {
    //     state_ = 0;
    //     offset_ = -1;
    //     std::fill_n(seed_.begin(), cycle_size, uint8_t{0});
    //     seed_.data[0] = i;
    // }

    // template <typename SeedSeq>
    // void seed(SeedSeq seq) {
    //     state_ = 0;
    //     offset_ = -1;
    //     uint32_t *p = reinterpret_cast<uint32_t*>(seed_.data);
    //     seq.generate(p, p + (Hasher::digest_size / sizeof(uint32_t)));
    // }

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

template <IsHashScheme Hasher>
struct PRNF {
    using result_type = uint64_t;

    template <typename K, typename... V> requires Hashable<K, Hasher> && (Hashable<V, Hasher> && ...)
    result_type operator()(const K& key, const V&... inputs) const {
        Hasher h;
        h << key;
        ((h << inputs), ...);
        auto digest = h.flush_digest();
        result_type random_bits = *reinterpret_cast<result_type*>(digest.data);
        return random_bits;
    }
};

// template <IsHashScheme Hasher> requires (Hasher::digest_size == randombytes_SEEDBYTES)
// struct pseudo_random_generator {
//     using seed_type = typename Hasher::digest;
//     constexpr static size_t seed_size = randombytes_SEEDBYTES;

//     explicit pseudo_random_generator() : seed_({0}) { }
//     explicit pseudo_random_generator(const unsigned char (&seed)[seed_size])
//         : seed_(std::to_array(seed)) { }
//     explicit pseudo_random_generator(const typename Hasher::digest& digest)
//         : seed_(std::to_array(digest.data)) { }
//     explicit pseudo_random_generator(const std::array<unsigned char, seed_size>& seed)
//         : seed_(seed) { }

//     pseudo_random_generator& operator()(void * const buf, size_t size) {
//         if (sodium_init() < 0)
//             throw std::runtime_error("Cannot initialize libsodium securely");
//         randombytes_buf_deterministic(buf, size, seed_.data());
//         update_seed({ reinterpret_cast<uint8_t const * const>(buf), size });
//         return *this;
//     }

//     /* Implement Fast Dice Roller algorithm described here [https://arxiv.org/pdf/1304.1916.pdf].
//      * Generating a true uniformly distributed number from random bytes  */
//     template <typename T>
//     T uniform_fdr(T upper_bound) {
//         constexpr size_t num_bits = std::numeric_limits<T>::digits;
//         constexpr size_t u8_size = std::numeric_limits<uint8_t>::digits;
//         constexpr size_t num_bytes = num_bits / u8_size;
        
//         uint8_t flips[num_bytes];
//         this->operator()(flips, num_bytes);
//         size_t curr = 0;
        
//         T v = 1, c = 0;
//         while (true) {
//             if (curr >= num_bits) {
//                 this->operator()(flips, num_bytes);
//                 curr = 0;
//             }
//             size_t block = curr / u8_size, offset = curr % u8_size;
//             T flip = ((static_cast<T>(1) << offset) & flips[block]) >> offset;
//             v = v << 1;
//             c = (c << 1) + flip;
//             if (v >= upper_bound) {
//                 if (c < upper_bound) {
//                     return c;
//                 }
//                 else {
//                     v -= upper_bound;
//                     c -= upper_bound;
//                 }
//             }
//             curr++;
//         }
//     }

// private:
//     void update_seed(byte_view view) {
//         hash_ << view;
//         seed_ = std::to_array(hash_.flush_digest().data);
//     }

// protected:
//     std::array<unsigned char, seed_size> seed_;
//     Hasher hash_;
// };

}  // namespace ligero::zkp
