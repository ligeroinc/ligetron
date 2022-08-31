#pragma once

#include <context.hpp>
#include <zkp/encoding.hpp>
#include <zkp/merkle_tree.hpp>

namespace ligero::vm::zkp {

template <typename T>
struct shared_tag {
    shared_tag(T&& f) : val(std::forward<T>(f)) { }
    shared_tag(T&& f, bool shared) : val(std::forward<T>(f)), secret_shared(shared) { }

    T& operator*() { return val; }
    const T& operator*() const { return val; }
    
    operator bool() const { return secret_shared; }
    operator T() const { return val; }

    T val;
    bool secret_shared = false;
};

template <typename Poly>
struct zkp_context_base : public context_base {
    using Base = context_base;
    using poly_type = Poly;
    using tagged_poly = shared_tag<poly_type>;
    using field_type = typename Poly::field_type;
    using operator_type = standard_op;

    zkp_context_base() = default;

    void stack_push(svalue_t&& val) override {
        std::visit(prelude::overloaded{
                [this](u32 v) { zstack_push(v); },
                [this](u64 v) { zstack_push(v); },
                [](const auto&) { }
            }, val);
        Base::stack_push(std::move(val));
    }

    svalue_t stack_pop_raw() override {
        auto&& val = Base::stack_pop_raw();
        std::visit(prelude::overloaded{
                [this](u32) { zstack_pop(); },
                [this](u64) { zstack_pop(); },
                [](const auto&) { }
            }, val);
        return std::move(val);
    }

    template <typename T>
    T stack_pop() {
        return std::get<T>(stack_pop_raw());
    }

    const auto& zstack() const { return zstack_; }
    auto& zstack() { return zstack_; }

    void vstack_push(svalue_t&& sv) {
        Base::stack_push(std::move(sv));
    }
    
    template <typename T>
    T vstack_pop() {
        return std::get<T>(Base::stack_pop_raw());
    }

    void zstack_push(const field_type& f) {
        zstack_.push_back(tagged_poly{ poly_type(1, f) });
    }

    void zstack_push(tagged_poly&& tp) {
        zstack_.push_back(std::move(tp));
    }

    tagged_poly zstack_pop() {
        auto x = zstack_.back();
        zstack_.pop_back();
        return x;
    }
    
protected:
    std::vector<tagged_poly> zstack_;
};

template <typename Field>
struct zkp_context
    : public zkp_context_base<Field>,
      public extend_call_host<zkp_context<Field>>
{ };

// Stage 1 (Merkle Tree)
/* ------------------------------------------------------------ */
template <typename Hasher, typename Field>
struct stage1_prover_context : public zkp_context<Field> {
    using field_type = typename Field::field_type;
    using operator_type = standard_op;
    
    stage1_prover_context(reed_solomon64& l, reed_solomon64& q)
        : lrs_(l), qrs_(q), builder_(l.encoded_size()) { }

    void bound_linear(size_t a, size_t b, size_t c) {
        auto& x = *(this->zstack().rbegin() + a);
        auto& y = *(this->zstack().rbegin() + b);
        auto& z = *(this->zstack().rbegin() + c);
        if (!x) lrs_.encode(*x);
        if (!y) lrs_.encode(*y);
        if (!z) lrs_.encode(*z);
        builder_ << *x << *y << *z;
    }

    void bound_quadratic(size_t a, size_t b, size_t c) {
        auto& x = *(this->zstack().rbegin() + a);
        auto& y = *(this->zstack().rbegin() + b);
        auto& z = *(this->zstack().rbegin() + c);
        if (!x) qrs_.encode(*x);
        if (!y) qrs_.encode(*y);
        if (!z) qrs_.encode(*z);
        builder_ << *x << *y << *z;
    }
    
protected:
    reed_solomon64& lrs_, qrs_;
    merkle_tree<Hasher> mkt_;
    typename merkle_tree<Hasher>::builder builder_;
};

}  // namespace ligero::vm::zkp
