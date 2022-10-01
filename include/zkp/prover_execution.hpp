#pragma once

#include <runtime.hpp>
#include <instruction.hpp>
#include <execution.hpp>
#include <zkp/variable.hpp>

namespace ligero::vm::zkp {

// template <typename T>
// T bit_of(T num, size_t index) {
//     return (num >> index) & static_cast<T>(1);
// }

template <typename Context>
struct zkp_exe_numeric : virtual public NumericExecutor {    
    using field = typename Context::field_type;
    using poly = typename Context::field_poly;
    using opt = typename Context::operator_type;
    
    using var = typename Context::var_type;
    
    zkp_exe_numeric(Context& ctx)
        : ctx_(ctx),
          zero(ctx_.make_var(0)),
          one(ctx_.make_var(1)),
          two(ctx_.make_var(2))
        { }

    result_t run(const op::inn_const& i) override {
        assert(i.type == int_kind::i32);

        u32 c = static_cast<u32>(i.val);
        ctx_.stack_push(var{ c, ctx_.encode(c) });
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
        
        ctx_.stack_push(ctx_.eval(x + y));
        // u32 y = ctx_.template vstack_pop<u32>();
        // u32 x = ctx_.template vstack_pop<u32>();
        // ctx_.vstack_push(x + y);

        // poly fy = ctx_.zstack_pop();
        // poly fx = ctx_.zstack_pop();
        
        // poly fz = fx + fy;
        // ctx_.zstack_pushs(std::move(fx), std::move(fy), std::move(fz));
        // ctx_.bound_linear(2, 1, 0);        // linear: x + y = z
        // ctx_.zstack_drop(2, 1);
        return {};
    }

    result_t run(const op::inn_sub& ins) override {
        assert(ins.type == int_kind::i32);

        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();
        
        ctx_.stack_push(ctx_.eval(x - y));
        // u32 y = ctx_.template vstack_pop<u32>();
        // u32 x = ctx_.template vstack_pop<u32>();
        // ctx_.vstack_push(x - y);

        // poly fy = ctx_.zstack_pop();
        // poly fx = ctx_.zstack_pop();
        // poly fz = fx - fy;
        // ctx_.zstack_pushs(fx, fy, fz);
        // ctx_.bound_linear(0, 1, 2);        // linear: z + y = x
        // ctx_.zstack_drop(2, 1);
        return {};
    }

    result_t run(const op::inn_mul& ins) override {
        assert(ins.type == int_kind::i32);

        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();
        
        ctx_.stack_push(ctx_.eval(x * y));
        // u32 y = ctx_.template vstack_pop<u32>();
        // u32 x = ctx_.template vstack_pop<u32>();
        // ctx_.vstack_push(x * y);

        // poly fy = ctx_.zstack_pop();
        // poly fx = ctx_.zstack_pop();
        // poly fz = fx * fy;
        // ctx_.zstack_pushs(fx, fy, fz);
        // ctx_.bound_quadratic(2, 1, 0);        // quadratic: x * y = z
        // ctx_.zstack_drop(2, 1);
        return {};
    }

    result_t run(const op::inn_div_sx& ins) override {
        assert(ins.type == int_kind::i32);

        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();
        
        ctx_.stack_push(ctx_.eval(x / y));
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

        var acc = zero;
        for (size_t i = 0; i < 32; i++) {
            var p = ctx_.make_var(1UL << i);
            acc = ctx_.eval(x[i] * y[i] * p + acc);
        }

        ctx_.stack_push(acc);

        // u32 y = ctx_.template vstack_pop<u32>();
        // u32 x = ctx_.template vstack_pop<u32>();
        // ctx_.vstack_push(x & y);

        // field acc = 0;
        // for (size_t i = 0; i < 32; i++) {
        //     field a = bit_of(x, i);
        //     field b = bit_of(y, i);
        //     field c = a * b;
        //     field k = 1 << i;
        //     field t = c * k;

        //     ctx_.zstack_push(a);
        //     ctx_.bound_quadratic(0, 0, 0);              // quadratic: a * a = a
        //     ctx_.zstack_push(b);
        //     ctx_.bound_quadratic(0, 0, 0);              // quadratic: b * b = b
        //     ctx_.zstack_push(c);
        //     ctx_.bound_quadratic(2, 1, 0);              // quadratic: a * b = c
        //     ctx_.zstack_push(k);
        //     ctx_.zstack_push(t);
        //     ctx_.bound_quadratic(2, 1, 0);

        //     if (i == 0) {
        //         acc = t;
        //     }
        //     else {
        //         ctx_.zstack_push(acc);
        //         acc = acc + t;
        //         ctx_.zstack_push(acc);
        //         ctx_.bound_linear(2, 1, 0);
        //     }
        // }
        
        // auto top = ctx_.zstack_pop();         // save z
        // ctx_.zstack_drop((32 * 5) + (31 * 2) - 1);
        // ctx_.zstack_pop();                    // pop y
        // ctx_.zstack_pop();                    // pop x
        // ctx_.zstack_push(std::move(top));     // push z back
        return {};
    }

