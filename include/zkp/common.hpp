#pragma once

namespace ligero::vm::zkp {

template <typename Number>
Number modulo(const Number& x, const Number& m) {
    return x % m;
}

template <typename Number>
Number modulo_neg(const Number& x, const Number& m) {
    return (x % m + m) % m;
}

}
