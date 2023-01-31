#pragma once

#include <zkp/hash.hpp>

namespace ligero::vm::params {

constexpr uint64_t modulus_50bits = 1125625028935681ULL;
constexpr uint64_t modulus_60bits = 4611686018326724609ULL;

constexpr uint64_t modulus = modulus_50bits;

constexpr size_t num_code_test = 6;
constexpr size_t num_linear_test = 3;
constexpr size_t num_quadratic_test = 3;

using hasher = zkp::sha256;

}  // namespace ligero::vm::params
