#pragma once

#include <runtime.hpp>
#include <prelude.hpp>
#include <call_host.hpp>

#include <functional>

namespace ligero::vm {

struct standard_op;

struct context_base {
    context_base() = default;

    virtual void stack_push(svalue_t&& val) {
        stack_.push_back(std::move(val));
    }

    virtual svalue_t stack_pop_raw() {
        svalue_t top = std::move(stack_.back());
        stack_.pop_back();
        return top;
    }

    template <typename T>
    const T& stack_peek() const {
        return std::get<T>(stack_.back());
    }

    template <typename T>
    T stack_pop() {
        auto tmp = stack_pop_raw();
        return std::get<T>(std::move(tmp));
    }
    // template <typename T>
    // T stack_pop() {
        // svalue_t top = std::move(stack_.back());
        // stack_.pop_back();
        // return std::get<T>(std::move(top));
    // }

    void show_stack() const {
        std::cout << "stack: ";
        for (const auto& v : stack_) {
            std::visit(prelude::overloaded {
                    [](u32 x) { std::cout << "(" << x << " : i32) "; },
                    [](u64 x) { std::cout << "(" << x << " : i64) "; },
                    [](label l) { std::cout << "Label<" << l.arity << "> "; },
                    [](const frame_ptr& f) { std::cout << "Frame<" << f->arity << "> "; }
                }, v);
        }
        std::cout << std::endl;
    }

    value_t local_get(index_t i) const {
        return current_frame()->locals[i];
    }

    template <typename T>
    void local_set(index_t i, T v) {
        current_frame()->locals[i] = v;
    }

    frame* current_frame() const {
        return frames_.back();
    }

    frame* push_frame(frame_ptr&& ptr) {
        frame *p = ptr.get();
        frames_.push_back(p);
        stack_.emplace_back(std::move(ptr));
        // stack_push(std::move(ptr));
        return p;
    }

    void block_entry(u32 param, u32 ret) {
        stack_.insert((stack_.rbegin() + param).base(), label{ ret });
    }

    store_t* store() { return store_; }
    void store(store_t *s) { store_ = s; }

    module_instance* module() { return module_; }
    void module(module_instance *mod) { module_ = mod; }

    const auto& args() { return argv_; }
    void set_args(const std::vector<uint8_t>& data) { argv_ = data; }

    auto stack_bottom() { return stack_.rend(); }
    auto stack_top() { return stack_.rbegin(); }

/* ------------------------------------------------------------ */
protected:    
    store_t *store_ = nullptr;
    module_instance *module_ = nullptr;
    std::vector<frame*> frames_;
    std::vector<svalue_t> stack_;

    std::vector<uint8_t> argv_;
};


template <typename Op = standard_op>
struct standard_context
    : public context_base
    , public extend_call_host<standard_context<Op>>
{
    using operator_type = Op;
};


struct standard_op {
    template <typename T = void>
    struct countl_zero {
        T operator()(const T& a) const { return std::countl_zero(a); }
    };    

    template <typename T = void>
    struct countr_zero {
        T operator()(const T& a) const { return std::countr_zero(a); }
    };

    template <typename T = void>
    struct popcount {
        T operator()(const T& a) const { return std::popcount(a); }
    };

    using plus = std::plus<>;
    using minus = std::minus<>;
    using multiplies = std::multiplies<>;
    using divides = std::divides<>;
    using modulus = std::modulus<>;
    using bit_and = std::bit_and<>;
    using bit_or = std::bit_or<>;
    using bit_xor = std::bit_xor<>;
    using shiftl = prelude::shiftl<>;
    using shiftr = prelude::shiftr<>;
    using rotatel = prelude::rotatel<>;
    using rotater = prelude::rotater<>;
    using equal_to = std::equal_to<>;
    using not_equal_to = std::not_equal_to<>;
    using less = std::less<>;
    using greater = std::greater<>;
    using less_equal = std::less_equal<>;
    using greater_equal = std::greater_equal<>;
};

}  // namespace ligero::vm
