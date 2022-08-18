#pragma once

#include <execution>
#include <optional>
#include <zkp/poly_field.hpp>
#include <zkp/random.hpp>
#include <zkp/number_theory.hpp>

namespace ligero::vm::zkp {

struct reed_solomon64 {
    reed_solomon64(uint64_t modulus, size_t l, size_t d, size_t n) : l_(l), d_(d), n_(n) {
        ntt_message_ = hexl::NTT(d, modulus);
        if (d == n) {
            uint64_t minimal_root = ntt_message_.GetMinimalRootOfUnity();
            // std::cout << "Minimal root: " << minimal_root << std::endl;
            uint64_t next_root = next_root_of_unity(minimal_root, 2*n, modulus);
            // std::cout << "Next root: " << next_root << std::endl;
            ntt_codeword_ = hexl::NTT(n, modulus, next_root);
        }
        else {
            ntt_codeword_ = hexl::NTT(n, modulus);
        }
    }

    template <typename Poly>
    Poly& encode(Poly& poly) {
        poly.pad_random(d_);
        ntt_message_.ComputeInverse(poly.data().data(), poly.data().data(), 1, 1);
        poly.pad(n_);
        ntt_codeword_.ComputeForward(poly.data().data(), poly.data().data(), 1, 1);
        return poly;
    }

    // template <typename Poly>
    // void encode(relation<Poly> *ptr) {
    //     if (auto *p = dynamic_cast<binary_relation<Poly>*>(ptr); p != nullptr) {
    //         encode(p->a());
    //         encode(p->b());
    //         encode(p->c());
    //     }
    // }

    template <typename Iter>
    void encode(Iter begin, Iter end) {
        #pragma omp parallel for
        for (auto it = begin; it != end; it++) {
            encode(*it);
        }
    }

    template <typename Poly>
    void decode(Poly& poly) {
        ntt_codeword_.ComputeInverse(poly.data().data(), poly.data().data(), 1, 1);
        ntt_message_.ComputeForward(poly.data().data(), poly.data().data(), 1, 1);
        poly.data().erase(poly.begin() + l_, poly.end());
        // poly.resize(l_);
    }

    size_t plain_size() const { return l_; }
    size_t padded_size() const { return d_; }
    size_t encoded_size() const { return n_; }

protected:
    const size_t l_, d_, n_;
    hexl::NTT ntt_message_, ntt_codeword_;
};

}  // namespace ligero::zkp
