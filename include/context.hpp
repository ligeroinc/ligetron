#pragma once

#include <runtime.hpp>
#include <prelude.hpp>
// #include <call_host.hpp>

#include <functional>

namespace ligero::vm {

struct standard_op;

template <typename LocalValue, typename StackValue>
struct context_base {
    using value_type = LocalValue;
    using svalue_type = StackValue;
    
    using u32_type = typename svalue_type::u32_type;
    using s32_type = typename svalue_type::s32_type;
    using u64_type = typename svalue_type::u64_type;
    using s64_type = typename svalue_type::s64_type;
    
    using label_type = typename svalue_type::label_type;
    using frame_type = typename svalue_type::frame_type;
    using frame_pointer = typename svalue_type::frame_pointer;
    using frame_ptr = typename svalue_type::frame_ptr;
    
    context_base() = default;

    virtual void stack_push(svalue_type&& val) {
        stack_.push_back(std::move(val));
    }

    virtual svalue_type stack_pop() {
        assert(!stack_.empty());
        svalue_type top = std::move(stack_.back());
        stack_.pop_back();
        return top;
    }

    virtual svalue_type& stack_peek() {
        return stack_.back();
    }

    virtual void push_witness(const u32_type& v) {
        stack_push(v);
    }

    virtual void drop_n_below(size_t n, size_t pos = 0) {
        auto it = stack_.rbegin() + pos;
        auto begin = (it + n).base();
        auto end = it.base();

        for (auto iter = begin; iter != end; ++iter) {
            if (std::holds_alternative<frame_ptr>(iter->data())) {
                std::cout << "pop current frame" << std::endl;
                frames_.pop_back();
            }
        }
        
        stack_.erase((it + n).base(), it.base());
    }

    virtual void push_arguments(frame_pointer fp, u32 n) {
        std::vector<value_type> arguments;
        for (size_t i = 0; i < n; i++) {
            svalue_type sv = stack_pop();
            arguments.push_back(std::move(sv.template as<value_type>()));
        }
        std::reverse(arguments.begin(), arguments.end());
        fp->locals = std::move(arguments);
    }

    virtual void push_local(frame_pointer fp, value_kind k) {
        switch (k) {
        case value_kind::i32:
            fp->locals.emplace_back(u32_type{0U});
            break;
        case value_kind::i64:
            fp->locals.emplace_back(u64_type{0U});
            break;
        default:
            undefined();
        }
    }

    // virtual value_type local_get(index_t i) const {
    //     return current_frame()->locals[i];
    // }

    // virtual void local_set(index_t i, value_type v) {
    //     current_frame()->locals[i] = std::move(v);
    // }

    // virtual void local_tee(index_t i) {
    //     local_set(i, stack_peek().template as<value_type>());
    // }

    frame_pointer current_frame() const {
        return frames_.back();
    }

    frame_pointer push_frame(frame_ptr&& ptr) {
        frame_pointer p = ptr.get();
        frames_.push_back(p);
        stack_.emplace_back(std::move(ptr));
        // stack_push(std::move(ptr));
        return p;
    }

    // void pop_frame() { frames_.pop_back(); }

    void block_entry(u32 param, u32 ret) {
        stack_.insert((stack_.rbegin() + param).base(), label_type{ ret });
    }

    store_t* store() { return store_; }
    void store(store_t *s) { store_ = s; }

    module_instance* module() { return module_; }
    void module(module_instance *mod) { module_ = mod; }

    const auto& args() { return argv_; }
    void set_args(const std::vector<uint8_t>& data) { argv_ = data; }

    auto stack_bottom() { return stack_.rend(); }
    auto stack_top() { return stack_.rbegin(); }

    void show_stack() const {
        std::cout << "stack: ";
        for (const auto& v : stack_) {
            std::cout << v.to_string() << " ";
            // std::visit(prelude::overloaded {
            //         [](u32 x) { std::cout << "(" << x << " : i32) "; },
            //         [](u64 x) { std::cout << "(" << x << " : i64) "; },
            //         [](label l) { std::cout << "Label<" << l.arity << "> "; },
            //         [](const frame_ptr& f) { std::cout << "Frame<" << f->arity << "> "; }
            //     }, v);
        }
        std::cout << std::endl;
    }

/* ------------------------------------------------------------ */
protected:    
    store_t *store_ = nullptr;
    module_instance *module_ = nullptr;
    std::vector<frame_pointer> frames_;
    std::vector<svalue_type> stack_;

    std::vector<uint8_t> argv_;
};


template <typename LocalValue, typename StackValue, typename Op = standard_op>
struct standard_context : public context_base<LocalValue, StackValue>
{
    using operator_type = Op;
    // using Base = context_base<LocalValue, StackValue>;
    // using Base::u32_type, Base::s32_type, Base::u64_type, Base::s64_type;
    // using Base::label_type, Base::frame_type, Base::frame_ptr, Base::frame_pointer;
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