    result_t run(const op::inn_or& ins) override {
        assert(ins.type == int_kind::i32);

        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var acc = zero;
        for (size_t i = 0; i < 32; i++) {
            var xi = ctx_.eval(x[i]);
            var yi = ctx_.eval(y[i]);
            var p = ctx_.make_var(1UL << i);
            acc = ctx_.eval((xi + yi - xi * yi) * p + acc);
        }

        ctx_.stack_push(acc);

        // return run_binop<typename opt::bit_or>(ins.type);
        
        // u32 y = ctx_.template vstack_pop<u32>();
        // u32 x = ctx_.template vstack_pop<u32>();
        // ctx_.vstack_push(x | y);

        /* Analytical representation: f(x) = a + b - a * b */
        // field acc = 0;
        // for (size_t i = 0; i < 32; i++) {
        //     field a = bit_of(x, i);
        //     field b = bit_of(y, i);
        //     field pa = a + b;
        //     field pb = a * b;
        //     field c = pa - pb;
        //     field k = 1 << i;
        //     field t = c * k;

        //     ctx_.zstack_push(a);
        //     ctx_.bound_quadratic(0, 0, 0);              // quadratic: a * a = a
        //     ctx_.zstack_push(b);
        //     ctx_.bound_quadratic(0, 0, 0);              // quadratic: b * b = b
            
        //     ctx_.zstack_push(pa);
        //     ctx_.bound_linear(2, 1, 0);                 // a + b = pa
            
        //     ctx_.zstack_push(pb);
        //     ctx_.bound_quadratic(3, 2, 0);              // a * b = pb
            
        //     ctx_.zstack_push(c);
        //     ctx_.bound_linear(0, 1, 2);                 // c + pb = pa
            
        //     ctx_.zstack_push(k);
        //     ctx_.zstack_push(t);
        //     ctx_.bound_quadratic(2, 1, 0);

        //     if (i == 0) {
        //         acc = t;
        //     }
        //     else {
        //         ctx_.zstack_push(acc);
        //         acc = acc + t;
        //         ctx_.zstack_push(acc);
        //         ctx_.bound_linear(2, 1, 0);
        //     }
        // }
        
        // auto top = ctx_.zstack_pop();         // save z
        // ctx_.zstack_drop((32 * 7) + (31 * 2) - 1);
        // ctx_.zstack_pop();                    // pop y
        // ctx_.zstack_pop();                    // pop x
        // ctx_.zstack_push(std::move(top));     // push z back
        return {};
    }

