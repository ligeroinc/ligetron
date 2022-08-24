#pragma once

#include <runtime.hpp>
#include <instruction.hpp>
#include <prelude.hpp>
#include <exception>
#include <tuple>
#include <limits>
#include <bit>

namespace ligero::vm {

template <typename Context>
class basic_exe_control : virtual public ControlExecutor {
    Context& ctx_;

public:
    basic_exe_control(Context& ctx) : ctx_(ctx) { }    

    result_t run(const op::nop&) override {
        return {};
    }
    
    result_t run(const op::unreachable&) override {
        throw wasm_trap("Unreachable");
        return {};
    }

    result_t run(const op::block& block) override {
        /* Entering block with Label L */
        u32 m = 0, n = 0;
        if (block.type) {
            const auto& type = ctx_.module()->types[*block.type];
            m = type.params.size();
            n = type.returns.size();
        }

        // stack_.insert((stack_.rbegin() + m).base(), label{ n });
        ctx_.block_entry(m, n);

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
        ctx_.block_exit(n);
        return {};
    }

    result_t run(const op::loop& block) override {
    loop:
        /* Entering block with Label L */
        u32 m = 0, n = 0;
        if (block.type) {
            const auto& type = ctx_.module()->types[*block.type];
            m = type.params.size();
            n = type.returns.size();
        }

        // stack_.insert((stack_.rbegin() + m).base(), label{ n });
        ctx_.block_entry(m, n);

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
        ctx_.block_exit(n);
        return {};
    }

    result_t run(const op::if_then_else&) override {
        throw wasm_trap("Impossible instruction if_then_else");
        return {};
    }

    result_t run(const op::br& b) override {
        const int l = b.label;

        /* Find L-th label */
        auto it = ctx_.template find_nth<label>(l);
        // int count = -1;
        // auto rit = std::find_if(stack_.rbegin(), stack_.rend(), [&count, l](const auto& v) {
        //     if (std::holds_alternative<label>(v)) {
        //         count++;
        //         if (count == l) {
        //             return true;
        //         }
        //     }
        //     return false;
        // });

        // auto it = (++rit).base();

        const size_t n = std::get<label>(*it).arity;
        ctx_.stack_pops(it, ctx_.stack_top() + n);
        // auto top = stack_.rbegin() + n;
        // stack_.erase(it, top.base());

        std::cout << "<br " << l << "> ";
        ctx_.show_stack();
        return l;
    }

    result_t run(const op::br_if& b) override {
        u32 cond = ctx_.template stack_pop<u32>();
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
        const size_t arity = ctx_.current_frame()->arity;
        // auto top = stack_.rbegin() + arity;
        // auto it = top;

        auto frm = ctx_.template find_nth<frame_ptr>(1);
        ctx_.template stack_pops(frm, ctx_.template stack_top() + arity);
        // while (!std::holds_alternative<frame_ptr>(*it)) {
        //     it++;
        // }

        // stack_.erase((++it).base(), top.base());

        return std::numeric_limits<int>::max();
    }

    result_t run(const op::call& c) override {        
        address_t addr =  ctx_.current_frame()->module->funcaddrs[c.func];
        const function_instance& func = ctx_.store()->functions[addr];
        u32 n = func.kind.params.size();
        u32 m = func.kind.returns.size();

        std::cout << "<call[" << addr << "].entry>";
        ctx_.show_stack();

        /* Push arguments */
        /* -------------------------------------------------- */
        std::vector<value_t> arguments;
        for (auto it = ctx_.template stack_top(); it != ctx_.template stack_top() + n; it++) {
            arguments.push_back(to_value(*it));
            // if (std::holds_alternative<u32>(*i)) {
            //     arguments.emplace_back(std::get<u32>(*i));
            // }
            // else if (std::holds_alternative<u64>(*i)) {
            //     arguments.emplace_back(std::get<u64>(*i));
            // }
            // else {
            //     undefined("Invalid stack value");
            // }
        }
        ctx_.template stack_pops(ctx_.template stack_top() + n - 1, ctx_.template stack_top());
        std::reverse(arguments.begin(), arguments.end());
        // stack_.erase((stack_.rbegin() + n).base(), stack_.end());

        /* Invoke the function (native function) */
        /* -------------------------------------------------- */
        if (auto *pcode = std::get_if<function_instance::func_code>(&func.code)) {
            auto fp = std::make_unique<frame>(m, std::move(arguments), ctx_.module());

            for (const value_kind& type : pcode->locals) {
                fp->locals.emplace_back(static_cast<u64>(0));
            }
        
            ctx_.push_frame(std::move(fp));

            for (const auto& instr : pcode->body) {
                auto ret = instr->run(*this);
            
                if (int *p = std::get_if<int>(&ret)) {
                    break;
                }
            }

            ctx_.pop_frame(m);
        }
        /* Invoke the function (host function) */
        /* -------------------------------------------------- */
        else {
            const auto& hostc = std::get<function_instance::host_code>(func.code);
            ctx_.call_host(hostc.host_name, arguments);
        }


        std::cout << "<call[" << addr << "].exit>";
        ctx_.show_stack();
        return {};
    }

