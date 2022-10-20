#pragma once

#include <array>
#include <tuple>

#include <prelude.hpp>

namespace ligero::vm {

struct fixed_vector_expr { };

template <typename T>
concept IsFVExpr = std::derived_from<std::remove_reference_t<T>, fixed_vector_expr>;

template <typename T, size_t Len>
struct fixed_vector : public fixed_vector_expr {
    using value_type = T;

    constexpr static size_t size = Len;

    fixed_vector() : data_{} { }
    fixed_vector(const T& v) : data_{} { data_.fill(v); }

    fixed_vector(const fixed_vector&) = default;
    fixed_vector(fixed_vector&&) = default;
    fixed_vector& operator=(const fixed_vector&) = default;
    fixed_vector& operator=(fixed_vector&&) = default;

    // template <typename... Args> requires ((!IsFVExpr<Args> && ...))
    // fixed_vector(Args&&... args) : data_{{ std::forward<Args>(args)... }} { }
    
    // explicit fixed_vector(std::convertible_to<value_type> auto&&... ts)
    //     : data_{ std::forward<decltype(ts)>(ts)... } { }

    template <IsFVExpr Op>
    fixed_vector(const Op& op) : data_{} {
        #pragma omp simd
        for (size_t i = 0; i < Op::size; i++) {
            data_[i] = op[i];
        }
    }

    auto begin() { return data_.begin(); }
    auto end()   { return data_.end(); }
    auto begin() const { return data_.begin(); }
    auto end()   const { return data_.end(); }

    auto& data() { return data_; }
    const auto& data() const { return data_; }

    // constexpr size_t size() const { return Len; }
    bool empty() const { return data_.empty(); }
    
    const value_type& operator[](size_t index) const { return data_[index]; }
    value_type& operator[](size_t index) { return data_[index]; }

    template <typename Archive>
    void serialize(Archive& ar, unsigned int version) {
        ar & data_;
    }

    // const primitive_poly& eval(primitive_poly& out) const { return *this; }
    
protected:
    std::array<T, Len> data_;
};

/* ------------------------------------------------------------ */

template <typename Op, typename... Args>
struct fv_expr : public fixed_vector_expr {
    using tuple_type = std::tuple<Args...>;
    using value_type = std::common_type_t<typename std::remove_reference_t<Args>::value_type...>;
    
    Op op_;
    tuple_type args_;

    static_assert((std::remove_reference_t<Args>::size == ...));
    constexpr static size_t size = std::min(std::remove_reference_t<Args>::size...);

    constexpr fv_expr(Op op, Args&&... args) : op_(op), args_(std::forward<Args>(args)...) { }

    // size_t size() const { return size; }

    auto operator[](size_t index) const {
        return std::apply([&](const auto&... args) {
            return op_(args[index]...);
        }, args_);
    }
};


template <IsFVExpr Op1, IsFVExpr Op2>
auto operator+(Op1&& op1, Op2&& op2) {
    return fv_expr<std::plus<>, Op1, Op2>(std::plus{}, std::forward<Op1>(op1), std::forward<Op2>(op2));
}

template <IsFVExpr Op1, IsFVExpr Op2>
auto operator-(Op1&& op1, Op2&& op2) {
    return fv_expr<std::minus<>, Op1, Op2>(std::minus{}, std::forward<Op1>(op1), std::forward<Op2>(op2));
}

template <IsFVExpr Op1, IsFVExpr Op2>
auto operator*(Op1&& op1, Op2&& op2) {
    return fv_expr<std::multiplies<>, Op1, Op2>(std::multiplies{}, std::forward<Op1>(op1), std::forward<Op2>(op2));
}

template <IsFVExpr Op1, IsFVExpr Op2>
auto operator/(Op1&& op1, Op2&& op2) {
    return fv_expr<std::divides<>, Op1, Op2>(std::divides{}, std::forward<Op1>(op1), std::forward<Op2>(op2));
}

template <IsFVExpr Op>
auto operator>>(Op&& op, size_t n) {
    using OPT = std::remove_reference_t<Op>;
    using value_type = typename OPT::value_type;
    using vec_type = fixed_vector<value_type, OPT::size>;
    return fv_expr<prelude::shiftr<>, Op, vec_type>(prelude::shiftr{}, std::forward<Op>(op), vec_type(static_cast<value_type>(n)));
}

template <IsFVExpr Op>
auto operator&(Op&& op, size_t n) {
    using OPT = std::remove_reference_t<Op>;
    using value_type = typename OPT::value_type;
    using vec_type = fixed_vector<value_type, OPT::size>;
    return fv_expr<std::bit_and<>, Op, vec_type>(std::bit_and{}, std::forward<Op>(op), fixed_vector<value_type, OPT::size>(static_cast<value_type>(n)));
}

}  // namespace ligero::zkp
