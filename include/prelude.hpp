#pragma once

#include <bit>

namespace ligero::prelude {

/// Helper function for std::visit
template <typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;  // deduction guide

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
struct rotl {
    constexpr T operator()(const T& num, const T& by) { return std::rotl(num, by); }
};

PRELUDE_DECLARE_HELPER(rotl)

template <typename T = void>
struct rotr {
    constexpr T operator()(const T& num, const T& by) { return std::rotr(num, by); }
};

PRELUDE_DECLARE_HELPER(rotr)

}  // namespace ligero::prelude