    // result_t run(const op::call_indirect&) override {
    //     undefined("undefined");
    //     return {};
    // }
};

template <typename Context>
class basic_exe_variable : virtual public VariableExecutor {
    Context& ctx_;

public:
    basic_exe_variable(Context& ctx) : ctx_(ctx) { }

    result_t run(const op::local_get& var) override {
        index_t x = var.local;
        auto local = ctx_.local_get(x);
        if (std::holds_alternative<u32>(local)) {
            ctx_.template stack_push(std::get<u32>(local));
            std::cout << "local.get[" << x << "]=" << std::get<u32>(local) << " ";
        }
        else {
            ctx_.template stack_push(std::get<u64>(local));
        }
        ctx_.show_stack();
        return {};
    }

    result_t run(const op::local_set& var) override {
        index_t x = var.local;
        u32 top = ctx_.template stack_pop<u32>();
        std::cout << "local.set[" << x << "]=" << top << " ";
        ctx_.local_set(x, top);
        ctx_.show_stack();
        return {};
    }

    result_t run(const op::local_tee& var) override {
        index_t x = var.local;
        u32 top = ctx_.template stack_peek<u32>();
        std::cout << "local.tee[" << x << "]=" << top << " ";
        ctx_.local_set(x, top);
        ctx_.show_stack();
        return {};
    }

    result_t run(const op::global_get& ins) override {
        index_t x = ins.local;
        address_t a = ctx_.current_frame()->module->globaladdrs[x];
        const global_instance& glob = ctx_.store()->globals[a];
        ctx_.template stack_push(to_svalue(glob.val));
        return {};
    }

    result_t run(const op::global_set& ins) override {
        index_t x = ins.local;
        address_t a = ctx_.current_frame()->module->globaladdrs[x];
        global_instance& glob = ctx_.store()->globals[a];
        svalue_t val = ctx_.template stack_pop_raw();
        glob.val = to_value(val);
        // std::visit(prelude::overloaded {
        //         [&glob](u32 v) { glob.val = v; },
        //         [&glob](u64 v) { glob.val = v; },
        //         [](const auto& v) { throw wasm_trap("Invalid stack value"); }
        //     }, val);
        return {};
    }
};


template <typename Context>
class basic_exe_numeric : virtual public NumericExecutor {
    Context& ctx_;

public:
    using opt = typename Context::operator_type;
    
    basic_exe_numeric(Context& ctx) : ctx_(ctx) { }

    result_t run(const op::inn_const& i) override {
        if (i.type == int_kind::i32) {
            ctx_.template stack_push(static_cast<u32>(i.val));
        }
        else {
            ctx_.template stack_push(i.val);
        }
        return {};
    }

    result_t run(const op::inn_clz& ins) override {
        if (ins.type == int_kind::i32) {
            u32 c = ctx_.template stack_pop<u32>();
            u32 count = typename opt::countl_zero<u32>{}(c);
            ctx_.template stack_push(count);
        }
        else {
            u64 c = ctx_.template stack_pop<u64>();
            u64 count = typename opt::countl_zero<u64>{}(c);
            ctx_.template stack_push(count);
        }
        return {};
    }

