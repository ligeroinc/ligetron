#pragma once

#include <boost/multiprecision/integer.hpp>
#include <boost/integer/common_factor.hpp>
// #include <generator.hpp>

namespace ligero::vm::zkp {

using namespace boost;
namespace mp = boost::multiprecision;

// template <typename Integer>
// constexpr Integer root_of_unity(const Integer& g, const Integer& n, const Integer& modulus) {
//     return mp::powm(g, n, modulus);
// }

template <typename Integer>
Integer next_root_of_unity(const Integer& seed, const Integer& n, const Integer& modulus) {
    Integer phi = 0;
    for (Integer i = 1; i < n; i++) {
        if (integer::gcd(i, n) == Integer(1)) {
            phi++;
        }
    }
    for (Integer i = 2; i < modulus; i++) {
        if (integer::gcd(i, phi) == Integer(1)) {
            return mp::powm(seed, i, modulus);
        }
    }
    return seed;
}

// template <typename Integer>
// zkp::generator<Integer> primitive_root(const Integer& modulus) {
//     const Integer phi = modulus - 1;
//     for (Integer candidate = 2; candidate < modulus; candidate++) {
//         bool found = true;
//         for (Integer factor = 2; factor < phi - 1; factor++) {
//             if (phi % factor == 0) {
//                 if (mp::powm(candidate, phi / factor, modulus) == Integer(1)) {
//                     found = false;
//                     break;
//                 }
//             }
//         }
//         if (found) co_yield candidate;
//     }
// }

}  // namespace ligero
