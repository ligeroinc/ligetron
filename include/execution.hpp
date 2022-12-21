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
class basic_exe_parametric : virtual public ParametricExecutor {
    using u32_type = typename Context::u32_type;
    
    Context& ctx_;

public:
    basic_exe_parametric(Context& ctx) : ctx_(ctx) { }

    result_t run(const op::drop& ins) override {
        ctx_.stack_pop();
        return {};
    }

    result_t run(const op::select&) override {
        u32_type c = ctx_.stack_pop().template as<u32_type>();
        u32_type v2 = ctx_.stack_pop().template as<u32_type>();
        u32_type v1 = ctx_.stack_pop().template as<u32_type>();
        if constexpr (requires { u32_type::size; }) {
            for (size_t i = 0; i < u32_type::size; i++) {
                c[i] = c[i] != 0 ? v1[i] : v2[i];
            }
        }
        else {
            c = c != 0 ? v1 : v2;
        }
        ctx_.stack_push(c);
        return {};
    }
};

template <typename Context>
class basic_exe_control : virtual public ControlExecutor {
    using value_type = typename Context::value_type;
    using svalue_type = typename Context::svalue_type;
    
    using u32_type = typename Context::u32_type;
    using u64_type = typename Context::u64_type;
    using frame_type = typename Context::frame_type;
    using frame_ptr = typename svalue_type::frame_ptr;
    using frame_pointer = typename Context::frame_pointer;
    
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
        {
            ctx_.drop_n_below(1, n);
            // std::vector<svalue_t> ret;
            // for (size_t i = 0; i < n; i++) {
            //     ret.emplace_back(ctx_.stack_pop_raw());
            // }
            // ctx_.template stack_pop<label>();
            // for (auto it = ret.rbegin(); it != ret.rend(); it++) {
            //     ctx_.stack_push(std::move(*it));
            // }
        }
        
