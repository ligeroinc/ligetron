#pragma once

#include <compare>
#include <type_traits>
#include <limits>
#include <concepts>
#include <tuple>

#include <hexl/hexl.hpp>
#include <boost/integer/mod_inverse.hpp>
#include <boost/multiprecision/integer.hpp>

#include <zkp/common.hpp>
// #include <CRT.hpp>

namespace ligero::vm::zkp {

using namespace boost;
using namespace intel;
namespace mp = boost::multiprecision;

struct prime_field_expr { };

template <typename Fp>
concept IsPrimeFieldExpr = std::derived_from<Fp, prime_field_expr>;

template <typename Fp>
concept IsPrimeModulus = requires(Fp x) {
    typename Fp::value_type;
    { Fp::modulus } -> std::convertible_to<typename Fp::value_type>;
};

using no_reduce_t = std::false_type;
constexpr no_reduce_t no_reduce_coeffs;

template <typename Fp> requires IsPrimeModulus<Fp>
struct prime_field : prime_field_expr {
    using modulus_type = Fp;
    using value_type = typename Fp::value_type;
    using signed_value_type = typename Fp::signed_value_type;

    inline static value_type modulus = Fp::modulus;

    constexpr prime_field() : element_() { }
    constexpr prime_field(value_type e) : element_(modulo(e, modulus)) { }
    constexpr prime_field(value_type e, no_reduce_t) : element_(e) { }
    constexpr prime_field(signed_value_type se) : element_(modulo_neg<signed_value_type>(se, modulus)) { }
    // constexpr prime_field(value_type&& e) : element_(modulo_neg(std::move(e), modulus)) { }

    template <IsPrimeFieldExpr Expr>
    prime_field(const Expr& expr) : element_(expr.data()) { }


    prime_field& operator=(const prime_field& pf) {
        element_ = pf.element_;
        return *this;
    }
    
    template <IsPrimeFieldExpr Expr>
    prime_field& operator=(const Expr& expr) {
        element_ = expr.data();
        return *this;
    }

    prime_field& operator+=(const prime_field& other) {
        element_ = hexl::AddUIntMod(element_, other.element_, modulus);
        return *this;
    }

    prime_field& operator-=(const prime_field& other) {
        element_ = hexl::SubUIntMod(element_, other.element_, modulus);
        return *this;
    }

    prime_field& operator*=(const prime_field& other) {
        element_ = hexl::MultiplyMod(element_, other.element_, modulus);
        return *this;
    }

    // prime_field& operator%=(const value_type& v) {
    //     element_ %= v;
    //     return *this;
    // }

    template <typename Number>
    operator Number() const {
        if constexpr (requires { element_.template convert_to<Number>(); }) {
            return element_.template convert_to<Number>();
        }
        else {
            return static_cast<Number>(element_);
        }
    }

    value_type inverse() const {
        return hexl::InverseMod(element_, modulus);
    }
    
    void divide_qr(const prime_field& y, prime_field& q, prime_field& r) {
        mp::divide_qr(*this, y, q, r);
    }

