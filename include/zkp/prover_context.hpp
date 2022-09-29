#pragma once

#include <context.hpp>
#include <zkp/encoding.hpp>
#include <zkp/merkle_tree.hpp>
#include <zkp/argument.hpp>
#include <zkp/variable.hpp>

namespace ligero::vm::zkp {

// template <typename T>
// struct shared_tag {
//     shared_tag(T&& f) : val(std::forward<T>(f)) { }
//     shared_tag(T&& f, bool shared) : val(std::forward<T>(f)), secret_shared(shared) { }

//     T& operator*() { return val; }
//     const T& operator*() const { return val; }
    
//     operator bool() const { return secret_shared; }
//     operator T() const { return val; }

//     T val;
//     bool secret_shared = false;
// };

template <typename Fp>
struct zkp_context_base : public context_base {
    using Base = context_base;
    using field_poly = Fp;
    using field_type = typename Fp::field_type;
    using operator_type = standard_op;
    using var_type = zkp_var<u32, field_poly>;

    zkp_context_base(reed_solomon64& encoder) : encoder_(encoder) { }

    void stack_push(svalue_t&& val) override {
        std::visit(prelude::overloaded{
                [this](u32 v) { zstack_push(static_cast<s32>(v)); },
                [this](u64 v) { zstack_push(static_cast<s32>(v)); },
                [](const auto&) { }
            }, val);
        Base::stack_push(std::move(val));
    }

    svalue_t stack_pop_raw() override {
        auto val = Base::stack_pop_raw();
        std::visit(prelude::overloaded{
                [this](u32) { zstack_pop(); },
                [this](u64) { zstack_pop(); },
                [](const auto&) { }
            }, val);
        return std::move(val);
    }

    void stack_push(const var_type& var) {
        Base::stack_push(var.val());
        zstack_.push_back(var.poly());
    }

    var_type stack_pop_var() {
        auto val = Base::stack_pop_raw();
        assert(std::holds_alternative<u32>(val));
        return var_type{ std::get<u32>(val), zstack_pop() };
    }

    // template <typename T>
    // T stack_pop() {
    //     auto tmp = stack_pop_raw();
    //     return std::get<T>(std::move(tmp));
    // }

    // auto& zstack(size_t n) { return *(zstack_.rbegin() + n); }

    // void vstack_push(svalue_t&& sv) {
    //     Base::stack_push(std::move(sv));
    // }
    
    // template <typename T>
    // T vstack_pop() {
    //     return std::get<T>(Base::stack_pop_raw());
    // }

    // virtual void zstack_push_impl(const field_poly& f) {
    //     poly_type p(1, f);
    //     encoder_.encode(p);
    //     zstack_.push_back(std::move(p));
    // }
    
    field_poly encode(const field_type& v) {
        field_poly p(1, v);
        encoder_.encode(p);
        return p;
    }

    virtual void zstack_push(const field_type& v) {
        zstack_.push_back(encode(v));
    }

    // virtual void zstack_push(field_poly&& f) {
    //     zstack_.push_back(std::move(f));
    // }

    // template <typename... Args>
    // void zstack_pushs(Args&&... args) {
    //     ((zstack_push(std::forward<Args>(args))), ...);
    // }

    virtual field_poly zstack_pop() {
        auto x = zstack_.back();
        zstack_.pop_back();
        return x;
    }

    // void zstack_drop(size_t count, size_t offset_from_top = 0) {
    //     auto it = zstack_.rbegin() + offset_from_top;
    //     zstack_.erase((it + count).base(), it.base());
    // }

    // void count() const {
    //     std::cout << "field count: " << fcounter
    //               << " scounter: " << scounter << std::endl;
    // }

    virtual void assert_linear(const field_poly& z, const field_poly& x, const field_poly& y) {
        // do nothing - override in child class
    }

    virtual void assert_quadratic(const field_poly& z, const field_poly& x, const field_poly& y) {
        // do nothing - override in child class
    }

    template <typename Expr>
    auto eval(const Expr& expr) {
        return expr.eval(*this);
    }

    virtual var_type eval(zkp_ops::add, const var_type& x, const var_type& y) {
        field_type v = x.val() + y.val();
        field_poly p = x.poly() + y.poly();
        assert_linear(p, x.poly(), y.poly());  // p = x + y
        return var_type{ std::move(v), std::move(p) };
    }

    virtual var_type eval(zkp_ops::sub, const var_type& x, const var_type& y) {
        field_type v = x.val() - y.val();
        field_poly p = x.poly() - y.poly();
        assert_linear(x.poly(), p, y.poly());  // p = x - y ==> x = p + y
        return var_type{ std::move(v), std::move(p) };
    }

    virtual var_type eval(zkp_ops::mul, const var_type& x, const var_type& y) {
        field_type v = x.val() * y.val();
        auto p = encode(v);
        assert_quadratic(p, x.poly(), y.poly());  // p = x * y
        return var_type{ std::move(v), std::move(p) };
    }

    virtual var_type eval(zkp_ops::div, const var_type& x, const var_type& y) {
        auto v = x.val() / y.val();
        auto p = encode(v);
        assert_quadratic(x.poly(), p, y.poly());  // p = x / y ==> x = p * y
        return var_type{ std::move(v), std::move(p) };
    }