        return {};
    }

    result_t run(const op::loop& block) override {
        /* Entering block with Label L */
        u32 m = 0, n = 0;
        if (block.type) {
            const auto& type = ctx_.module()->types[*block.type];
            m = type.params.size();
            n = type.returns.size();
        }

    loop:
        // stack_.insert((stack_.rbegin() + m).base(), label{ n });
        ctx_.block_entry(m, n);

        for (const auto& instr : block.body) {
            auto ret = instr->run(*this);

            /* Return from nested block */
            if (int *p = std::get_if<int>(&ret)) {
                assert(*p >= 0);
                if (*p == 0) {
                    // std::cout << std::endl << std::endl;
                    goto loop;
                }
                else {
                    return *p - 1;
                }
            }
        }

        /* Exiting block with Label L if no jump happended */
        {
            ctx_.drop_n_below(1, n);
            // std::vector<svalue_t> ret;
            // for (size_t i = 0; i < n; i++) {
            //     ret.emplace_back(ctx_.stack_pop_raw());
            // }
            // ctx_.template stack_pop<label>();
            // for (auto it = ret.rbegin(); it != ret.rend(); it++) {
            //     ctx_.stack_push(std::move(*it));
            // }
        }
        return {};
    }

    result_t run(const op::if_then_else& branch) override {
        u32 m = 0, n = 0;
        if (branch.type) {
            const auto& type = ctx_.module()->types[*branch.type];
            m = type.params.size();
            n = type.params.size();
        }

        u32 c = ctx_.stack_pop().template as<u32_type>();
        ctx_.block_entry(m, n);

        auto& ins_vec = c ? branch.then_body : branch.else_body;
        
        for (const instr_ptr& instr : ins_vec) {
            auto ret = instr->run(*this);

            if (int *p = std::get_if<int>(&ret)) {
                assert (*p >= 0);
                if (*p == 0) {
                    return {};
                }
                else {
                    return *p - 1;
                }
            }
        }

        ctx_.drop_n_below(1, n);
        return {};
    }

    result_t run(const op::br& b) override {
        const int l = b.label;

        /* Find L-th label */
        auto it = prelude::find_if_n(ctx_.stack_top(), ctx_.stack_bottom(), l+1, [](const auto& v) {
            return std::holds_alternative<label>(v.data());
        });
        assert(it != ctx_.stack_bottom());
        size_t distance = std::distance(ctx_.stack_top(), it);

        /* Pop unused stack values */
        {
            const size_t n = it->template as<label>().arity;
            ctx_.drop_n_below(distance - n + 1, n);
            // std::vector<svalue_t> ret;
            // for (size_t i = 0; i < n; i++) {
            //     ret.emplace_back(ctx_.stack_pop_raw());
            // }
            // for (size_t i = 0; i < distance - n; i++) {
            //     ctx_.stack_pop_raw();
            // }
            // ctx_.template stack_pop<label>();  // Discard label
            // for (auto it = ret.rbegin(); it != ret.rend(); it++) {
            //     ctx_.stack_push(std::move(*it));
            // }
        }

        // std::cout << "<br " << l << "> ";
        // ctx_.show_stack();
        return l;
    }

    result_t run(const op::br_if& b) override {
        // u32 cond = ctx_.template stack_pop<u32>();
        auto v = ctx_.stack_pop().template as<u32_type>();

        u32 cond = 0;
        if constexpr (requires{ {v.begin()}; {v.end()}; }) {
            // assert(std::all_of(v.begin(), v.end(), [first = v[0]](const auto& a) {
            //     return a == first;
            // }));
            cond = v[0];
        }
        else {
            cond = v;
        }
        
        if (cond) {
            return run(op::br{ b.label });
        }
        return {};
    }

    result_t run(const op::br_table& b) override {
        // u32 i = ctx_.template stack_pop<u32>();
        auto v = ctx_.stack_pop().template as<u32_type>();

        u32 i = 0;
        if constexpr (requires{ {v.begin()}; {v.end()}; }) {
            assert(std::all_of(v.begin(), v.end(), [first = v[0]](const auto& a) {
                return a == first;
            }));
            i = v[0];
        }
        else {
            i = v;
        }

        if (i < b.branches.size()) {
            return run(op::br{ b.branches[i] });
        }
        else {
            return run(op::br{ b.default_ });
        }
        return {};
    }

    result_t run(const op::ret&) override {
        // std::cout << "Ret" << std::endl;
        const size_t arity = ctx_.current_frame()->arity;
        // auto top = stack_.rbegin() + arity;
        // auto it = top;

        auto frm = std::find_if(ctx_.stack_top(), ctx_.stack_bottom(), [](const auto& v) {
            return std::holds_alternative<frame_ptr>(v.data());
        });
        assert(frm != ctx_.stack_bottom());
        size_t distance = std::distance(ctx_.stack_top(), frm);
        
        {
            ctx_.drop_n_below(distance - arity + 1, arity);
            // std::vector<svalue_t> ret;
            // for (size_t i = 0; i < arity; i++) {
            //     ret.emplace_back(ctx_.stack_pop_raw());
            // }
            // for (size_t i = 0; i < distance - arity; i++) {
            //     ctx_.stack_pop_raw();
            // }
            // for (auto it = ret.rbegin(); it != ret.rend(); it++) {
            //     ctx_.stack_push(std::move(*it));
            // }
            // ctx_.template stack_pop<frame_ptr>();
        }

        return std::numeric_limits<int>::max();
    }

    result_t run(const op::call& c) override {        
        address_t addr =  ctx_.current_frame()->module->funcaddrs[c.func];
        const function_instance& func = ctx_.store()->functions[addr];
        u32 n = func.kind.params.size();
        u32 m = func.kind.returns.size();

        // std::cout << "<call[" << addr << "].entry>";

        /* Push arguments */
        /* -------------------------------------------------- */
        auto fp = std::make_unique<frame_type>();
        fp->arity = m;
        fp->module = ctx_.module();
        ctx_.push_arguments(fp.get(), n);
        // std::vector<value_type> arguments;
        // for (size_t i = 0; i < n; i++) {
        //     svalue_type sv = ctx_.stack_pop();
        //     arguments.push_back(std::move(sv.template as<value_type>()));
        // }
        // std::reverse(arguments.begin(), arguments.end());
        

        /* Invoke the function (native function) */
        /* -------------------------------------------------- */
        if (auto *pcode = std::get_if<function_instance::func_code>(&func.code)) {
            // auto fp = std::make_unique<frame_type>(m, std::move(arguments), ctx_.module());

            for (const value_kind& type : pcode->locals) {
                // fp->locals.emplace_back(u32_type(0U));
                ctx_.push_local(fp.get(), type);
            }
        
            ctx_.push_frame(std::move(fp));

            for (const auto& instr : pcode->body) {
                auto ret = instr->run(*this);
            
                if (int *p = std::get_if<int>(&ret)) {
                    break;
                }
            }

            // ctx_.pop_frame(m);
            {
                ctx_.drop_n_below(1, m);
                // std::vector<svalue_t> ret;
                // for (size_t i = 0; i < m; i++) {
                //     ret.emplace_back(ctx_.stack_pop_raw());
                // }

                // // ctx_.template stack_pop<label>();
                // ctx_.template stack_pop<frame_ptr>();
                
                // for (auto it = ret.rbegin(); it != ret.rend(); it++) {
                //     ctx_.stack_push(std::move(*it));
                // }
            }
        }
        /* Invoke the function (host function) */
        /* -------------------------------------------------- */
        else {
            undefined();
            // const auto& hostc = std::get<function_instance::host_code>(func.code);
            // ctx_.call_host(hostc.host_name, arguments);
        }


        // std::cout << "<call[" << addr << "].exit>" << std::endl;
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
        // auto local = ctx_.local_get(x);
        // ctx_.stack_push(std::move(local));
        ctx_.stack_push(ctx_.current_frame()->locals[x]);
        return {};
    }

    result_t run(const op::local_set& var) override {
        index_t x = var.local;
        // auto top = ctx_.stack_pop().template as<typename Context::value_type>();
        // ctx_.local_set(x, std::move(top));
        ctx_.current_frame()->locals[x] = ctx_.stack_pop().template as<typename Context::value_type>();

        // std::cout << "local.set[" << x << "]" << std::endl;
        // ctx_.show_stack();
        return {};
    }

    result_t run(const op::local_tee& var) override {
        index_t x = var.local;
        // auto& top = ctx_.stack_peek().template as<typename Context::value_type>();
        // ctx_.local_set(x, top);
        ctx_.current_frame()->locals[x] = ctx_.stack_peek().template as<typename Context::value_type>();
        
        // std::cout << "local.tee[" << x << "]" << std::endl;
        // ctx_.show_stack();
        return {};
    }

    result_t run(const op::global_get& ins) override {
        index_t x = ins.local;
        address_t a = ctx_.current_frame()->module->globaladdrs[x];
        const global_instance& glob = ctx_.store()->globals[a];
        ctx_.stack_push(typename Context::u32_type(glob.val));
        
        // ctx_.show_stack();
        return {};
    }

    result_t run(const op::global_set& ins) override {
        index_t x = ins.local;
        address_t a = ctx_.current_frame()->module->globaladdrs[x];
        global_instance& glob = ctx_.store()->globals[a];
        auto val = ctx_.stack_pop().template as<typename Context::u32_type>();

        u32 gv = 0;
        if constexpr (requires(typename Context::u32_type u, size_t i) { u[i]; }) {
            gv = val[0];
        }
        else {
            gv = val;
        }
        glob.val = gv;
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
            ctx_.stack_push(static_cast<u32>(i.val));
        }
        else {
            ctx_.stack_push(i.val);
        }
        return {};
    }

    result_t run(const op::inn_clz& ins) override {
        if (ins.type == int_kind::i32) {
            u32 c = ctx_.stack_pop().template as<u32>();
            u32 count = typename opt::template countl_zero<u32>{}(c);
            ctx_.stack_push(count);
        }
        else {
            u64 c = ctx_.stack_pop().template as<u64>();
            u64 count = typename opt::template countl_zero<u64>{}(c);
            ctx_.stack_push(count);
        }
        return {};
    }

    result_t run(const op::inn_ctz& ins) override {
        if (ins.type == int_kind::i32) {
            u32 c = ctx_.stack_pop().template as<u32>();
            u32 count = typename opt::template countr_zero<u32>{}(c);
            ctx_.stack_push(count);
        }
        else {
            u64 c = ctx_.template stack_pop().template as<u64>();
            u64 count = typename opt::template countr_zero<u64>{}(c);
            ctx_.stack_push(count);
        }
        return {};
    }

    result_t run(const op::inn_popcnt& ins) override {
        if (ins.type == int_kind::i32) {
            u32 c = ctx_.stack_pop().template as<u32>();
            u32 count = typename opt::template popcount<u32>{}(c);
            ctx_.stack_push(count);
        }
        else {
            u64 c = ctx_.stack_pop().template as<u64>();
            u64 count = typename opt::template popcount<u64>{}(c);
            ctx_.stack_push(count);
        }
        return {};
    }

    result_t run(const op::inn_add& ins) override {
        // std::cout << ins.name();
        return run_binop<typename opt::plus>(ins.type);
    }

    result_t run(const op::inn_sub& ins) override {
        // std::cout << ins.name();
        return run_binop<typename opt::minus>(ins.type);
    }

    result_t run(const op::inn_mul& ins) override {
        // std::cout << ins.name();
        return run_binop<typename opt::multiplies>(ins.type);
    }

    result_t run(const op::inn_div_sx& ins) override {
        // std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<typename opt::divides, s32, s64>(ins.type);
        }
        else {
            return run_binop<typename opt::divides>(ins.type);
        }
    }

    result_t run(const op::inn_rem_sx& ins) override {
        // std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<typename opt::modulus, s32, s64>(ins.type);
        }
        else {
            return run_binop<typename opt::modulus>(ins.type);
        }
    }

    result_t run(const op::inn_and& ins) override {
        // std::cout << ins.name();
        return run_binop<typename opt::bit_and>(ins.type);
    }

    result_t run(const op::inn_or& ins) override {
        // std::cout << ins.name();
        return run_binop<typename opt::bit_or>(ins.type);
    }

    result_t run(const op::inn_xor& ins) override {
        // std::cout << ins.name();
        return run_binop<typename opt::bit_xor>(ins.type);
    }

    result_t run(const op::inn_shl& ins) override {
        // std::cout << ins.name();
        return run_binop<typename opt::shiftl>(ins.type);
    }

    result_t run(const op::inn_shr_sx& ins) override {
        // std::cout << ins.name();
        return run_binop<typename opt::shiftr>(ins.type);
    }

    result_t run(const op::inn_rotl& ins) override {
        // std::cout << ins.name();
        return run_binop<typename opt::rotatel>(ins.type);
    }

    result_t run(const op::inn_rotr& ins) override {
        // std::cout << ins.name();
        return run_binop<typename opt::rotater>(ins.type);
    }

    result_t run(const op::inn_eqz& ins) override {
        // std::cout << ins.name();
        if (ins.type == int_kind::i32) {
            u32 c = ctx_.stack_pop().template as<u32>();
            u32 r = typename opt::equal_to{}(c, u32{0});
            ctx_.stack_push(r);
        }
        else {
            u64 c = ctx_.stack_pop().template as<u64>();
            u64 r = typename opt::equal_to{}(c, u64{0});
            ctx_.stack_push(r);
        }
        return {};
    }

    result_t run(const op::inn_eq& ins) override {
        // std::cout << ins.name();
        return run_binop<typename opt::equal_to>(ins.type);
    }

    result_t run(const op::inn_ne& ins) override {
        // std::cout << ins.name();
        return run_binop<typename opt::not_equal_to>(ins.type);
    }

    result_t run(const op::inn_lt_sx& ins) override {
        // std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<typename opt::less, s32, s64>(ins.type);
        }
        else {
            return run_binop<typename opt::less>(ins.type);
        }
    }

    result_t run(const op::inn_gt_sx& ins) override {
        // std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<typename opt::greater, s32, s64>(ins.type);
        }
        else {
            return run_binop<typename opt::greater>(ins.type);
        }
    }

    result_t run(const op::inn_le_sx& ins) override {
        // std::cout << ins.name();
        if (ins.sign == sign_kind::sign) {
            return run_binop<typename opt::less_equal, s32, s64>(ins.type);
        }
        else {
            return run_binop<typename opt::less_equal>(ins.type);
        }
    }

    result_t run(const op::inn_ge_sx& ins) override {
        // std::cout << ins.name();
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
            u32 sv = ctx_.stack_pop().template as<u32>();
            u64 c = static_cast<u64>(static_cast<s64>(static_cast<s32>(sv)));
            ctx_.stack_push(c);
        }
        else {
            u32 sv = ctx_.stack_pop().template as<u32>();
            u64 c = sv;
            ctx_.stack_push(c);
        }
        return {};
    }

    result_t run(const op::i32_wrap_i64& ins) override {
        u64 sv = ctx_.stack_pop().template as<u64>();
        ctx_.stack_push(static_cast<u32>(sv));
        return {};
    }

