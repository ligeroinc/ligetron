#pragma once

#include <execution>
#include <optional>
#include <zkp/poly_field.hpp>
#include <zkp/random.hpp>
#include <zkp/number_theory.hpp>
#include <util/timer.hpp>

#include <chrono>

namespace ligero::vm::zkp {

struct reed_solomon64 {
    reed_solomon64(uint64_t modulus, size_t l, size_t d, size_t n)
        : modulus_(modulus), l_(l), d_(d), n_(n),
          ntt_message_(d, modulus), ntt_codeword_()
        {
            if (d == n) {
                uint64_t minimal_root = ntt_message_.GetMinimalRootOfUnity();
                // std::cout << "Minimal root: " << minimal_root << std::endl;
                uint64_t next_root = next_root_of_unity(minimal_root, static_cast<uint64_t>(2*n), modulus);
                // std::cout << "Next root: " << next_root << std::endl;
                ntt_codeword_ = hexl::NTT(n, modulus, next_root);
            }
            else {
                ntt_codeword_ = hexl::NTT(n, modulus);
            }
        }

    // template <typename Poly>
    // Poly& encode(Poly& poly) {
    //     // auto t = make_timer("NTT", __func__);
    //     poly.pad(l_);
    //     poly.pad_random(d_, random_);
    //     ntt_message_.ComputeInverse(poly.data().data(), poly.data().data(), 1, 4);
    //     poly.pad(n_);
    //     ntt_codeword_.ComputeForward(poly.data().data(), poly.data().data(), 4, 1);
    //     return poly;
    // }

    template <typename Poly, typename RandGen>
    Poly& encode_with(Poly& poly, RandGen& rand) {
        // auto t1 = make_timer(__func__, "pad1");
        poly.pad(l_);
        poly.pad_random(d_, rand);
        // t1.stop();
        // auto t2 = make_timer(__func__, "inverse");
        ntt_message_.ComputeInverse(poly.data().data(), poly.data().data(), 1, 4);
        // t2.stop();
        poly.pad(n_);
        // auto t4 = make_timer(__func__, "forward");
        ntt_codeword_.ComputeForward(poly.data().data(), poly.data().data(), 4, 1);
        // t4.stop();
        return poly;
    }

    template <typename Poly>
    Poly& encode_const(Poly& poly) {
        poly.pad(d_);
        ntt_message_.ComputeInverse(poly.data().data(), poly.data().data(), 1, 4);
        poly.pad(n_);
        ntt_codeword_.ComputeForward(poly.data().data(), poly.data().data(), 4, 1);
        return poly;
    }

    template <typename Iter>
    void encode(Iter begin, Iter end) {
        for (auto it = begin; it != end; it++) {
            encode(*it);
        }
    }

    // template <typename Poly>
    // void decode(Poly& poly) {
    //     ntt_codeword_.ComputeInverse(poly.data().data(), poly.data().data(), 1, 1);
    //     ntt_message_.ComputeForward(poly.data().data(), poly.data().data(), 1, 1);
    //     poly.data().erase(poly.begin() + l_, poly.end());
    // }

    template <typename Poly>
    void decode(Poly& poly) {
        assert(d_ * 2 == n_);
        assert(poly.size() == n_);
        
        ntt_codeword_.ComputeInverse(poly.data().data(), poly.data().data(), 1, 4);
        hexl::EltwiseSubMod(poly.data().data(),
                            poly.data().data(),
                            poly.data().data() + d_,
                            d_,
                            Poly::modulus);
        ntt_message_.ComputeForward(poly.data().data(), poly.data().data(), 4, 1);
        poly.data().erase(poly.begin() + l_, poly.end());
    }

    template <typename Poly>
    void partial_decode(Poly& poly) {
        ntt_codeword_.ComputeInverse(poly.data().data(), poly.data().data(), 1, 4);
        hexl::EltwiseSubMod(poly.data().data(),
                            poly.data().data(),
                            poly.data().data() + d_,
                            d_,
                            Poly::modulus);
        poly.resize(d_);
    }

    template <typename Poly>
    void partial_encode(Poly& poly) {
        poly.pad(n_);
        ntt_codeword_.ComputeForward(poly.data().data(), poly.data().data(), 4, 1);
    }

    uint64_t modulus() const { return modulus_; }
    size_t plain_size() const { return l_; }
    size_t padded_size() const { return d_; }
    size_t encoded_size() const { return n_; }

    // void seed(unsigned int seed) {
    //     random_.seed(seed);
    // }

    // void discard_random() {
    //     std::uniform_int_distribution<uint64_t> dist(0, modulus_ - 1);
    //     for (size_t i = 0; i < d_ - l_; i++)
    //         dist(random_);
    // }
protected:
    const uint64_t modulus_;
    const size_t l_, d_, n_;
    hexl::NTT ntt_message_, ntt_codeword_;
    // std::mt19937 random_;
};

}  // namespace ligero::zkp