    result_t run(const op::inn_xor& ins) override {
        assert(ins.type == int_kind::i32);

        // return run_binop<typename opt::bit_xor>(ins.type);

        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var acc = zero;
        for (size_t i = 0; i < 32; i++) {
            var xi = ctx_.eval(x[i]);
            var yi = ctx_.eval(y[i]);
            var p = ctx_.make_var(1UL << i);
            acc = ctx_.eval((xi + yi - two * xi * yi) * p + acc);
        }

        ctx_.stack_push(acc);
        
        // u32 y = ctx_.template vstack_pop<u32>();
        // u32 x = ctx_.template vstack_pop<u32>();
        // ctx_.vstack_push(x xor y);

        /* Analytical representation: f(x) = a + b - 2ab */
        // field acc = 0;
        // for (size_t i = 0; i < 32; i++) {
        //     field a = bit_of(x, i);
        //     field b = bit_of(y, i);
        //     field p = a + b;
        //     field q = a * b;
        //     field q2 = field{2} * q;
        //     field c = p - q2;
            
        //     field k = 1 << i;
        //     field t = c * k;

        //     ctx_.zstack_push(a);
        //     ctx_.bound_quadratic(0, 0, 0);  // quadratic: a * a = a
        //     ctx_.zstack_push(b);
        //     ctx_.bound_quadratic(0, 0, 0);  // quadratic: b * b = b
            
        //     ctx_.zstack_push(p);
        //     ctx_.bound_linear(2, 1, 0);     // linear:    a + b = p
        //     /* Stack: a b p q */
        //     ctx_.zstack_push(q);
        //     ctx_.bound_quadratic(3, 2, 0);  // quadratic: a * b = q
            
        //     ctx_.zstack_push(2);
        //     ctx_.zstack_push(q2);
        //     /* Stack: a b p q 2 q2 */
        //     ctx_.bound_quadratic(2, 1, 0);  // quadratic: q * 2 = q2
            
        //     ctx_.zstack_push(c);
        //     /* Stack: a b p q 2 q2 c */
        //     ctx_.bound_linear(0, 1, 4);     // linear:    c + q2 = p
            
        //     ctx_.zstack_push(k);
        //     ctx_.zstack_push(t);
        //     ctx_.bound_quadratic(2, 1, 0);

        //     if (i == 0) {
        //         acc = t;
        //     }
        //     else {
        //         ctx_.zstack_push(acc);
        //         acc = acc + t;
        //         ctx_.zstack_push(acc);
        //         ctx_.bound_linear(2, 1, 0);
        //     }
        // }
        
        // auto top = ctx_.zstack_pop();         // save z
        // ctx_.zstack_drop(32 * 9 + 31 * 2 - 1);
        // ctx_.zstack_pop();                    // pop y
        // ctx_.zstack_pop();                    // pop x
        // ctx_.zstack_push(std::move(top));     // push z back
        return {};
    }

    result_t run(const op::inn_shl& ins) override {
        assert(ins.type == int_kind::i32);
        
        // return run_binop<typename opt::shiftl>(ins.type);

        var shift = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var exp = one;
        for (size_t i = 0; i < 5; i++) {
            var p = ctx_.make_var(1UL << (1UL << i));  // 2 ^ (2 ^ i)
            var si = ctx_.eval(shift[i]);
            exp = ctx_.eval(exp * (p * si + one - si));
        }
        
        ctx_.stack_push(ctx_.eval(x * exp));
        
        // u32 y = ctx_.template vstack_pop<u32>();
        // u32 x = ctx_.template vstack_pop<u32>();
        // u32 z = x << y;
        // ctx_.vstack_push(z);

        /* Analytic form: shiftl(x, y) = x * 2 ^ y */
        // field exp = 1;
        // field shift = 1;

        // for (size_t i = 0; i < 32; i++) {
        //     field mask = bit_of(y, i);
        //     field v = mask * exp;

        //     ctx_.zstack_push(mask);
        //     ctx_.bound_quadratic(0, 0, 0);  // check mask is a bit
            
        //     if (i > 0) {
        //         ctx_.zstack_push(exp);
        //         ctx_.zstack_push(2);
        //         exp = exp * field{2};
        //         ctx_.zstack_push(exp);
        //     }
        //     else {
        //         ctx_.zstack_push(exp);
        //     }
            
        //     ctx_.zstack_push(v);
        //     ctx_.bound_quadratic(2, 1, 0);  // mask * exp = v

        //     ctx_.zstack_push(shift);
        //     shift = shift * v;
        //     ctx_.zstack_push(shift);
        //     ctx_.bound_quadratic(2, 1, 0);  // shift = shift * v
        // }

        // auto top = ctx_.zstack_pop();
        // ctx_.zstack_drop(4 * 32 + 3 * 31 + 1 - 1);
        // ctx_.zstack_push(std::move(top));
        
        // /* At this point, the shift value is on top of the zstack */
        
        // field fx = x;
        // field fz = fx * shift;
        // fz %= 1ULL << 32;

        // ctx_.zstack_push(fx);
        // ctx_.zstack_push(fz);
        // ctx_.bound_quadratic(2, 1, 0);

        // auto result = ctx_.zstack_pop();
        // ctx_.zstack_drop(2 + 2);
        // ctx_.zstack_push(std::move(result));
        
        return {};
    }

    result_t run(const op::inn_shr_sx& ins) override {
        assert(ins.type == int_kind::i32);
        assert(ins.sign == sign_kind::unsign);

        var shift = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var exp = one;
        for (size_t i = 0; i < 5; i++) {
            var p = ctx_.make_var(1UL << (1UL << i));  // 2 ^ (2 ^ i)
            var si = ctx_.eval(shift[i]);
            exp = ctx_.eval(exp * (p * si + one - si));
        }
        
        ctx_.stack_push(ctx_.eval(x / exp));
        return {};
    }