    virtual var_type eval(zkp_ops::index_of op, const var_type& x) {
        size_t index = op.index;
        auto v = x.val();
        auto bit = (v >> index) & static_cast<decltype(v)>(1);
        auto p = encode(bit);
        assert_quadratic(p, p, p);  // x^2 - x = 0
        return var_type { std::move(bit), std::move(p) };
    }
    
protected:
    std::vector<field_poly> zstack_;
    reed_solomon64& encoder_;
    // size_t fcounter = 0, scounter = 0;
};

template <typename Fp>
struct zkp_context
    : public zkp_context_base<Fp>,
      public extend_call_host<zkp_context<Fp>>
{
    using zkp_context_base<Fp>::zkp_context_base;
};

// Stage 1 (Merkle Tree)
/* ------------------------------------------------------------ */
template <typename Fp, typename Hasher = sha256>
struct stage1_prover_context : public zkp_context<Fp> {
    using field_poly = Fp;
    using field_type = typename Fp::field_type;
    using operator_type = standard_op;
    
    stage1_prover_context(reed_solomon64& encoder)
        // : lrs_(l), qrs_(q), builder_(l.encoded_size()) { }
        : zkp_context<Fp>(encoder), builder_(encoder.encoded_size()) { }

    void assert_linear(const field_poly& z, const field_poly& x, const field_poly& y) override {
        builder_ << x << y << z;
    }

    void assert_quadratic(const field_poly& z, const field_poly& x, const field_poly& y) {
        builder_ << x << y << z;
    }

    auto root_hash() {
        mkt_ = builder_;
        return mkt_.root();
    }
    
protected:
    // reed_solomon64& lrs_, qrs_;
    merkle_tree<Hasher> mkt_;
    typename merkle_tree<Hasher>::builder builder_;
};


// Stage 2 (code test, linear test, qudratic test)
/* ------------------------------------------------------------ */
template <typename Fp, typename RandomEngine = hash_random_engine<sha256>>
struct stage2_prover_context : public zkp_context<Fp> {
    using field_poly = Fp;
    using field_type = typename Fp::field_type;
    using operator_type = standard_op;
    
    stage2_prover_context(reed_solomon64& encoder, const typename RandomEngine::seed_type& seed)
        // : lrs_(l), qrs_(q), arg_(l.encoded_size(), seed) { }
        : zkp_context<Fp>(encoder), arg_(encoder.encoded_size(), seed) { }

    void assert_linear(const field_poly& z, const field_poly& x, const field_poly& y) override {
        arg_.update_code(x)
            .update_code(y)
            .update_code(z)
            .update_linear(x, y, z);
    }

    void assert_quadratic(const field_poly& z, const field_poly& x, const field_poly& y) override {
        arg_.update_code(x)
            .update_code(y)
            .update_code(z)
            .update_quadratic(x, y, z);
    }

    bool validate_linear() {
        auto row = arg_.linear();
        this->encoder_.decode(row);
        return std::all_of(row.begin(), row.end(), [](auto v) { return v == 0; });
    }

    bool validate_quadratic() {
        auto row = arg_.quadratic();
        this->encoder_.decode(row);
        return std::all_of(row.begin(), row.end(), [](auto v) { return v == 0; });
    }

    const auto& get_argument() const { return arg_; }
    
protected:
    quasi_argument<Fp, RandomEngine> arg_;
};


template <typename Fp>
struct stage3_prover_context : public zkp_context<Fp>
{
    using Base = zkp_context<Fp>;
    using field_poly = Fp;
    using field_type = typename Fp::field_type;
    using operator_type = standard_op;

    stage3_prover_context(reed_solomon64& encoder, const std::vector<size_t>& si)
        : Base(encoder), sample_index_(si) { }

    // void zstack_push(field_poly&& f) override {
    //     // poly_type p(1, f);
    //     // this->encoder_.encode(p);
        
    //     field_poly sp(sample_index_.size());
    //     #pragma omp parallel for
    //     for (size_t i = 0; i < sample_index_.size(); i++) {
    //         sp[i] = f[sample_index_[i]];
    //     }
        
    //     // this->zstack_.push_back(f);
    //     Base::zstack_push(std::move(f));
    //     samples_.push_back(std::move(sp));
    // }

    void bound_linear(size_t, size_t, size_t) {
        // Nothing to check
    }

    void bound_quadratic(size_t, size_t, size_t) {
        // Nothing to check
    }

    const auto& get_sample() const { return samples_; }

protected:
    std::vector<size_t> sample_index_;
    std::vector<field_poly> samples_;
};


template <typename Fp, typename RandomEngine = hash_random_engine<sha256>>
struct verifier_context : public stage2_prover_context<Fp> {
    using Base = stage2_prover_context<Fp, RandomEngine>;
    using field_poly = Fp;
    using field_type = typename Fp::field_type;
    using operator_type = standard_op;

    verifier_context(reed_solomon64& e,
                     const typename RandomEngine::seed_type& seed,
                     const std::vector<field_poly>& sv)
        : Base(e, seed), sampled_val_(sv)
        {
            std::reverse(sampled_val_.begin(), sampled_val_.end());
        }
    
    // void zstack_push(field_poly&& f) override {
    //     assert(!sampled_val_.empty());
    //     field_poly p = sampled_val_.back();
    //     sampled_val_.pop_back();
    //     this->zstack_.push_back(std::move(p));
    // }

protected:
    std::vector<field_poly> sampled_val_;
};

}  // namespace ligero::vm::zkp
