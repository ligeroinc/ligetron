#pragma once

namespace ligero::zkp {

template <typename Number>
Number modulo(const Number& x, const Number& m) {
    if (x >= Number(0))
        return x % m;
    else
        return (m - x) % m;
}

}