    result_t run(const op::inn_rotl& ins) override {
        // std::cout << ins.name();
        // return run_binop<typename opt::rotatel>(ins.type);
        undefined();
        return {};
    }

    result_t run(const op::inn_rotr& ins) override {
        // std::cout << ins.name();
        // return run_binop<typename opt::rotater>(ins.type);
        undefined();
        return {};
    }

    result_t run(const op::inn_eqz& ins) override {
        assert(ins.type == int_kind::i32);

        var x = ctx_.stack_pop_var();

        var acc = one;
        for (size_t i = 0; i < 32; i++) {
            acc = ctx_.eval(acc * (one - x[i]));
        }

        ctx_.stack_push(acc);
        // if (ins.type == int_kind::i32) {
        //     u32 c = ctx_.template stack_pop<u32>();
        //     u32 r = typename opt::equal_to{}(c, u32{0});
        //     ctx_.template stack_push(r);
        // }
        // else {
        //     u64 c = ctx_.template stack_pop<u64>();
        //     u64 r = typename opt::equal_to{}(c, u64{0});
        //     ctx_.template stack_push(r);
        // }
        // std::cout << " " << x.val() << " " << acc.val() << std::endl;
        return {};
    }

    result_t run(const op::inn_eq& ins) override {
        // assert(ins.type == int_kind::i32);
        // std::cout << ins.name() << std::endl;

        var y = ctx_.stack_pop_var();
        var x = ctx_.stack_pop_var();

        var acc = one;
        var diff = ctx_.eval(x - y);
        for (size_t i = 0; i < 32; i++) {
            acc = ctx_.eval(acc * (one - diff[i]));
        }

        ctx_.stack_push(acc);
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

        var acc = one;
        var diff = ctx_.eval(x - y);
        for (size_t i = 0; i < 32; i++) {
            acc = ctx_.eval(acc * (one - diff[i]));
        }
        acc = ctx_.eval(one - acc);
        
        ctx_.stack_push(acc);

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

        var diff = ctx_.eval(y - x);
        var is_pos = ctx_.eval(one - diff[31]);

        var acc = zero;
        for (int i = 30; i >= 0; i--) {
            acc = ctx_.eval(acc + diff[i] - acc * diff[i]);
        }

        var result = ctx_.eval(is_pos * acc);
        ctx_.stack_push(result);

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

        var diff = ctx_.eval(x - y);
        var is_pos = ctx_.eval(one - diff[31]);

        var acc = zero;
        for (int i = 30; i >= 0; i--) {
            acc = ctx_.eval(acc + diff[i] - acc * diff[i]);
        }

        var result = ctx_.eval(is_pos * acc);
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

        var diff = ctx_.eval(y - x);
        var result = ctx_.eval(one - diff[31]);

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

        var diff = ctx_.eval(x - y);
        var result = ctx_.eval(one - diff[31]);

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
            // std::cout << " f(" << x << ", " << y << ") = " << result << " ";
        }
        else {
            T64 y = ctx_.template stack_pop<u64>();
            T64 x = ctx_.template stack_pop<u64>();
            u64 result = static_cast<u64>(Op{}(x, y));
            ctx_.template stack_push(result);
            // std::cout << " f(" << x << ", " << y << ") = " << result << " ";
        }
        // ctx_.show_stack();
        return {};
    }

private:
    Context& ctx_;
    const var zero, one, two;
};


template <typename Context>
struct zkp_executor :
        public basic_exe_parametric<Context>,
        public basic_exe_control<Context>,
        public basic_exe_variable<Context>,
        public zkp_exe_numeric<Context>,
        public basic_exe_memory<Context>
{
    using basic_exe_parametric<Context>::run;
    using basic_exe_control<Context>::run;
    using basic_exe_variable<Context>::run;
    using zkp_exe_numeric<Context>::run;
    using basic_exe_memory<Context>::run;

    zkp_executor(Context& ctx)
        : basic_exe_parametric<Context>(ctx),
          basic_exe_control<Context>(ctx),
          basic_exe_variable<Context>(ctx),
          zkp_exe_numeric<Context>(ctx),
          basic_exe_memory<Context>(ctx),
          ctx_(ctx)
        { }

    Context& context() { return ctx_; }

protected:
    Context& ctx_;
};

}  // namespace ligero::vm::zkp
