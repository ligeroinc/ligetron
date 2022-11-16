#pragma once

#include <random>
#include <memory>

#include <hexl/hexl.hpp>

#include <zkp/prime_field.hpp>

namespace ligero::vm::zkp {

using namespace intel;

// template <typename> struct poly_base { };
struct poly_field_expr { };

template <typename Poly>
concept IsPolyExpr = std::derived_from<Poly, poly_field_expr>;

template <uint64_t Modulus>
struct primitive_poly : public poly_field_expr
// public poly_base<primitive_poly<Modulus>>
{
    using value_type = uint64_t;
    using signed_value_type = std::make_signed_t<value_type>;
    using field_type = prime_field<Fp64<Modulus>>;

    constexpr static value_type modulus = Modulus;
    constexpr static bool is_leaf = true;

    primitive_poly() = default;
    explicit primitive_poly(size_t n) : data_(n, 0) { }
    primitive_poly(size_t n, const field_type& v) : data_(n, static_cast<value_type>(v)) { }
    primitive_poly(std::initializer_list<value_type> vals) : data_(vals) { }

    // primitive_poly(const primitive_poly& other) : data_(other.data_) { }
    // primitive_poly(primitive_poly&& other) : data_(std::move(other.data_)) { }
    
    template <typename Iter>
    primitive_poly(Iter begin, Iter end) : data_(begin, end) {
        #pragma omp simd
        for (size_t i = 0; i < std::distance(begin, end); i++) {
            if (auto s = static_cast<signed_value_type>(data_[i]); s < 0) {
                data_[i] = s + modulus;
            }
        }
        reduce_coefficient();
    }

    template <IsPolyExpr Op>
    primitive_poly(const Op& op) : data_(op.size()) { op.eval(*this); }

    template <value_type M> requires (M <= Modulus)
        primitive_poly(const primitive_poly<M>& other) : data_(other.data()) { }

    template <value_type M> requires (M <= Modulus)
        primitive_poly(primitive_poly<M>&& other) : data_(std::move(other.data())) { }

    bool operator==(const primitive_poly& o) const {
        if (size() != o.size())
            return false;
        for (size_t i = 0; i < size(); i++) {
            if (data_[i] != o.data_[i])
                return false;
        }
        return true;
    }

    bool operator!=(const primitive_poly& o) const { return !(*this == o); }

    auto begin() { return data_.begin(); }
    auto end()   { return data_.end(); }
    auto begin() const { return data_.cbegin(); }
    auto end()   const { return data_.cend(); }

    auto& data() { return data_; }
    const auto& data() const { return data_; }

    size_t size() const { return data_.size(); }
    void resize(size_t N) { data_.resize(N); }

    bool empty() const { return data_.empty(); }
    void clear() { data_.clear(); }

    primitive_poly& push_back(value_type&& v) {
        data_.push_back(std::forward<value_type>(v));
        return *this;
    }

    template <typename... Args>
    primitive_poly& emplace_back(Args&&... args) {
        data_.emplace_back(std::forward<Args>(args)...);
        return *this;
    }

    primitive_poly& pad(size_t N, value_type v = 0) {
        if (size_t old_size = data_.size(); N > old_size) {
            data_.resize(N);
            #pragma omp simd
            for (size_t i = old_size; i < N; i++)
                data_[i] = v;
        }
        return *this;
    }

    template <typename RandomGenerator>
    primitive_poly& pad_random(size_t N, RandomGenerator& rand) {
        if (size_t old = data_.size(); N > old) {
            std::uniform_int_distribution<uint64_t> dist(0, modulus - 1);
            data_.resize(N);
            for (size_t i = old; i < N; i++)
                data_[i] = dist(rand);
        }
        return *this;
    }

    // primitive_poly& pad_random(size_t N, unsigned int seed = ) {
    //     std::random_device rd;
    //     std::mt19937_64 gen(rd());
    //     std::uniform_int_distribution<uint64_t> dist(0, modulus - 1);

    //     if (size_t old = data_.size(); N > old) {
    //         data_.resize(N);
    //         for (size_t i = old; i < N; i++)
    //             data_[i] = dist(gen);
    //     }
    //     return *this;
    // }
    
    const value_type& operator[](size_t index) const { return data_[index]; }
    value_type& operator[](size_t index) { return data_[index]; }

    void reduce_coefficient(size_t in = 0, size_t out = 1) {
        hexl::EltwiseReduceMod(data_.data(), data_.data(), data_.size(), modulus, in, out);
    }

    primitive_poly& operator+=(const primitive_poly& o) {
        hexl::EltwiseAddMod(data_.data(), data_.data(), o.data().data(), data_.size(), modulus);
        return *this;
    }

    primitive_poly& operator+=(value_type v) {
        hexl::EltwiseAddMod(data_.data(), data_.data(), v, data_.size(), modulus);
        return *this;
    }

    primitive_poly& operator-=(const primitive_poly& o) {
        hexl::EltwiseSubMod(data_.data(), data_.data(), o.data().data(), data_.size(), modulus);
        return *this;
    }

    primitive_poly& operator-=(value_type v) {
        hexl::EltwiseSubMod(data_.data(), data_.data(), v, data_.size(), modulus);
        return *this;
    }

    primitive_poly& operator*=(const primitive_poly& o) {
        hexl::EltwiseMultMod(data_.data(), data_.data(), o.data().data(), data_.size(), modulus, 1);
        return *this;
    }

    primitive_poly& operator*=(value_type v) {
        #pragma omp simd
        for (size_t i = 0; i < data_.size(); i++) {
            data_[i] = hexl::MultiplyMod(data_[i], v, modulus);
        }
        return *this;
    }

    primitive_poly& fma_mod(const primitive_poly& m, value_type scalar) {
        assert(m.size() == data_.size());
        hexl::EltwiseFMAMod(data_.data(), m.data().data(), scalar, data_.data(), data_.size(), modulus, 1);
        return *this;
    }

    primitive_poly& fma_mod(const primitive_poly& a, value_type scalar, const primitive_poly& c) {
        hexl::EltwiseFMAMod(data_.data(), a.data().data(), scalar, c.data().data(), data_.size(), modulus, 1);
        return *this;
    }

    template <typename Archive>
    void serialize(Archive& ar, unsigned int version) {
        ar & data_;
    }

    const primitive_poly& eval(primitive_poly& out) const { return *this; }
    
protected:
    hexl::AlignedVector64<uint64_t> data_;
};

// template <size_t N, uint64_t Modulus>
// struct NTT {
//     static hexl::NTT& ntt_obj() {
//         static hexl::NTT ntt{N, Modulus};
//         return ntt;
//     }

//     static void forward(primitive_poly<Modulus>& p) {
//         ntt_obj().ComputeForward(p.data().data(), p.data().data(), 1, 1);
//     }

//     static void inverse(primitive_poly<Modulus>& p) {
//         ntt_obj().ComputeInverse(p.data().data(), p.data().data(), 1, 1);
//     }
// };

/* ------------------------------------------------------------ */

template <typename Op, typename... Args>
struct poly_expr : public poly_field_expr {
    using tuple_type = std::tuple<const Args&...>;
    