    result_t run(const op::inn_ctz& ins) override {
        if (ins.type == int_kind::i32) {
            u32 c = ctx_.template stack_pop<u32>();
            u32 count = typename opt::countr_zero<u32>{}(c);
            ctx_.template stack_push(count);
        }
        else {
            u64 c = ctx_.template stack_pop<u64>();
            u64 count = typename opt::countr_zero<u64>{}(c);
            ctx_.template stack_push(count);
        }
        return {};
    }

    result_t run(const op::inn_popcnt& ins) override {
        if (ins.type == int_kind::i32) {
            u32 c = ctx_.template stack_pop<u32>();
            u32 count = typename opt::popcount<u32>{}(c);
            ctx_.template stack_push(count);
        }
        else {
            u64 c = ctx_.template stack_pop<u64>();
            u64 count = typename opt::popcount<u64>{}(c);
            ctx_.template stack_push(count);
        }
        return {};
    }

    result_t run(const op::inn_add& ins) override {
        std::cout << ins.name();
        return run_binop<typename opt::plus>(ins.type);
    }

    result_t run(const op::inn_sub& ins) override {
        std::cout << ins.name();
        return run_binop<typename opt::minus>(ins.type);
    }

    result_t run(const op::inn_mul& ins) override {
        std::cout << ins.name();
        return run_binop<typename opt::multiplies>(ins.type);
    }

    result_t run(const op::inn_div_sx& ins) override {
        std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<typename opt::divides, s32, s64>(ins.type);
        }
        else {
            return run_binop<typename opt::divides>(ins.type);
        }
    }

    result_t run(const op::inn_rem_sx& ins) override {
        std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<typename opt::modulus, s32, s64>(ins.type);
        }
        else {
            return run_binop<typename opt::modulus>(ins.type);
        }
    }

    result_t run(const op::inn_and& ins) override {
        std::cout << ins.name();
        return run_binop<typename opt::bit_and>(ins.type);
    }

    result_t run(const op::inn_or& ins) override {
        std::cout << ins.name();
        return run_binop<typename opt::bit_or>(ins.type);
    }

    result_t run(const op::inn_xor& ins) override {
        std::cout << ins.name();
        return run_binop<typename opt::bit_xor>(ins.type);
    }

    result_t run(const op::inn_shl& ins) override {
        std::cout << ins.name();
        return run_binop<typename opt::shiftl>(ins.type);
    }

    result_t run(const op::inn_shr_sx& ins) override {
        std::cout << ins.name();
        return run_binop<typename opt::shiftr>(ins.type);
    }

    result_t run(const op::inn_rotl& ins) override {
        std::cout << ins.name();
        return run_binop<typename opt::rotatel>(ins.type);
    }

    result_t run(const op::inn_rotr& ins) override {
        std::cout << ins.name();
        return run_binop<typename opt::rotater>(ins.type);
    }

    result_t run(const op::inn_eqz& ins) override {
        std::cout << ins.name();
        if (ins.type == int_kind::i32) {
            u32 c = ctx_.template stack_pop<u32>();
            u32 r = typename opt::equal_to{}(c, u32{0});
            ctx_.template stack_push(r);
        }
        else {
            u64 c = ctx_.template stack_pop<u64>();
            u64 r = typename opt::equal_to{}(c, u64{0});
            ctx_.template stack_push(r);
        }
        return {};
    }

    result_t run(const op::inn_eq& ins) override {
        std::cout << ins.name();
        return run_binop<typename opt::equal_to>(ins.type);
    }

    result_t run(const op::inn_ne& ins) override {
        std::cout << ins.name();
        return run_binop<typename opt::not_equal_to>(ins.type);
    }

    result_t run(const op::inn_lt_sx& ins) override {
        std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<typename opt::less, s32, s64>(ins.type);
        }
        else {
            return run_binop<typename opt::less>(ins.type);
        }
    }

    result_t run(const op::inn_gt_sx& ins) override {
        std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<typename opt::greater, s32, s64>(ins.type);
        }
        else {
            return run_binop<typename opt::greater>(ins.type);
        }
    }

    result_t run(const op::inn_le_sx& ins) override {
        std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<typename opt::less_equal, s32, s64>(ins.type);
        }
        else {
            return run_binop<typename opt::less_equal>(ins.type);
        }
    }

    result_t run(const op::inn_ge_sx& ins) override {
        std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<typename opt::greater_equal, s32, s64>(ins.type);
        }
        else {
            return run_binop<typename opt::greater_equal>(ins.type);
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
            u32 sv = ctx_.template stack_pop<u32>();
            u64 c = static_cast<u64>(static_cast<s64>(static_cast<s32>(sv)));
            ctx_.template stack_push(c);
        }
        else {
            u32 sv = ctx_.template stack_pop<u32>();
            u64 c = sv;
            ctx_.template stack_push(c);
        }
        return {};
    }

    result_t run(const op::i32_wrap_i64& ins) override {
        u64 sv = ctx_.template stack_pop<u64>();
        ctx_.template stack_push(static_cast<u32>(sv));
        return {};
    }