private:
    template <typename Op, typename T32 = u32, typename T64 = u64>
    result_t run_binop(int_kind k) {
        if (k == int_kind::i32) {
            T32 y = ctx_.stack_pop().template as<u32>();
            T32 x = ctx_.stack_pop().template as<u32>();
            u32 result = static_cast<u32>(Op{}(x, y));
            ctx_.stack_push(result);
            // std::cout << " f(" << x << ", " << y << ") = " << result << " ";
        }
        else {
            T64 y = ctx_.stack_pop().template as<u64>();
            T64 x = ctx_.stack_pop().template as<u64>();
            u64 result = static_cast<u64>(Op{}(x, y));
            ctx_.stack_push(result);
            // std::cout << " f(" << x << ", " << y << ") = " << result << " ";
        }
        // ctx_.show_stack();
        return {};
    }
};

template <typename Context>
class basic_exe_memory : virtual public MemoryExecutor {
    using value_type = typename Context::value_type;
    using svalue_type = typename Context::svalue_type;
    
    using u32_type = typename Context::u32_type;
    using u64_type = typename Context::u64_type;
    using frame_type = typename Context::frame_type;
    using frame_pointer = typename Context::frame_pointer;
    
    Context& ctx_;

public:
    basic_exe_memory(Context& ctx) : ctx_(ctx) { }

