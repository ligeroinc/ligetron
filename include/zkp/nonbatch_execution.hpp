#pragma once

#include <runtime.hpp>
#include <instruction.hpp>
#include <execution.hpp>

namespace ligero::vm::zkp {



template <typename Context>
struct nonbatch_exe_numeric : virtual public NumericExecutor {    
    using field = typename Context::field_type;
    using poly = typename Context::field_poly;
    using opt = typename Context::operator_type;
    
    using var = typename Context::var_type;

    using u32_type = typename Context::u32_type;
    using s32_type = typename Context::s32_type;
    using u64_type = typename Context::u64_type;
    using s64_type = typename Context::s64_type;
    
    nonbatch_exe_numeric(Context& ctx) : ctx_(ctx) { }

    result_t run(const op::inn_const& i) override {
        assert(i.type == int_kind::i32);

        u32 c = static_cast<u32>(i.val);
        ctx_.stack_push(std::move(c));
        return {};
    }

    result_t run(const op::inn_clz& ins) override {
        undefined();
        return {};
    }

    result_t run(const op::inn_ctz& ins) override {
        undefined();
        return {};
    }

    result_t run(const op::inn_popcnt& ins) override {
        undefined();
        return {};
    }

    result_t run(const op::inn_add& ins) override {
        assert(ins.type == int_kind::i32);

        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();
        
        ctx_.stack_push(ctx_.eval_signed(x + y));

        // std::cout << "add ";
        // std::cout << " " << x.val() << " " << y.val() << std::endl;
        // ctx_.show_stack();
        return {};
    }

    result_t run(const op::inn_sub& ins) override {
        assert(ins.type == int_kind::i32);

        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();
        
        ctx_.stack_push(ctx_.eval_signed(x - y));

        // std::cout << "sub ";
        // std::cout << " " << x.val() << " " << y.val() << std::endl;
        // ctx_.show_stack();
        return {};
    }

    result_t run(const op::inn_mul& ins) override {
        assert(ins.type == int_kind::i32);

        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();
        
        ctx_.stack_push(ctx_.eval_signed(x * y));
        return {};
    }

    result_t run(const op::inn_div_sx& ins) override {
        assert(ins.type == int_kind::i32);

        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        if (ins.sign == sign_kind::sign) {
            ctx_.stack_push(ctx_.eval_signed(x / y));
        }
        else {
            ctx_.stack_push(ctx_.eval_unsigned(x / y));
        }
        return {};
    }

    result_t run(const op::inn_rem_sx& ins) override {
        undefined();
        return {};
    }

    result_t run(const op::inn_and& ins) override {
        assert(ins.type == int_kind::i32);
        
        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var acc = ctx_.make_var(0);
        auto dx = ctx_.template bit_decompose<32>(x);
        auto dy = ctx_.template bit_decompose<32>(y);
        for (size_t i = 0; i < 32; i++) {
            u32 idx = 1UL << i;
            var p = ctx_.make_var(idx);
            acc = ctx_.eval_unsigned(dx[i] * dy[i] * p + acc);
        }

        ctx_.stack_push(acc);

        // std::cout << "and ";
        // std::cout << " " << x.val() << " " << y.val() << " " << acc.val() << std::endl;
        // ctx_.show_stack();
        return {};
    }

    result_t run(const op::inn_or& ins) override {
        assert(ins.type == int_kind::i32);

        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var acc = ctx_.make_var(0);
        auto dx = ctx_.template bit_decompose<32>(x);
        auto dy = ctx_.template bit_decompose<32>(y);
        for (size_t i = 0; i < 32; i++) {
            var xi = dx[i];
            var yi = dy[i];
            u32 idx = 1UL << i;
            var p = ctx_.make_var(idx);
            acc = ctx_.eval_unsigned((xi + yi - xi * yi) * p + acc);
        }

        ctx_.stack_push(acc);

        // std::cout << "or ";
        // std::cout << " " << x.val() << " " << y.val() << " " << acc.val() << std::endl;
        // ctx_.show_stack();

        // return run_binop<typename opt::bit_or>(ins.type);
        return {};
    }

