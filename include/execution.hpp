#pragma once

#include <runtime.hpp>
#include <instruction.hpp>
#include <prelude.hpp>
#include <stack>
#include <exception>
#include <tuple>
#include <limits>
#include <bit>

namespace ligero::vm {

struct execution_context {
    execution_context() = default;

    void stack_push(svalue_t&& val) {
        stack_.push_back(std::move(val));
    }

    template <typename... Args>
    void stack_emplace(Args&&... args) {
        stack_.emplace_back(std::forward<Args>(args)...);
    }

    svalue_t& stack_top() { return stack_.back(); }
    const svalue_t& stack_top() const { return stack_.back(); }

    template <typename T>
    T stack_peek() const {
        return std::get<T>(stack_top());
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

    void pop_frame() {
        frames_.pop_back();
    }

    store_t& store() { return *store_; }
    void store(store_t *s) { store_ = s; }

    const module_instance& module() const { return *module_; }
    void module(module_instance *m) { module_ = m; }

    const auto& args() { return argv_; }
    void args(const std::vector<uint8_t>& data) { argv_ = data; }

    result_t call_host(const import_name_t& name, const std::vector<value_t>& args) {
        if (name.second == "get_witness_size") {
            return builtin_get_witness_size();
        }
        else if (name.second == "get_witness") {
            return builtin_get_witness(args);
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
        // std::cout << "<block.entry> ";
        // show_stack();
        
        /* Entering block with Label L */
        u32 m = 0, n = 0;
        if (block.type) {
            const auto& type = module_->types[*block.type];
            m = type.params.size();
            n = type.returns.size();
        }

        stack_.insert((stack_.rbegin() + m).base(), label{ n });

        for (const instr_ptr& instr : block.body) {
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

        /* Exiting block with Label L if no jump happended */
        auto it = (stack_.rbegin() + n + 1).base();
        assert(std::holds_alternative<label>(*it));
        stack_.erase(it);

        // std::cout << "<block.exit> ";
        // show_stack();
        return {};
    }

    result_t run(const op::loop& block) override {
        // std::cout << "<loop.entry> ";
        // show_stack();
    loop:
        /* Entering block with Label L */
        u32 m = 0, n = 0;
        if (block.type) {
            const auto& type = module_->types[*block.type];
            m = type.params.size();
            n = type.returns.size();
        }

        stack_.insert((stack_.rbegin() + m).base(), label{ n });

        for (const auto& instr : block.body) {
            auto ret = instr->run(*this);

            /* Return from nested block */
            if (int *p = std::get_if<int>(&ret)) {
                assert(*p >= 0);
                if (*p == 0) {
                    goto loop;
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

        // std::cout << "<loop.exit> ";
        // show_stack();
        return {};
    }

    result_t run(const op::if_then_else&) override {
        throw wasm_trap("Impossible instruction if_then_else");
        return {};
    }

    result_t run(const op::br& b) override {
        const int l = b.label;

        /* Find L-th label */
        int count = -1;
        auto rit = std::find_if(stack_.rbegin(), stack_.rend(), [&count, l](const auto& v) {
            if (std::holds_alternative<label>(v)) {
                count++;
                if (count == l) {
                    return true;
                }
            }
            return false;
        });

        auto it = (++rit).base();
        
        assert(std::holds_alternative<label>(*it));
        const size_t n = std::get<label>(*it).arity;
        auto top = stack_.rbegin() + n;
        stack_.erase(it, top.base());

        std::cout << "<br " << l << "> ";
        show_stack();
        return l;
    }

    result_t run(const op::br_if& b) override {
        u32 cond = stack_pop<u32>();
        std::cout << "<br_if> cond=" << cond << " ";

        if (cond) {
            return run(op::br{ b.label });
        }
        return {};
    }

    // result_t run(const op::br_table&) override {
    //     undefined("Undefined");
    //     return {};
    // }

    result_t run(const op::ret&) override {
        const size_t arity = current_frame()->arity;
        auto top = stack_.rbegin() + arity;
        auto it = top;

        while (!std::holds_alternative<frame_ptr>(*it)) {
            it++;
        }

        stack_.erase((++it).base(), top.base());

        return std::numeric_limits<int>::max();
    }

    result_t run(const op::call& c) override {        
        address_t addr = current_frame()->module->funcaddrs[c.func];
        const function_instance& func = store_->functions[addr];
        u32 n = func.kind.params.size();
        u32 m = func.kind.returns.size();

        std::cout << "<call[" << addr << "].entry>";
        show_stack();

        /* Push arguments */
        /* -------------------------------------------------- */
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

        /* Invoke the function (native function) */
        /* -------------------------------------------------- */
        if (auto *pcode = std::get_if<function_instance::func_code>(&func.code)) {
            auto fp = std::make_unique<frame>(m, std::move(arguments), module_);

            for (const value_kind& type : pcode->locals) {
                fp->locals.emplace_back(static_cast<u64>(0));
            }
        
            push_frame(std::move(fp));

            for (const auto& instr : pcode->body) {
                auto ret = instr->run(*this);
            
                if (int *p = std::get_if<int>(&ret)) {
                    break;
                }
            }

            it = (stack_.rbegin() + m + 1).base();
            assert(std::holds_alternative<frame_ptr>(*it));
            stack_.erase(it);
            pop_frame();
        }
        /* Invoke the function (host function) */
        /* -------------------------------------------------- */
        else {
            const auto& hostc = std::get<function_instance::host_code>(func.code);
            call_host(hostc.host_name, arguments);
        }


        std::cout << "<call[" << addr << "].exit>";
        show_stack();
        return {};
    }

    // result_t run(const op::call_indirect&) override {
    //     undefined("undefined");
    //     return {};
    // }
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

    result_t run(const op::global_get& ins) override {
        index_t x = ins.local;
        address_t a = current_frame()->module->globaladdrs[x];
        const global_instance& glob = store().globals[a];
        stack_push(std::visit(prelude::overloaded {
                    [](u32 v) -> svalue_t { return v; },
                    [](u64 v) -> svalue_t { return v; }
                }, glob.val));
        return {};
    }

    result_t run(const op::global_set& ins) override {
        index_t x = ins.local;
        address_t a = current_frame()->module->globaladdrs[x];
        global_instance& glob = store().globals[a];
        svalue_t val = stack_pop_raw();
        std::visit(prelude::overloaded {
                [&glob](u32 v) { glob.val = v; },
                [&glob](u64 v) { glob.val = v; },
                [](const auto& v) { throw wasm_trap("Invalid stack value"); }
            }, val);
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

    result_t run(const op::inn_clz& ins) override {
        if (ins.type == int_kind::i32) {
            u32 c = stack_pop<u32>();
            u32 count = std::countl_zero(c);
            stack_push(count);
        }
        else {
            u64 c = stack_pop<u64>();
            u64 count = std::countl_zero(c);
            stack_push(count);
        }
        return {};
    }

    result_t run(const op::inn_ctz& ins) override {
        if (ins.type == int_kind::i32) {
            u32 c = stack_pop<u32>();
            u32 count = std::countr_zero(c);
            stack_push(count);
        }
        else {
            u64 c = stack_pop<u64>();
            u64 count = std::countr_zero(c);
            stack_push(count);
        }
        return {};
    }

    result_t run(const op::inn_popcnt& ins) override {
        if (ins.type == int_kind::i32) {
            u32 c = stack_pop<u32>();
            u32 count = std::popcount(c);
            stack_push(count);
        }
        else {
            u64 c = stack_pop<u64>();
            u64 count = std::popcount(c);
            stack_push(count);
        }
        return {};
    }

    result_t run(const op::inn_add& ins) override {
        std::cout << ins.name();
        return run_binop<std::plus<>>(ins.type);
    }

    result_t run(const op::inn_sub& ins) override {
        std::cout << ins.name();
        return run_binop<std::minus<>>(ins.type);
    }

    result_t run(const op::inn_mul& ins) override {
        std::cout << ins.name();
        return run_binop<std::multiplies<>>(ins.type);
    }

    result_t run(const op::inn_div_sx& ins) override {
        std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<std::divides<>, s32, s64>(ins.type);
        }
        else {
            return run_binop<std::divides<>>(ins.type);
        }
    }

    result_t run(const op::inn_rem_sx& ins) override {
        std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<std::modulus<>, s32, s64>(ins.type);
        }
        else {
            return run_binop<std::modulus<>>(ins.type);
        }
    }

    result_t run(const op::inn_and& ins) override {
        std::cout << ins.name();
        return run_binop<std::bit_and<>>(ins.type);
    }

    result_t run(const op::inn_or& ins) override {
        std::cout << ins.name();
        return run_binop<std::bit_or<>>(ins.type);
    }

    result_t run(const op::inn_xor& ins) override {
        std::cout << ins.name();
        return run_binop<std::bit_xor<>>(ins.type);
    }

    result_t run(const op::inn_shl& ins) override {
        std::cout << ins.name();
        return run_binop<prelude::shiftl<>>(ins.type);
    }

    result_t run(const op::inn_shr_sx& ins) override {
        std::cout << ins.name();
        return run_binop<prelude::shiftr<>>(ins.type);
    }

    result_t run(const op::inn_rotl& ins) override {
        std::cout << ins.name();
        return run_binop<prelude::rotl<>>(ins.type);
    }

    result_t run(const op::inn_rotr& ins) override {
        std::cout << ins.name();
        return run_binop<prelude::rotr<>>(ins.type);
    }

    result_t run(const op::inn_eqz& ins) override {
        std::cout << ins.name();
        if (ins.type == int_kind::i32) {
            u32 c = stack_pop<u32>();
            stack_push(static_cast<u32>(c == 0));
        }
        else {
            u64 c = stack_pop<u64>();
            stack_push(static_cast<u64>(c == 0));
        }
        return {};
    }

    result_t run(const op::inn_eq& ins) override {
        std::cout << ins.name();
        return run_binop<std::equal_to<>>(ins.type);
    }

    result_t run(const op::inn_ne& ins) override {
        std::cout << ins.name();
        return run_binop<std::not_equal_to<>>(ins.type);
    }

    result_t run(const op::inn_lt_sx& ins) override {
        std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<std::less<>, s32, s64>(ins.type);
        }
        else {
            return run_binop<std::less<>>(ins.type);
        }
    }

    result_t run(const op::inn_gt_sx& ins) override {
        std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<std::greater<>, s32, s64>(ins.type);
        }
        else {
            return run_binop<std::greater<>>(ins.type);
        }
    }

    result_t run(const op::inn_le_sx& ins) override {
        std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<std::less_equal<>, s32, s64>(ins.type);
        }
        else {
            return run_binop<std::less_equal<>>(ins.type);
        }
    }

    result_t run(const op::inn_ge_sx& ins) override {
        std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<std::greater_equal<>, s32, s64>(ins.type);
        }
        else {
            return run_binop<std::greater_equal<>>(ins.type);
        }
    }

    result_t run(const op::inn_extend8_s& ins) override {
        undefined(ins);
        return {};
    }

    result_t run(const op::inn_extend16_s& ins) override {
        undefined(ins);
        return {};
    }

    result_t run(const op::i64_extend32_s& ins) override {
        undefined(ins);
        return {};
    }

    result_t run(const op::i64_extend_i32_sx& ins) override {
        if (ins.sign == sign_kind::sign) {
            u32 sv = stack_peek<u32>();
            u64 c = static_cast<u64>(static_cast<s64>(static_cast<s32>(sv)));
            stack_top() = c;
        }
        else {
            stack_top() = static_cast<u64>(stack_peek<u32>());
        }
        return {};
    }

    result_t run(const op::i32_wrap_i64& ins) override {
        stack_top() = static_cast<u32>(stack_peek<u64>());
        return {};
    }

private:
    template <typename Operator, typename T32 = u32, typename T64 = u64>
    result_t run_binop(int_kind k) {
        if (k == int_kind::i32) {
            T32 y = stack_pop<u32>();
            T32 x = stack_pop<u32>();
            u32 result = static_cast<u32>(Operator{}(x, y));
            stack_push(result);
            std::cout << " f(" << x << ", " << y << ") = " << result << " ";
        }
        else {
            T64 y = stack_pop<u64>();
            T64 x = stack_pop<u64>();
            u64 result = static_cast<u64>(Operator{}(x, y));
            stack_push(result);
            std::cout << " f(" << x << ", " << y << ") = " << result << " ";
        }
        show_stack();
        return {};
    }
};


struct basic_exe_memory : virtual execution_context, virtual MemoryExecutor {
    basic_exe_memory() = default;