    Op op_;
    tuple_type args_;

    static_assert((Args::modulus == ...));
    constexpr static auto modulus = std::remove_reference_t<typename std::tuple_element<0, tuple_type>::type>::modulus;
    constexpr static bool is_leaf = false;
    constexpr static bool all_leafs = (Args::is_leaf || ...);

    constexpr poly_expr(Op op, const Args&... args) : op_(op), args_(args...) { }

    size_t size() const {
        // assert(l_.size() == r_.size());
        // return l_.size();
        return std::get<0>(args_).size();
    }

    template <typename Out> requires (all_leafs)
    auto eval(Out& out) const {
        // At most one of the sub-expression will use the output
        return std::apply([&](const Args&... args) {
            return op_(out, modulus, args.eval(out)...);
        }, args_);
    }

    template <typename Out> requires (!all_leafs)
    auto eval(Out& out) const {
        return std::apply([&](const Args&... args) {
            return op_(out, modulus, args.eval(Out{out})...);
        }, args_);
        // auto tmp = out;
        // return op(out, modulus, l_.eval(tmp), r_.eval(out));
    }
};

namespace poly_ops {

struct add_mod {
    template <typename Out, typename M, typename Op1, typename Op2>
    const auto& operator()(Out& out, const M& modulus, const Op1& op1, const Op2& op2) const {
        assert(op1.size() == op2.size());
        hexl::EltwiseAddMod(out.data().data(), op1.data().data(), op2.data().data(), op1.size(), modulus);
        return out;
    }
};

struct sub_mod {
    template <typename Out, typename M, typename Op1, typename Op2>
    auto& operator()(Out& out, const M& modulus, const Op1& op1, const Op2& op2) const {
        assert(op1.size() == op2.size());
        hexl::EltwiseSubMod(out.data().data(), op1.data().data(), op2.data().data(), op1.size(), modulus);
        return out;
    }
};

struct mul_mod {
    template <typename Out, typename M, typename Op1, typename Op2>
    auto& operator()(Out& out, const M& modulus, const Op1& op1, const Op2& op2) const {
        assert(op1.size() == op2.size());
        hexl::EltwiseMultMod(out.data().data(), op1.data().data(), op2.data().data(), op1.size(), modulus, 1);
        return out;
    }
};

struct shiftr {
    template <typename Out, typename M, typename Op>
    auto& operator()(Out& out, const M& modulus, const Op& op) const {
        #pragma omp simd
        for (size_t i = 0; i < op.size(); i++) {
            out[i] = op[i] >> shift;
        }
        return out;
    }

    size_t shift;
};

struct mask_scalar {
    template <typename Out, typename M, typename Op>
    auto& operator()(Out& out, const M& modulus, const Op& op) const {
        #pragma omp simd
        for (size_t i = 0; i < op.size(); i++) {
            out[i] = op[i] & mask;
        }
        return out;
    }

    size_t mask;
};

}  // namespace polyop

template <IsPolyExpr Op1, IsPolyExpr Op2>
auto operator+(const Op1& op1, const Op2& op2) {
    return poly_expr(poly_ops::add_mod{}, op1, op2);
}

template <IsPolyExpr Op1, IsPolyExpr Op2>
auto operator-(const Op1& op1, const Op2& op2) {
    return poly_expr(poly_ops::sub_mod{}, op1, op2);
}

template <IsPolyExpr Op1, IsPolyExpr Op2>
auto operator*(const Op1& op1, const Op2& op2) {
    return poly_expr(poly_ops::mul_mod{}, op1, op2);
}

template <IsPolyExpr Op>
auto operator>>(const Op& op, size_t n) {
    return poly_expr(poly_ops::shiftr{ n }, op);
}

template <IsPolyExpr Op>
auto operator&(const Op& op, size_t n) {
    return poly_expr(poly_ops::mask_scalar{ n }, op);
}

}  // namespace ligero::zkp