    result_t run(const op::inn_xor& ins) override {
        assert(ins.type == int_kind::i32);

        // return run_binop<typename opt::bit_xor>(ins.type);

        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var acc = ctx_.make_var(0);
        var two = ctx_.make_var(2);
        auto dx = ctx_.template bit_decompose<32>(x);
        auto dy = ctx_.template bit_decompose<32>(y);
        for (size_t i = 0; i < 32; i++) {
            var xi = dx[i];
            var yi = dy[i];
            u32 idx = 1UL << i;
            var p = ctx_.make_var(idx);
            acc = ctx_.eval_unsigned((xi + yi - two * xi * yi) * p + acc);
        }

        ctx_.stack_push(acc);
        return {};
    }

    result_t run(const op::inn_shl& ins) override {
        assert(ins.type == int_kind::i32);
        
        // return run_binop<typename opt::shiftl>(ins.type);

        var shift = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        // var exp = one;
        // for (size_t i = 0; i < 5; i++) {
        //     u32 idx = 1UL << (1UL << i);  // 2 ^ (2 ^ i)
        //     var p{ idx, ctx_.encode_const(idx) };
        //     var si = ctx_.eval(shift[i]);
        //     exp = ctx_.eval(exp * (p * si + one - si));
        // }
        
        // ctx_.stack_push(ctx_.eval(x * exp));

        // Assume shift value is public
        var exp = ctx_.make_var(1);
        var two = ctx_.make_var(2);
        for (size_t i = 0; i < shift.val(); i++) {
            exp = ctx_.eval_unsigned(exp * two);
        }

        ctx_.stack_push(ctx_.eval_unsigned(x * exp));

        // std::cout << "shiftl ";
        // std::cout << " " << x.val() << " " << shift.val();
        // ctx_.show_stack();
        
        return {};
    }

    result_t run(const op::inn_shr_sx& ins) override {
        assert(ins.type == int_kind::i32);
        assert(ins.sign == sign_kind::unsign);

        var shift = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var exp = ctx_.make_var(1);
        var two = ctx_.make_var(2);
        for (size_t i = 0; i < shift.val(); i++) {
            exp = ctx_.eval_unsigned(exp * two);
        }

        var result;
        if (ins.sign == sign_kind::sign) {
            result = ctx_.eval_signed(x / exp);
        }
        else {
            result = ctx_.eval_unsigned(x / exp);
        }
        
        ctx_.stack_push(result);

        // var exp = one;
        // for (size_t i = 0; i < 5; i++) {
        //     u32 idx = 1UL << (1UL << i);  // 2 ^ (2 ^ i)
        //     var p{ idx, ctx_.encode_const(idx) };
        //     var si = ctx_.eval(shift[i]);
        //     exp = ctx_.eval(exp * (p * si + one - si));
        // }
        
        // ctx_.stack_push(ctx_.eval(x / exp));
        return {};
    }

    result_t run(const op::inn_rotl& ins) override {
        // std::cout << ins.name();
        // return run_binop<typename opt::rotatel>(ins.type);

        var shift = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var exp_left = ctx_.make_var(1);
        var exp_right = ctx_.make_var(1);
        var two = ctx_.make_var(2);
        for (size_t i = 0; i < shift.val(); i++) {
            exp_left = ctx_.eval_unsigned(exp_left * two);
        }

        for (size_t i = 0; i < 32 - shift.val(); i++) {
            exp_right = ctx_.eval_unsigned(exp_right * two);
        }

        ctx_.stack_push(ctx_.eval_unsigned(x * exp_left + x / exp_right));
        
        return {};
    }

    result_t run(const op::inn_rotr& ins) override {
        // std::cout << ins.name();
        // return run_binop<typename opt::rotater>(ins.type);

        var shift = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var exp_right = ctx_.make_var(1);
        var exp_left = ctx_.make_var(1);
        var two = ctx_.make_var(2);
        for (size_t i = 0; i < shift.val(); i++) {
            exp_right = ctx_.eval_unsigned(exp_right * two);
        }

        for (size_t i = 0; i < 32 - shift.val(); i++) {
            exp_left = ctx_.eval_unsigned(exp_left * two);
        }

        ctx_.stack_push(ctx_.eval_unsigned(x / exp_right + x * exp_left));
        
        return {};
    }