    value_type& data() { return element_; }
    const value_type& data() const { return element_; }

private:
    value_type element_;
};

/* ------------------------------------------------------------ */

/* Auxiliary code for expression template support */

// template <template <typename> typename Op, typename... Args>
// struct pf_expression : prime_field_expr {
//     std::tuple<const Args&...> args_;

//     using modulus_type = std::common_type_t<typename Args::modulus_type...>;

//     pf_expression(const Args&... args) : args_(args...) { }

//     auto data() const {
//         return std::apply([](const Args&... ops) {
//             return Op<modulus_type>{}(ops.data()...);
//         }, args_);
//     }
// };

template <typename Fp>
prime_field<Fp> operator+(prime_field<Fp> op1, prime_field<Fp> op2) {
    return prime_field<Fp>(hexl::AddUIntMod(op1.data(), op2.data(), prime_field<Fp>::modulus), no_reduce_coeffs);
}

template <typename Fp>
prime_field<Fp> operator-(prime_field<Fp> op1, prime_field<Fp> op2) {
    return prime_field<Fp>(hexl::SubUIntMod(op1.data(), op2.data(), prime_field<Fp>::modulus), no_reduce_coeffs);
}

template <typename Fp>
prime_field<Fp> operator*(prime_field<Fp> op1, prime_field<Fp> op2) {
    return prime_field<Fp>(hexl::MultiplyMod(op1.data(), op2.data(), prime_field<Fp>::modulus), no_reduce_coeffs);
}

namespace field_ops {

// template <typename Modulus> requires IsPrimeModulus<Modulus>
// struct negate_mod {
//     template <typename Op>
//     auto operator()(const Op& op) const {
//         return (Modulus::modulus - op) % Modulus::modulus;
//     }
// };

// template <typename Modulus> requires IsPrimeModulus<Modulus>
// struct add_mod {
//     template <typename Op1, typename Op2>
//     auto operator()(const Op1& op1, const Op2& op2) const {
//         return hexl::AddUIntMod(op1, op2, Modulus::modulus);
//     }
// };

// template <typename Modulus> requires IsPrimeModulus<Modulus>
// struct sub_mod {
//     template <typename Op1, typename Op2>
//     auto operator()(const Op1& op1, const Op2& op2) const {
//         return hexl::SubUIntMod(op1, op2, Modulus::modulus);
//         // return (op1 + negate_mod<Modulus>{}(op2)) % Modulus::modulus;
//     }
// };

// template <typename Modulus> requires IsPrimeModulus<Modulus>
// struct mult_mod {
//     template <typename Op1, typename Op2>
//     auto operator()(const Op1& op1, const Op2& op2) const {
//         // return (op1 * op2) % Modulus::modulus;
//         return hexl::MultiplyMod(op1, op2, Modulus::modulus);
//     }
// };

// template <typename Modulus> requires IsPrimeModulus<Modulus>
// struct div_mod {
//     template <typename Op1, typename Op2>
//     auto operator()(const Op1& op1, const Op2& op2) const {
//         return mult_mod<Modulus>{}(op1, integer::mod_inverse(op2, Modulus::modulus));
//     }
// };

// template <typename Modulus> requires IsPrimeModulus<Modulus>
// struct pow_mod {
//     template <typename Op1, typename Op2>
//     auto operator()(const Op1& op1, const Op2& op2) const { return mp::powm(op1, op2, Modulus::modulus); }
// };

}  // namespace ops

// template <IsPrimeFieldExpr Op>
// auto operator-(const Op& op) {
//     return pf_expression<field_ops::negate_mod, Op>(op);
// }

// template <IsPrimeFieldExpr Op1, IsPrimeFieldExpr Op2>
// auto operator+(const Op1& op1, const Op2& op2) {
//     return pf_expression<field_ops::add_mod, Op1, Op2>(op1, op2);
// }

// template <IsPrimeFieldExpr Op1, IsPrimeFieldExpr Op2>
// auto operator-(const Op1& op1, const Op2& op2) {
//     return pf_expression<field_ops::sub_mod, Op1, Op2>(op1, op2);
// }

// template <IsPrimeFieldExpr Op1, IsPrimeFieldExpr Op2>
// auto operator*(const Op1& op1, const Op2& op2) {
//     return pf_expression<field_ops::mult_mod, Op1, Op2>(op1, op2);
// }

// template <IsPrimeFieldExpr Op1, IsPrimeFieldExpr Op2>
// auto operator/(const Op1& op1, const Op2& op2) {
//     return pf_expression<field_ops::div_mod, Op1, Op2>(op1, op2);
// }

// template <IsPrimeFieldExpr Op1, IsPrimeFieldExpr Op2>
// auto operator^(const Op1& op1, const Op2& op2) {
//     return pf_expression<field_ops::pow_mod, Op1, Op2>(op1, op2);
// }

/* ------------------------------------------------------------ */

/* Helper class for define a field prime */

template <typename T, T Prime>
struct primitive_modulus {
    using value_type = T;
    using signed_value_type = std::make_signed_t<value_type>;
    
    constexpr static T modulus = Prime;
};

template <uint32_t Prime>
using Fp32 = primitive_modulus<uint32_t, Prime>;

template <uint64_t Prime>
using Fp64 = primitive_modulus<uint64_t, Prime>;

}  // namespace ligero::zkp
