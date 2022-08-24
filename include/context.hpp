#pragma once

#include <runtime.hpp>
#include <prelude.hpp>
#include <functional>

namespace ligero::vm {

struct standard_op;

template <typename Op = standard_op>
struct standard_context {
    using operator_type = Op;
    
    standard_context() = default;

    void stack_push(svalue_t&& val) {
        stack_.push_back(std::move(val));
    }

    template <typename T>
    T stack_peek() const {
        return std::get<T>(stack_.back());
    }

    template <typename T>
    T stack_pop() {
        svalue_t top = std::move(stack_.back());
        stack_.pop_back();
        return std::get<T>(top);
    }

    svalue_t stack_pop_raw() {
        svalue_t top = std::move(stack_.back());
        stack_.pop_back();
        return top;
    }

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
        stack_push(std::move(ptr));
        return p;
    }

    void pop_frame(u32 rets) {
        auto it = (stack_.rbegin() + rets + 1).base();
        assert(std::holds_alternative<frame_ptr>(*it));
        stack_.erase(it);
        frames_.pop_back();
    }

    store_t* store() { return store_; }
    void store(store_t *s) { store_ = s; }

    module_instance* module() { return module_; }
    void module(module_instance *mod) { module_ = mod; }

    const auto& args() { return argv_; }
    void set_args(const std::vector<uint8_t>& data) { argv_ = data; }

    void block_entry(u32 param, u32 ret) {
        stack_.insert((stack_.rbegin() + param).base(), label{ ret });
    }

    void block_exit(u32 ret) {
        auto it = (stack_.rbegin() + ret + 1).base();
        assert(std::holds_alternative<label>(*it));
        stack_.erase(it);
    }

    auto stack_bottom() { return stack_.rend(); }
    auto stack_top() { return stack_.rbegin(); }

    template <typename Kind>
    auto find_nth(size_t n) {
        auto it = stack_.rbegin();
        for (int i = 0; i < n; i++) {
            it = std::find_if(it, stack_.rend(), [](const auto& v) {
                return std::holds_alternative<Kind>(v);
            });
            if (it == stack_.rend())
                throw wasm_trap("Cannot find nth value on stack");
        }
        return it;
    }

    template <typename RIter>
    void stack_pops(RIter from, RIter to) {
        stack_.erase((++from).base(), to.base());
    }

    // const auto& args() { return argv_; }
    // void args(const std::vector<uint8_t>& data) { argv_ = data; }

    result_t call_host(const import_name_t& name, const std::vector<value_t>& args) {
        if (name.second == "get_witness_size") {
            return builtin_get_witness_size();
        }
        else if (name.second == "get_witness") {
            show_stack();
            auto a = builtin_get_witness(args);
            show_stack();
            return a;
        }
        else {
            undefined("Undefined host function");
            return {};
        }
    }

private:
    result_t builtin_get_witness_size() {
        u32 size = argv_.size() / sizeof(u32);
        stack_push(size);
        return {};
    }

    result_t builtin_get_witness(const std::vector<value_t>& args) {
        assert(args.size() == 1);
        u32 index = std::get<u32>(args[0]);
        u32 val = reinterpret_cast<const u32*>(argv_.data())[index];
        stack_push(val);
        return {};
    }

/* ------------------------------------------------------------ */
protected:    
    store_t *store_ = nullptr;
    module_instance *module_ = nullptr;
    std::vector<frame*> frames_;
    std::vector<svalue_t> stack_;

    std::vector<uint8_t> argv_;
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

    // template <typename T>
    // struct plus {
    //     T operator()(const T& a, const T& b) const { return std::plus<T>{}(a, b); }
    // };

    // template <typename T>
    // struct minus {
    //     T operator()(const T& a, const T& b) const { return std::minus<T>{}(a, b); }
    // };

    // template <typename T>
    // struct multiplies {
    //     T operator()(const T& a, const T& b) const { return std::multiplies<T>{}(a, b); }
    // };

    // template <typename T>
    // struct divides {
    //     T operator()(const T& a, const T& b) const { return std::divides<T>{}(a, b); }
    // };

    // template <typename T>
    // struct modulus {
    //     T operator()(const T& a, const T& b) const { return std::modulus<T>{}(a, b); }
    // };

    // template <typename T>
    // struct bit_and {
    //     T operator()(const T& a, const T& b) const { return std::bit_and<T>{}(a, b); }
    // };

    // template <typename T>
    // struct bit_or {
    //     T operator()(const T& a, const T& b) const { return std::bit_or<T>{}(a, b); }
    // };

    // template <typename T>
    // struct bit_xor {
    //     T operator()(const T& a, const T& b) const { return std::bit_xor<T>{}(a, b); }
    // };

    // template <typename T>
    // struct shiftl {
    //     T operator()(const T& a, const T& b) const { return prelude::shiftl<T>{}(a, b); }
    // };

    // template <typename T>
    // struct shiftr {
    //     T operator()(const T& a, const T& b) const { return prelude::shiftr<T>{}(a, b); }
    // };

    // template <typename T>
    // struct rotatel {
    //     T operator()(const T& a, const T& b) const { return prelude::rotatel<T>{}(a, b); }
    // };

    // template <typename T>
    // struct equal_to {
    //     T operator()(const T& a, const T& b) const { return std::equal_to<T>{}(a, b); }
    // };

    // template <typename T>
    // struct not_equal_to {
    //     T operator()(const T& a, const T& b) const { return std::not_equal_to<T>{}(a, b); }
    // };

    // template <typename T>
    // struct less {
    //     T operator()(const T& a, const T& b) const { return std::less<T>{}(a, b); }
    // };

    // template <typename T>
    // struct greater {
    //     T operator()(const T& a, const T& b) const { return std::greater<T>{}(a, b); }
    // };

    // template <typename T>
    // struct less_equal {
    //     T operator()(const T& a, const T& b) const { return std::less_equal<T>{}(a, b); }
    // };

    // template <typename T>
    // struct greater_equal {
    //     T operator()(const T& a, const T& b) const { return std::greater_equal<T>{}(a, b); }
    // };
};

}  // namespace ligero::vm
