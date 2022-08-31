#pragma once

#include <bit>

namespace ligero::prelude {

/// Helper function for std::visit
template <typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;  // deduction guide

template <typename InputIt, typename UnaryPredicate>
InputIt find_if_n(InputIt first, InputIt last, size_t n, UnaryPredicate p) {
    auto it = first;
    for (size_t i = 0; i < n; i++) {
        it = std::find_if(it, last, p);
    }
    return it;
}

#define PRELUDE_DECLARE_HELPER(NAME)                                    \
    template <>                                                         \
    struct NAME<void> {                                                 \
        template <typename T>                                           \
        constexpr T operator()(const T& x, const T& y) { return NAME<T>{}(x, y); } \
    };

template <typename T = void>
struct shiftl {
    constexpr T operator()(const T& num, const T& by) { return num << by; }
};

PRELUDE_DECLARE_HELPER(shiftl)

template <typename T = void>
struct shiftr {
    constexpr T operator()(const T& num, const T& by) { return num >> by; }
};

PRELUDE_DECLARE_HELPER(shiftr)

template <typename T = void>
struct rotatel {
    constexpr T operator()(const T& num, const T& by) { return std::rotl(num, by); }
};

PRELUDE_DECLARE_HELPER(rotatel)

template <typename T = void>
struct rotater {
    constexpr T operator()(const T& num, const T& by) { return std::rotr(num, by); }
};

PRELUDE_DECLARE_HELPER(rotater)

}  // namespace ligero::prelude