    result_t run(const op::memory_size&) override {
        frame_pointer f = ctx_.current_frame();
        address_t a = f->module->memaddrs[0];
        const auto& mem = ctx_.store()->memorys[a];
        u32 sz = mem.data.size() / memory_instance::page_size;
        ctx_.stack_push(u32_type(sz));
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
        frame_pointer f = ctx_.current_frame();
        address_t ma = f->module->memaddrs[0];
        auto& mem = ctx_.store()->memorys[ma];

        address_t da = f->module->dataaddrs[ins.data_index];
        auto& data = ctx_.store()->datas[da];

        u32 n = ctx_.stack_pop().template as<u32>();
        u32 s = ctx_.stack_pop().template as<u32>();
        u32 d = ctx_.stack_pop().template as<u32>();

        if (s + n > data.data.size() or d + n > mem.data.size()) {
            throw wasm_trap("memory_init: Invalid address");
        }

        std::copy(data.data.begin() + s, data.data.begin() + s + n, mem.data.begin() + d);
        
        return {};
    }

    result_t run(const op::data_drop& ins) override {
        frame_pointer f = ctx_.current_frame();
        address_t da = f->module->dataaddrs[ins.data_index];
        auto& data = ctx_.store()->datas[da];
        data.data.clear();
        return {};
    }

