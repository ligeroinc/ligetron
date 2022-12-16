#pragma once

#include <array>

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
