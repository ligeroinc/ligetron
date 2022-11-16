#pragma once

#include <type_traits>
#include <optional>
#include <memory>
#include <tuple>
#include <zkp/poly_field.hpp>

namespace ligero::vm::zkp {

struct zkp_var_expr { };

template <typename Expr>
concept IsZKPVar = std::derived_from<Expr, zkp_var_expr>;

namespace zkp_ops {

struct add { };
struct sub { };
struct mul { };
struct div { };
struct index_of { size_t index; };

}  // namespace zkp_ops

template <typename Op, typename... Args>
struct var_expression : public zkp_var_expr {
    Op op_;
    std::tuple<const Args&...> args_;

    var_expression(const Op& op, const Args&... args) : op_(op), args_(args...) { }

    template <typename Evaluator>
    auto eval(Evaluator& e) const {
        return std::apply([&](const Args&... ops) {
            return e.eval(op_, ops.eval(e)...);
        }, args_);
        // return e.eval(Ops, op1_.eval(e), op2_.eval(e));
    }
};

template <typename T, typename Poly>
struct zkp_var : public zkp_var_expr {
    zkp_var() = default;
    zkp_var(const Poly& p) : poly_(p) { }
    zkp_var(const T& v, const Poly& p) : val_(v), poly_(p) { }

    zkp_var(Poly&& p) : poly_(std::move(p)) { }
    zkp_var(T&& v, Poly&& p) : val_(std::move(v)), poly_(std::move(p)) { }

    T val() const { return val_; }
    const Poly& poly() const { return poly_; }

    auto operator[](size_t i) const {
        return var_expression(zkp_ops::index_of{ i }, *this);
    }

    template <typename... Args>
    const zkp_var& eval(Args&&...) const { return *this; }

    T val_;
    Poly poly_;
};

template <IsZKPVar Op1, IsZKPVar Op2>
auto operator+(const Op1& op1, const Op2& op2) {
    return var_expression(zkp_ops::add{}, op1, op2);
}

template <IsZKPVar Op1, IsZKPVar Op2>
auto operator-(const Op1& op1, const Op2& op2) {
    return var_expression(zkp_ops::sub{}, op1, op2);
}

template <IsZKPVar Op1, IsZKPVar Op2>
auto operator*(const Op1& op1, const Op2& op2) {
    return var_expression(zkp_ops::mul{}, op1, op2);
}

template <IsZKPVar Op1, IsZKPVar Op2>
auto operator/(const Op1& op1, const Op2& op2) {
    return var_expression(zkp_ops::div{}, op1, op2);
}

}  // namespace ligero::vm::zkp
