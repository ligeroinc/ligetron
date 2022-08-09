#pragma once

#include <runtime.hpp>
#include <instruction.hpp>
#include <stack>
#include <exception>
#include <tuple>
#include <limits>

namespace ligero::vm {

/// Helper function for std::visit
template <typename... Ts>
struct _overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> _overloaded(Ts...) -> _overloaded<Ts...>;  // deduction guide


struct execution_context {
    execution_context() = default;

    void stack_push(svalue_t&& val) {
        stack_.push_back(std::move(val));
    }

    template <typename... Args>
    void stack_emplace(Args&&... args) {
        stack_.emplace_back(std::forward<Args>(args)...);
    }

    template <typename T>
    T stack_peek() {
        const svalue_t& top = stack_.back();
        return std::get<T>(top);
    }

    template <typename T>
    T stack_pop() {
        svalue_t top = std::move(stack_.back());
        stack_.pop_back();
        return std::get<T>(top);
    }

    void show_stack() const {
        std::cout << "stack: ";
        for (const auto& v : stack_) {
            std::visit(_overloaded {
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

    void pop_frame() {
        frames_.pop_back();
    }
    
    const store_t *store_ = nullptr;
    module_instance *module_ = nullptr;
    std::vector<frame*> frames_;
    std::vector<svalue_t> stack_;
};

struct wasm_trap : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct basic_exe_control : virtual execution_context, virtual ControlExecutor {
    basic_exe_control() = default;

    result_t run(const op::nop&) override {
        return {};
    }
    
    result_t run(const op::unreachable&) override {
        throw wasm_trap("Unreachable");
        return {};
    }

    result_t run(const op::block& block) override {
        std::cout << "<block.entry> ";
        show_stack();
        /* Entering block with Label L */
        u32 m = 0, n = 0;
        if (block.type) {
            const auto& type = module_->types[*block.type];
            m = type.params.size();
            n = type.returns.size();
        }

        stack_.insert(stack_.rbegin().base() + m, label{ n });

        for (const auto& instr : block.body) {
            auto ret = instr->run(*this);

            /* Return from nested block */
            if (int *p = std::get_if<int>(&ret)) {
                assert(*p >= 0);
                if (*p == 0) {
                    return {};
                }
                else {
                    return *p - 1;
                }
            }
        }

        show_stack();

        /* Exiting block with Label L if no jump happended */
        auto it = (stack_.rbegin() + n + 1).base();
        assert(std::holds_alternative<label>(*it));
        stack_.erase(it);

        std::cout << "<block.exit> ";
        show_stack();
        return {};
    }

    result_t run(const op::loop& block) override {
        std::cout << "<loop.entry> ";
        show_stack();
        /* Entering block with Label L */
        u32 m = 0, n = 0;
        if (block.type) {
            const auto& type = module_->types[*block.type];
            m = type.params.size();
            n = type.returns.size();
        }

        stack_.insert(stack_.rbegin().base() + m, label{ m });

        for (const auto& instr : block.body) {
            auto ret = instr->run(*this);

            /* Return from nested block */
            if (int *p = std::get_if<int>(&ret)) {
                assert(*p >= 0);
                if (*p == 0) {
                    return run(block);
                }
                else {
                    return *p - 1;
                }
            }
        }

        /* Exiting block with Label L if no jump happended */
        auto it = (stack_.rbegin() + n + 1).base();
        assert(std::holds_alternative<label>(*it));
        stack_.erase(it);

        std::cout << "<loop.exit> ";
        show_stack();
        return {};
    }

    result_t run(const op::if_then_else&) override {
        throw wasm_trap("Impossible instruction if_then_else");
        return {};
    }

    result_t run(const op::br& b) override {
        const int l = b.label;

        std::cout << "<br " << l << "> ";
        show_stack();

        /* Find L-th label */
        int count = 0;
        auto rit = std::find_if(stack_.rbegin(), stack_.rend(), [&count, l](const auto& v) {
            if (std::holds_alternative<label>(v)) {
                count++;
                if (count == l + 1) {
                    return true;
                }
            }
            return false;
        });

        rit++;
        auto it = rit.base();
        
        assert(std::holds_alternative<label>(*it));
        const size_t n = std::get<label>(*it).arity;
        auto top = stack_.rbegin() + n;
        stack_.erase(it, top.base());
        return l;
    }

    result_t run(const op::br_if& b) override {
        u32 cond = stack_pop<u32>();
        std::cout << "<br_if> cond=" << cond << std::endl;

        if (cond != 0) {
            return run(op::br{ b.label });
        }
        return {};
    }

    result_t run(const op::br_table&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::ret&) override {
        const size_t arity = current_frame()->arity;
        auto top = stack_.rbegin() + arity;
        auto it = top;

        while (!std::holds_alternative<frame_ptr>(*it)) {
            it++;
        }

        stack_.erase(it.base(), top.base());

        return std::numeric_limits<int>::max();
    }

    result_t run(const op::call& c) override {
        std::cout << "<call.entry> ";
        show_stack();
        
        address_t addr = current_frame()->module->funcaddrs[c.func];
        const function_instance& func = store_->functions[addr];
        u32 n = func.kind.params.size();
        u32 m = func.kind.returns.size();

        auto it = (stack_.rbegin() + n).base();
        std::vector<value_t> arguments;
        for (auto i = it; i != stack_.end(); i++) {
            if (std::holds_alternative<u32>(*i)) {
                arguments.emplace_back(std::get<u32>(*i));
            }
            else if (std::holds_alternative<u64>(*i)) {
                arguments.emplace_back(std::get<u64>(*i));
            }
            else {
                undefined("Invalid stack value");
            }
        }

        stack_.erase((stack_.rbegin() + n).base(), stack_.end());
        
        auto fp = std::make_unique<frame>(m, std::move(arguments), module_);

        for (const value_kind& type : func.code.locals) {
            fp->locals.emplace_back(static_cast<u64>(0));
        }
        
        push_frame(std::move(fp));

        for (const auto& instr : func.code.body) {
            auto ret = instr->run(*this);
            
            if (int *p = std::get_if<int>(&ret)) {
                break;
            }
        }

        it = (stack_.rbegin() + m + 1).base();
        assert(std::holds_alternative<frame_ptr>(*it));
        stack_.erase(it);
        pop_frame();

        std::cout << "<call.exit> ";
        show_stack();
        return {};
    }

    result_t run(const op::call_indirect&) override {
        undefined("undefined");
        return {};
    }
};


struct basic_exe_variable : virtual execution_context, virtual VariableExecutor {
    basic_exe_variable() = default;

    result_t run(const op::local_get& var) override {
        index_t x = var.local;
        auto local = local_get(x);
        if (std::holds_alternative<u32>(local)) {
            stack_emplace(std::get<u32>(local));
            std::cout << "local.get[" << x << "]=" << std::get<u32>(local) << " ";
        }
        else {
            stack_emplace(std::get<u64>(local));
        }
        show_stack();
        return {};
    }

    result_t run(const op::local_set& var) override {
        index_t x = var.local;
        u32 top = stack_pop<u32>();
        std::cout << "local.set[" << x << "]=" << top << " ";
        local_set(x, top);
        show_stack();
        return {};
    }

    result_t run(const op::local_tee& var) override {
        index_t x = var.local;
        u32 top = stack_peek<u32>();
        std::cout << "local.tee[" << x << "]=" << top << " ";
        local_set(x, top);
        show_stack();
        return {};
    }

    result_t run(const op::global_get&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::global_set&) override {
        undefined("Undefined");
        return {};
    }
};


struct basic_exe_numeric : virtual execution_context, virtual NumericExecutor {
    basic_exe_numeric() = default;

    result_t run(const op::inn_const& i) override {
        if (i.type == int_kind::i32) {
            stack_push(static_cast<u32>(i.val));
        }
        else {
            stack_push(i.val);
        }
        return {};
    }

    result_t run(const op::inn_clz&) override {
        undefined("Undefined");
        return {};        
    }

    result_t run(const op::inn_ctz&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_popcnt&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_add& ins) override {
        if (ins.type == int_kind::i32) {
            u32 x = stack_pop<u32>();
            u32 y = stack_pop<u32>();
            stack_push(x + y);
        }
        else {
            u32 x = stack_pop<u64>();
            u32 y = stack_pop<u64>();
            stack_push(x + y);
        }
        std::cout << "i32.add ";
        show_stack();
        return {};
    }

    result_t run(const op::inn_sub&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_mul&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_div_sx&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_rem_sx&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_and&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_or&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_xor&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_shl&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_shr_sx&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_rotl&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_rotr&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_eqz&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_eq&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_ne& ins) override {
        assert(ins.type == int_kind::i32);
        u32 y = stack_pop<u32>();
        u32 x = stack_pop<u32>();
        stack_push(static_cast<u32>(x != y));
        return {};
    }

    result_t run(const op::inn_lt_sx& ins) override {
        assert(ins.type == int_kind::i32);
        u32 y = stack_pop<u32>();
        u32 x = stack_pop<u32>();
        if (ins.sign == sign_kind::sign) {
            u32 compare = static_cast<s32>(x) < static_cast<s32>(y);
            stack_emplace(compare);
        }
        else {
            stack_emplace(static_cast<u32>(x < y));
        }
        return {};
    }

    result_t run(const op::inn_gt_sx& ins) override {
        assert(ins.type == int_kind::i32);
        u32 y = stack_pop<u32>();
        u32 x = stack_pop<u32>();
        if (ins.sign == sign_kind::sign) {
            u32 compare = static_cast<s32>(x) > static_cast<s32>(y);
            stack_emplace(compare);
        }
        else {
            stack_emplace(static_cast<u32>(x > y));
        }
        return {};
    }

    result_t run(const op::inn_le_sx&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_ge_sx&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_extend8_s&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::inn_extend16_s&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::i64_extend32_s&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::i64_extend_i32_sx&) override {
        undefined("Undefined");
        return {};
    }

    result_t run(const op::i32_wrap_i64&) override {
        undefined("Undefined");
        return {};
    }
};

struct basic_executor :
        public basic_exe_control,
        public basic_exe_variable,
        public basic_exe_numeric
{
    using basic_exe_control::run;
    using basic_exe_variable::run;
    using basic_exe_numeric::run;
};

}  // namespace ligero::vm