    template <typename Load, typename To>
    void do_load(u32 offset) {
        frame_pointer f = ctx_.current_frame();
        address_t a = f->module->memaddrs[0];
        const auto& mem = ctx_.store()->memorys[a];
        
        u32_type i = ctx_.stack_pop().template as<u32_type>();
        u32_type ea = i + To{ offset };

        // u32 n = (ins.type == int_kind::i32) ? 4 : 8;
        u32 n = sizeof(Load);
        if (ea + n > mem.data.size()) {
            // std::cout << ea << " " << ea + n << " " << mem.data.size() << std::endl;
            throw wasm_trap("Invalid memory address");
        }

        To c = *reinterpret_cast<const Load*>(mem.data.data() + ea);
        ctx_.stack_push(c);
        
        // prelude::transform(ea, [&](const auto& v) {
        //     if (v + n > mem.data.size()) {
        //         // std::cout << ea << " " << ea + n << " " << mem.data.size() << std::endl;
        //         throw wasm_trap("Invalid memory address");
        //     }

        //     To c = *reinterpret_cast<const Load*>(mem.data.data() + v);
        //     // std::cout << "memory.load mem[" << ea << "]=" << c;
        //     // ctx_.template stack_push(c);
        //     return c;
        // });

        // ctx_.stack_push(ea);
    }