    result_t run(const op::memory_size&) override {
        frame *f = current_frame();
        address_t a = f->module->memaddrs[0];
        const auto& mem = store().memorys[a];
        u32 sz = mem.data.size() / memory_instance::page_size;
        stack_push(sz);
        return {};
    }

    result_t run(const op::memory_grow& ins) override {
        undefined(ins);
        return {};
    }

    result_t run(const op::memory_fill& ins) override {
        undefined(ins);
        return {};
    }

    result_t run(const op::memory_copy& ins) override {
        undefined(ins);
        return {};
    }

    result_t run(const op::memory_init& ins) override {
        undefined(ins);
        return {};
    }

    result_t run(const op::data_drop& ins) override {
        undefined(ins);
        return {};
    }

    result_t run(const op::inn_load& ins) override {
        frame *f = current_frame();
        address_t a = f->module->memaddrs[0];
        const auto& mem = store().memorys[a];
        
        u32 i = stack_pop<u32>();
        u32 ea = i + ins.offset;

        u32 n = (ins.type == int_kind::i32) ? 4 : 8;
        if (ea + n > mem.data.size()) {
            throw wasm_trap("Invalid memory address");
        }

        if (ins.type == int_kind::i32) {
            u32 c = *reinterpret_cast<const u32*>(mem.data.data() + ea);
            std::cout << "i32.load mem[" << ea << "]=" << c << std::endl;
            stack_push(c);
        }
        else {
            u64 c = *reinterpret_cast<const u64*>(mem.data.data() + ea);
            std::cout << "i64.load mem[" << ea << "]=" << c << std::endl;
            stack_push(c);
        }

        auto *v = reinterpret_cast<const u32*>(mem.data.data());
        std::cout << "Mem: ";
        for (auto i = 0; i < 16; i++) {
            std::cout << *(v + i) << " ";
        }
        std::cout << std::endl;
        return {};
    }