    result_t run(const op::inn_eqz& ins) override {
        assert(ins.type == int_kind::i32);

        var x = ctx_.stack_pop_var();

        var one = ctx_.make_var(1);
        var acc = ctx_.make_var(1);
        auto dx = ctx_.template bit_decompose<32>(x);
        for (size_t i = 0; i < 32; i++) {
            acc = ctx_.eval_unsigned(acc * (one - dx[i]));
        }

        ctx_.stack_push(acc);

        // std::cout << "eqz ";
        // std::cout << " " << x.val() << " " << acc.val() << std::endl;
        return {};
    }

    result_t run(const op::inn_eq& ins) override {
        // assert(ins.type == int_kind::i32);
        // std::cout << ins.name() << std::endl;

        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var one = ctx_.make_var(1);
        var acc = ctx_.make_var(1);        
        var diff = ctx_.eval_unsigned(x - y);
        auto ddiff = ctx_.template bit_decompose<32>(diff);
        for (size_t i = 0; i < 32; i++) {
            acc = ctx_.eval_unsigned(acc * (one - ddiff[i]));
        }

        ctx_.stack_push(acc);

        // std::cout << "eq ";
        // std::cout << " " << x.val() << " " << y.val() << " " << acc.val() << std::endl;
        
        // std::cout << ins.name();
        // return run_binop<typename opt::equal_to>(ins.type);

        return {};
    }

    result_t run(const op::inn_ne& ins) override {
        // assert(ins.type == int_kind::i32);
        // std::cout << ins.name() << std::endl;

        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var one = ctx_.make_var(1);
        var acc = ctx_.make_var(1);
        var diff = ctx_.eval_unsigned(x - y);
        auto ddiff = ctx_.template bit_decompose<32>(diff);
        for (size_t i = 0; i < 32; i++) {
            acc = ctx_.eval_unsigned(acc * (one - ddiff[i]));
        }
        acc = ctx_.eval_unsigned(one - acc);
        
        ctx_.stack_push(acc);

        // std::cout << "ne ";
        // std::cout << " " << x.val() << " " << y.val() << " " << acc.val() << std::endl;
        
        
        // std::cout << ins.name();
        // ctx_.show_stack();
        // return run_binop<typename opt::not_equal_to>(ins.type);
        return {};
    }

    result_t run(const op::inn_lt_sx& ins) override {
        // std::cout << ins.name() << std::endl;
        // assert(ins.type == int_kind::i32);
        
        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var diff = ctx_.eval_signed(y - x);
        auto ddiff = ctx_.template bit_decompose<32>(diff);
        var is_pos = ctx_.eval_signed(ctx_.make_var(1) - ddiff[31]);

        var acc = ctx_.make_var(0);
        for (int i = 30; i >= 0; i--) {
            acc = ctx_.eval_signed(acc + ddiff[i] - acc * ddiff[i]);
        }

        var result = ctx_.eval_signed(is_pos * acc);
        ctx_.stack_push(result);

        // std::cout << "lt_sx ";
        // std::cout << " " << x.val() << " " << y.val() << " " << result.val() << std::endl;
        
        
        // if (ins.sign == sign_kind::sign) {
        //     return run_binop<typename opt::less, s32, s64>(ins.type);
        // }
        // else {
        //     return run_binop<typename opt::less>(ins.type);
        // }
        return {};
    }

    result_t run(const op::inn_gt_sx& ins) override {
        // std::cout << ins.name();
        // if (ins.sign == sign_kind::sign) {
        //     return run_binop<typename opt::greater, s32, s64>(ins.type);
        // }
        // else {
        //     return run_binop<typename opt::greater>(ins.type);
        // }
        assert(ins.type == int_kind::i32);
        
        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var diff = ctx_.eval_signed(x - y);

        auto ddiff = ctx_.template bit_decompose<32>(diff);
        var is_pos = ctx_.eval_signed(ctx_.make_var(1) - ddiff[31]);

        var acc = ctx_.make_var(0);
        for (int i = 30; i >= 0; i--) {
            acc = ctx_.eval_signed(acc + ddiff[i] - acc * ddiff[i]);
        }

        var result = ctx_.eval_signed(is_pos * acc);
        ctx_.stack_push(result);
        return {};
    }

