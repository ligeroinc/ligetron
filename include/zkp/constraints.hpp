#pragma once

#include <concepts>

namespace ligero::zkp {

template <typename R>
concept IsRing = requires(R a, R b) {
    typename R::underlying_type;

    /* Make sure R is an abelian group under addition */
    R::zero();
    { a + b } -> std::convertible_to<R>;
    { -a }    -> std::convertible_to<R>;

    /* Make sure R is a monoid under multiplication */
    R::one();
    { a * b } -> std::convertible_to<R>;
};

/* A Field is a ring with multiplicative inverse */
template <typename F>
concept IsField = IsRing<F> && requires(F a) {
    { a.inverse() } -> std::convertible_to<F>;
};

/* A Finite Field is a Field that has finite order */
template <typename FF>
concept IsFiniteField = IsField<FF> && requires() {
    typename FF::numeric_type;
    { FF::modulo() } -> std::convertible_to<typename FF::numeric_type>;
    { FF::order() } -> std::convertible_to<typename FF::underlying_type>;
};

} // namespace ligero::zkp