private:
    template <typename Op, typename T32 = u32, typename T64 = u64>
    result_t run_binop(int_kind k) {
        if (k == int_kind::i32) {
            T32 y = ctx_.template stack_pop<u32>();
            T32 x = ctx_.template stack_pop<u32>();
            u32 result = static_cast<u32>(Op{}(x, y));
            ctx_.template stack_push(result);
            std::cout << " f(" << x << ", " << y << ") = " << result << " ";
        }
        else {
            T64 y = ctx_.template stack_pop<u64>();
            T64 x = ctx_.template stack_pop<u64>();
            u64 result = static_cast<u64>(Op{}(x, y));
            ctx_.template stack_push(result);
            std::cout << " f(" << x << ", " << y << ") = " << result << " ";
        }
        ctx_.show_stack();
        return {};
    }
};

template <typename Context>
class basic_exe_memory : virtual public MemoryExecutor {
    Context& ctx_;

public:
    basic_exe_memory(Context& ctx) : ctx_(ctx) { }

    result_t run(const op::memory_size&) override {
        frame *f = ctx_.current_frame();
        address_t a = f->module->memaddrs[0];
        const auto& mem = ctx_.store()->memorys[a];
        u32 sz = mem.data.size() / memory_instance::page_size;
        ctx_.template stack_push(sz);
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
        frame *f = ctx_.current_frame();
        address_t a = f->module->memaddrs[0];
        const auto& mem = ctx_.store()->memorys[a];
        
        u32 i = ctx_.template stack_pop<u32>();
        u32 ea = i + ins.offset;

        u32 n = (ins.type == int_kind::i32) ? 4 : 8;
        if (ea + n > mem.data.size()) {
            throw wasm_trap("Invalid memory address");
        }

        if (ins.type == int_kind::i32) {
            u32 c = *reinterpret_cast<const u32*>(mem.data.data() + ea);
            std::cout << "i32.load mem[" << ea << "]=" << c << std::endl;
            ctx_.template stack_push(c);
        }
        else {
            u64 c = *reinterpret_cast<const u64*>(mem.data.data() + ea);
            std::cout << "i64.load mem[" << ea << "]=" << c << std::endl;
            ctx_.template stack_push(c);
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
        frame *f = ctx_.current_frame();
        address_t a = f->module->memaddrs[0];
        memory_instance& mem = ctx_.store()->memorys[a];
        
        svalue_t sc = ctx_.template stack_pop_raw();
        u32 i = ctx_.template stack_pop<u32>();
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

template <typename Context>
struct basic_executor :
        public basic_exe_control<Context>,
        public basic_exe_variable<Context>,
        public basic_exe_numeric<Context>,
        public basic_exe_memory<Context>
{
    using basic_exe_control<Context>::run;
    using basic_exe_variable<Context>::run;
    using basic_exe_numeric<Context>::run;
    using basic_exe_memory<Context>::run;

    basic_executor(Context& ctx)
        : ctx_(ctx),
          basic_exe_control<Context>(ctx),
          basic_exe_variable<Context>(ctx),
          basic_exe_numeric<Context>(ctx),
          basic_exe_memory<Context>(ctx)
        { }

    Context& context() { return ctx_; }

protected:
    Context& ctx_;
};

}  // namespace ligero::vm