    result_t run(const op::inn_load& ins) override {
        if (ins.type == int_kind::i32) {
            do_load<u32, u32>(ins.offset);
        }
        else {
            do_load<u64, u64>(ins.offset);
        }
        return {};
    }

    result_t run(const op::inn_load8_sx& ins) override {
        if (ins.type == int_kind::i32) {
            if (ins.sign == sign_kind::sign) {
                do_load<s8, u32>(ins.offset);
            }
            else {
                do_load<u8, u32>(ins.offset);
            }
        }
        else {
            if (ins.sign == sign_kind::sign) {
                do_load<s8, u64>(ins.offset);
            }
            else {
                do_load<u8, u64>(ins.offset);
            }
        }
        return {};
    }

    result_t run(const op::inn_load16_sx& ins) override {
        if (ins.type == int_kind::i32) {
            if (ins.sign == sign_kind::sign) {
                do_load<s16, u32>(ins.offset);
            }
            else {
                do_load<u16, u32>(ins.offset);
            }
        }
        else {
            if (ins.sign == sign_kind::sign) {
                do_load<s16, u64>(ins.offset);
            }
            else {
                do_load<u16, u64>(ins.offset);
            }
        }
        return {};
    }

    result_t run(const op::i64_load32_sx& ins) override {
        if (ins.sign == sign_kind::sign) {
            do_load<s32, u64>(ins.offset);
        }
        else {
            do_load<u32, u64>(ins.offset);
        }
        return {};
    }

    template <typename T, typename Dest>
    result_t do_store(u32 offset) {
        frame_pointer f = ctx_.current_frame();
        address_t a = f->module->memaddrs[0];
        memory_instance& mem = ctx_.store()->memorys[a];
        
        Dest c = static_cast<Dest>(ctx_.stack_pop().template as<T>());
        u32 i = ctx_.stack_pop().template as<u32>();
        u32 ea = i + offset;

        // u32 n = (ins.type == int_kind::i32) ? 4 : 8;
        u32 n = sizeof(Dest);
        u32 upper = ea + n;

        if (upper > mem.data.size()) {
            // std::cout << ea << " " << ea + n << " " << mem.data.size() << std::endl;
            throw wasm_trap("Invalid memory address");
        }

        const u8 *ptr = reinterpret_cast<const u8*>(&c);
        std::copy(ptr, ptr + n, mem.data.data() + ea);
        return {};
    }

    result_t run(const op::inn_store& ins) override {
        if (ins.type == int_kind::i32) {
            return do_store<u32, u32>(ins.offset);
        }
        else {
            return do_store<u64, u64>(ins.offset);
        }
    }

    result_t run(const op::inn_store8& ins) override {
        if (ins.type == int_kind::i32) {
            return do_store<u32, u8>(ins.offset);
        }
        else {
            return do_store<u64, u8>(ins.offset);
        }
    }

    result_t run(const op::inn_store16& ins) override {
        if (ins.type == int_kind::i32) {
            return do_store<u32, u16>(ins.offset);
        }
        else {
            return do_store<u64, u16>(ins.offset);
        }
    }

    result_t run(const op::i64_store32& ins) override {
        return do_store<u64, u32>(ins.offset);
    }
};

template <typename Context>
struct basic_executor :
        public basic_exe_parametric<Context>,
        public basic_exe_control<Context>,
        public basic_exe_variable<Context>,
        public basic_exe_numeric<Context>,
        public basic_exe_memory<Context>
{
    using basic_exe_parametric<Context>::run;
    using basic_exe_control<Context>::run;
    using basic_exe_variable<Context>::run;
    using basic_exe_numeric<Context>::run;
    using basic_exe_memory<Context>::run;

    using value_type = typename Context::value_type;
    using svalue_type = typename Context::svalue_type;
    
    using u32_type = typename Context::u32_type;
    using u64_type = typename Context::u64_type;
    using frame_type = typename Context::frame_type;
    using frame_ptr = typename svalue_type::frame_ptr;
    using frame_pointer = typename Context::frame_pointer;

    basic_executor(Context& ctx)
        : ctx_(ctx),
          basic_exe_parametric<Context>(ctx),
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
