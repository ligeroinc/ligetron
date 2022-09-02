#pragma once

#include <context.hpp>
#include <zkp/encoding.hpp>
#include <zkp/merkle_tree.hpp>
#include <zkp/argument.hpp>

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

template <typename Poly>
struct zkp_context_base : public context_base {
    using Base = context_base;
    using poly_type = Poly;
    // using tagged_poly = shared_tag<poly_type>;
    using field_type = typename Poly::field_type;
    using operator_type = standard_op;

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

    void show_stack() {
        Base::show_stack();
        // std::cout << "zstack: ";
        // for (auto& v : zstack_) {
        //     lrs_.decode(v);
        //     std::cout << v[0] << " ";
        //     // std::visit(prelude::overloaded {
        //     //         [](u32 x) { std::cout << "(" << x << " : i32) "; },
        //     //         [](u64 x) { std::cout << "(" << x << " : i64) "; },
        //     //         [](label l) { std::cout << "Label<" << l.arity << "> "; },
        //     //         [](const frame_ptr& f) { std::cout << "Frame<" << f->arity << "> "; }
        //     //     }, v);
        //     lrs_.encode(v);
        // }
        // std::cout << std::endl;
    }

    // const auto& zstack() const { return zstack_; }
    // auto& zstack() { return zstack_; }
    auto& zstack(size_t n) { return *(zstack_.rbegin() + n); }

    void vstack_push(svalue_t&& sv) {
        Base::stack_push(std::move(sv));
    }
    
    template <typename T>
    T vstack_pop() {
        return std::get<T>(Base::stack_pop_raw());
    }

    void zstack_push(const field_type& f) {
        poly_type p(1, f);
        encoder_.encode(p);
        zstack_.push_back(std::move(p));
    }

    void zstack_push(poly_type&& tp) {
        zstack_.push_back(std::move(tp));
    }

    template <typename... Args>
    void zstack_pushs(Args&&... args) {
        ((zstack_push(std::forward<Args>(args))), ...);
    }

    poly_type zstack_pop() {
        auto x = zstack_.back();
        zstack_.pop_back();
        return x;
    }

    void zstack_drop(size_t count) {
        zstack_.erase((zstack_.rbegin() + count).base(), zstack_.end());
    }
    
protected:
    std::vector<poly_type> zstack_;
    reed_solomon64& encoder_;
};

template <typename Field>
struct zkp_context
    : public zkp_context_base<Field>,
      public extend_call_host<zkp_context<Field>>
{
    using zkp_context_base<Field>::zkp_context_base;
};

// Stage 1 (Merkle Tree)
/* ------------------------------------------------------------ */
template <typename Field, typename Hasher = sha256>
struct stage1_prover_context : public zkp_context<Field> {
    using field_type = typename Field::field_type;
    using operator_type = standard_op;
    
    stage1_prover_context(reed_solomon64& encoder)
        // : lrs_(l), qrs_(q), builder_(l.encoded_size()) { }
        : zkp_context<Field>(encoder), builder_(encoder.encoded_size()) { }

    void bound_linear(size_t a, size_t b, size_t c) {
        auto& x = this->zstack(a);
        auto& y = this->zstack(b);
        auto& z = this->zstack(c);
        // if (!x) {
        //     lrs_.encode(*x);
        //     x.secret_shared = true;
        // }
        // if (!y) {
        //     lrs_.encode(*y);
        //     y.secret_shared = true;
        // }
        // if (!z) {
        //     lrs_.encode(*z);
        //     z.secret_shared = true;
        // }
        builder_ << x << y << z;
    }

    void bound_boolean(size_t a) {
        auto& x = this->zstack(a);
        builder_ << x << x << x;
    }

    void bound_quadratic(size_t a, size_t b, size_t c) {
        auto& x = this->zstack(a);
        auto& y = this->zstack(b);
        auto& z = this->zstack(c);
        // if (!x) {
        //     qrs_.encode(*x);
        //     x.secret_shared = true;
        // }
        // if (!y) {
        //     qrs_.encode(*y);
        //     y.secret_shared = true;
        // }
        // if (!z) {
        //     qrs_.encode(*z);
        //     z.secret_shared = true;
        // }
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
template <typename Field, typename RandomEngine = hash_random_engine<sha256>>
struct stage2_prover_context : public zkp_context<Field> {
    using field_type = typename Field::field_type;
    using operator_type = standard_op;
    
    stage2_prover_context(reed_solomon64& encoder, const typename RandomEngine::seed_type& seed)
        // : lrs_(l), qrs_(q), arg_(l.encoded_size(), seed) { }
        : zkp_context<Field>(encoder), arg_(encoder.encoded_size(), seed) { }

    void bound_linear(size_t a, size_t b, size_t c) {
        auto& x = this->zstack(a);
        auto& y = this->zstack(b);
        auto& z = this->zstack(c);
        // if (!x) {
        //     lrs_.encode(*x);
        //     x.secret_shared = true;
        // }
        // if (!y) {
        //     lrs_.encode(*y);
        //     y.secret_shared = true;
        // }
        // if (!z) {
        //     lrs_.encode(*z);
        //     z.secret_shared = true;
        // }        
        
        arg_.update_code(x)
            .update_code(y)
            .update_code(z)
            .update_linear(x, y, z);
    }

    void bound_boolean(size_t a) {
        auto& x = this->zstack(a);

        arg_.update_code(x)
            .update_quadratic(x, x, x);
    }

    void bound_quadratic(size_t a, size_t b, size_t c) {
        auto& x = this->zstack(a);
        auto& y = this->zstack(b);
        auto& z = this->zstack(c);
        // if (!x) {
        //     qrs_.encode(*x);
        //     x.secret_shared = true;
        // }
        // if (!y) {
        //     qrs_.encode(*y);
        //     y.secret_shared = true;
        // }
        // if (!z) {
        //     qrs_.encode(*z);
        //     z.secret_shared = true;
        // }

        // std::cout << "encoded x: ";
        // for (auto i = 0; i < 64; i++) {
        //     std::cout << (*x)[i] << " ";
        // }
        // std::cout << std::endl;

        // this->lrs_.decode_doubled(x);
        // std::cout << "x: ";
        // for (auto i = 0; i < 1; i++) {
        //     std::cout << x[i] << " ";
        // }
        // std::cout << std::endl;

        // this->lrs_.decode_doubled(y);
        // std::cout << "y: ";
        // for (auto i = 0; i < 1; i++) {
        //     std::cout << y[i] << " ";
        // }
        // std::cout << std::endl;

        // this->lrs_.decode_doubled(z);
        // std::cout << "z: ";
        // for (auto i = 0; i < 1; i++) {
        //     std::cout << z[i] << " ";
        // }
        // std::cout << std::endl;

        // this->lrs_.encode(x);
        // this->lrs_.encode(y);
        // this->lrs_.encode(z);
        
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
    
protected:
    quasi_argument<Field, RandomEngine> arg_;
};

}  // namespace ligero::vm::zkp