    result_t run(const op::inn_le_sx& ins) override {
        // std::cout << ins.name();
        // if (ins.sign == sign_kind::sign) {
        //     return run_binop<typename opt::less_equal, s32, s64>(ins.type);
        // }
        // else {
        //     return run_binop<typename opt::less_equal>(ins.type);
        // }
        assert(ins.type == int_kind::i32);
        
        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var diff = ctx_.eval_signed(y - x);
        auto ddiff = ctx_.template bit_decompose<32>(diff);
        var result = ctx_.eval_signed(ctx_.make_var(1) - ddiff[31]);

        ctx_.stack_push(result);
        return {};
    }

    result_t run(const op::inn_ge_sx& ins) override {
        // std::cout << ins.name();
        // if (ins.sign == sign_kind::sign) {
        //     return run_binop<typename opt::greater_equal, s32, s64>(ins.type);
        // }
        // else {
        //     return run_binop<typename opt::greater_equal>(ins.type);
        // }
        assert(ins.type == int_kind::i32);
        
        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var diff = ctx_.eval_signed(x - y);
        auto ddiff = ctx_.template bit_decompose<32>(diff);
        var result = ctx_.eval_signed(ctx_.make_var(1) - ddiff[31]);

        ctx_.stack_push(result);
        return {};
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
        // if (ins.sign == sign_kind::sign) {
        //     u32 sv = ctx_.template stack_pop<u32>();
        //     u64 c = static_cast<u64>(static_cast<s64>(static_cast<s32>(sv)));
        //     ctx_.template stack_push(c);
        // }
        // else {
        //     u32 sv = ctx_.template stack_pop<u32>();
        //     u64 c = sv;
        //     ctx_.template stack_push(c);
        // }
        undefined(ins);
        return {};
    }

    result_t run(const op::i32_wrap_i64& ins) override {
        undefined(ins);
        // u64 sv = ctx_.template stack_pop<u64>();
        // ctx_.template stack_push(static_cast<u32>(sv));
        return {};
    }

private:
    Context& ctx_;
};


template <typename Context>
class nonbatch_exe_variable : virtual public VariableExecutor {
    Context& ctx_;

public:
    nonbatch_exe_variable(Context& ctx) : ctx_(ctx) { }

    result_t run(const op::local_get& var) override {
        index_t x = var.local;
        ctx_.stack_push(ctx_.current_frame()->refs[x]);
        return {};
    }

    result_t run(const op::local_set& var) override {
        index_t x = var.local;
        ctx_.current_frame()->refs[x] = ctx_.stack_pop_var();

        // std::cout << "local.set[" << x << "]" << std::endl;
        // ctx_.show_stack();
        return {};
    }

    result_t run(const op::local_tee& var) override {
        index_t x = var.local;
        ctx_.current_frame()->refs[x] = ctx_.stack_peek_var();
        
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
        return {};
    }
};


template <typename Context>
struct nonbatch_executor :
        public basic_exe_parametric<Context>,
        public basic_exe_control<Context>,
        public nonbatch_exe_variable<Context>,
        public nonbatch_exe_numeric<Context>,
        public basic_exe_memory<Context>
{
    using basic_exe_parametric<Context>::run;
    using basic_exe_control<Context>::run;
    using nonbatch_exe_variable<Context>::run;
    using nonbatch_exe_numeric<Context>::run;
    using basic_exe_memory<Context>::run;

    using value_type = typename Context::value_type;
    using svalue_type = typename Context::svalue_type;
    
    using u32_type = typename Context::u32_type;
    using s32_type = typename Context::s32_type;
    using u64_type = typename Context::u64_type;
    using s64_type = typename Context::s64_type;

    using label_type = typename Context::label_type;
    using frame_type = typename Context::frame_type;
    using frame_pointer = typename Context::frame_pointer;

    nonbatch_executor(Context& ctx)
        : basic_exe_parametric<Context>(ctx),
          basic_exe_control<Context>(ctx),
          nonbatch_exe_variable<Context>(ctx),
          nonbatch_exe_numeric<Context>(ctx),
          basic_exe_memory<Context>(ctx),
          ctx_(ctx)
        { }

    Context& context() { return ctx_; }

protected:
    Context& ctx_;
};

}  // namespace ligero::vm::zkp
