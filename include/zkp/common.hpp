#pragma once

#include <util/timer.hpp>

namespace ligero::vm::zkp {

template <typename Number>
Number modulo(const Number& x, const Number& m) {
    // auto t = make_timer(__func__);
    return x % m;
}

template <typename Number>
Number modulo_neg(const Number& x, const Number& m) {
    // auto t = make_timer(__func__);
    return (x % m + m) % m;
}

}