    result_t run(const op::inn_load8_sx& ins) override {
        undefined();
        return {};
    }

    result_t run(const op::inn_load16_sx& ins) override {
        undefined();
        return {};
    }

    result_t run(const op::i64_load32_sx& ins) override {
        undefined();
        return {};
    }

    result_t run(const op::inn_store& ins) override {
        frame *f = current_frame();
        address_t a = f->module->memaddrs[0];
        memory_instance& mem = store().memorys[a];
        
        svalue_t sc = stack_pop_raw();
        u32 i = stack_pop<u32>();
        u32 ea = i + ins.offset;

        u32 n = (ins.type == int_kind::i32) ? 4 : 8;
        if (ea + n > mem.data.size()) {
            throw wasm_trap("Invalid memory address");
        }

        if (ins.type == int_kind::i32) {
            u32 c = std::get<u32>(sc);
            u8 *ptr = reinterpret_cast<u8*>(&c);
            std::copy(ptr, ptr + n, mem.data.data() + ea);
            std::cout << "i32.store mem[" << ea << "]=" << c << std::endl;
        }
        else {
            u64 c = std::get<u64>(sc);
            u8 *ptr = reinterpret_cast<u8*>(&c);
            std::copy(ptr, ptr + n, mem.data.data() + ea);
            std::cout << "i64.store mem[" << ea << "]=" << c << std::endl;
        }
        auto *v = reinterpret_cast<u32*>(mem.data.data());
        std::cout << "Mem: ";
        for (auto i = 0; i < 16; i++) {
            std::cout << *(v + i) << " ";
        }
        std::cout << std::endl;
        return {};
    }

    result_t run(const op::inn_store8& ins) override {
        undefined();
        return {};
    }

    result_t run(const op::inn_store16& ins) override {
        undefined();
        return {};
    }

    result_t run(const op::i64_store32& ins) override {
        undefined();
        return {};
    }
};

struct basic_executor :
        public basic_exe_control,
        public basic_exe_variable,
        public basic_exe_numeric,
        public basic_exe_memory
{
    using basic_exe_control::run;
    using basic_exe_variable::run;
    using basic_exe_numeric::run;
    using basic_exe_memory::run;
};

}  // namespace ligero::vm
