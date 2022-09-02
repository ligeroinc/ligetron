#pragma once

#include <runtime.hpp>
#include <instruction.hpp>
#include <execution.hpp>

namespace ligero::vm::zkp {

template <typename T>
T bit_of(T num, size_t index) {
    return (num >> index) & static_cast<T>(1);
}


template <typename Context>
class zkp_exe_numeric : virtual public NumericExecutor {
    Context& ctx_;

public:
    using field = typename Context::field_type;
    using opt = typename Context::operator_type;
    
    zkp_exe_numeric(Context& ctx) : ctx_(ctx) { }

    result_t run(const op::inn_const& i) override {
        assert(i.type == int_kind::i32);
        std::cout << "const" << std::endl;

        ctx_.vstack_push(static_cast<u32>(i.val));
        field k = static_cast<s32>(i.val);
        ctx_.zstack_push(k);
        // if (i.type == int_kind::i32) {
        //     ctx_.template stack_push(static_cast<u32>(i.val));
        // }
        // else {
        //     ctx_.template stack_push(i.val);
        // }
        // return {};
    }

    result_t run(const op::inn_clz& ins) override {
        undefined();
    }

    result_t run(const op::inn_ctz& ins) override {
        undefined();
    }

    result_t run(const op::inn_popcnt& ins) override {
        undefined();
    }

    result_t run(const op::inn_add& ins) override {
        assert(ins.type == int_kind::i32);
        std::cout << ins.name() << std::endl;

        std::cout << ">>> Add <<< " << std::endl;
        ctx_.show_stack();
        
        u32 y = ctx_.template vstack_pop<u32>();
        u32 x = ctx_.template vstack_pop<u32>();
        ctx_.vstack_push(x + y);

        field fx = static_cast<s32>(x);
        field fy = static_cast<s32>(y);
        field fz = fx + fy;
        ctx_.zstack_push(fz);
        ctx_.bound_linear(2, 1, 0);        // linear: x + y = z
        auto top = ctx_.zstack_pop();      // save z
        ctx_.zstack_pop();                 // pop y
        ctx_.zstack_pop();                 // pop x
        ctx_.zstack_push(std::move(top));  // push z back
        return {};
    }

    result_t run(const op::inn_sub& ins) override {
        assert(ins.type == int_kind::i32);
        u32 y = ctx_.template vstack_pop<u32>();
        u32 x = ctx_.template vstack_pop<u32>();
        ctx_.vstack_push(x - y);

        field fx = static_cast<s32>(x);
        field fy = static_cast<s32>(y);
        field fz = fx - fy;
        ctx_.zstack_push(fz);
        ctx_.bound_linear(0, 1, 2);        // linear: z + y = x
        auto top = ctx_.zstack_pop();      // save z
        ctx_.zstack_pop();                 // pop y
        ctx_.zstack_pop();                 // pop x
        ctx_.zstack_push(std::move(top));  // push z back
    }

    result_t run(const op::inn_mul& ins) override {
        assert(ins.type == int_kind::i32);
        u32 y = ctx_.template vstack_pop<u32>();
        u32 x = ctx_.template vstack_pop<u32>();
        ctx_.vstack_push(x * y);

        field fx = static_cast<s32>(x);
        field fy = static_cast<s32>(y);
        field fz = fx * fy;
        ctx_.zstack_push(fz);
        ctx_.bound_quadratic(2, 1, 0);        // quadratic: x * y = z
        auto top = ctx_.zstack_pop();         // save z
        ctx_.zstack_pop();                    // pop y
        ctx_.zstack_pop();                    // pop x
        ctx_.zstack_push(std::move(top));     // push z back
    }

    result_t run(const op::inn_div_sx& ins) override {
        undefined();
    }

    result_t run(const op::inn_rem_sx& ins) override {
        undefined();
    }

    result_t run(const op::inn_and& ins) override {
        assert(ins.type == int_kind::i32);
        std::cout << ins.name() << std::endl;
        
        u32 y = ctx_.template vstack_pop<u32>();
        u32 x = ctx_.template vstack_pop<u32>();
        ctx_.vstack_push(x & y);

        field acc = 0;
        for (size_t i = 0; i < 32; i++) {
            field a = bit_of(x, i);
            field b = bit_of(y, i);
            field c = a * b;
            field k = 1 << i;
            field t = c * k;

            ctx_.zstack_push(a);
            ctx_.bound_boolean(0);              // quadratic: a * a = a
            ctx_.zstack_push(b);
            ctx_.bound_boolean(0);              // quadratic: b * b = b
            ctx_.zstack_push(c);
            ctx_.bound_quadratic(2, 1, 0);      // quadratic: a * b = c
            ctx_.zstack_push(k);
            ctx_.zstack_push(t);
            ctx_.bound_quadratic(2, 1, 0);

            if (i == 0) {
                acc = t;
            }
            else {
                ctx_.zstack_push(acc);
                acc = acc + t;
                ctx_.zstack_push(acc);
                ctx_.bound_linear(2, 1, 0);
            }
        }
        
        auto top = ctx_.zstack_pop();         // save z
        for (size_t i = 0; i < (32 * 5) + (31 * 2) - 1; i++) {
            ctx_.zstack_pop();
        }
        ctx_.zstack_pop();                    // pop y
        ctx_.zstack_pop();                    // pop x
        ctx_.zstack_push(std::move(top));     // push z back
    }

    result_t run(const op::inn_or& ins) override {
        undefined();
    }

    result_t run(const op::inn_xor& ins) override {
        return run_binop<typename opt::bit_xor>(ins.type);
        // assert(ins.type == int_kind::i32);
        // u32 y = ctx_.template vstack_pop<u32>();
        // u32 x = ctx_.template vstack_pop<u32>();
        // ctx_.vstack_push(x xor y);

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
        //     ctx_.zstack_push(q);
        //     ctx_.bound_quadratic(3, 2, 0);  // quadratic: a * b = q
        //     ctx_.zstack_push(2);
        //     ctx_.zstack_push(q2);
        //     ctx_.bound_quadratic(2, 1, 0);  // quadratic: q * 2 = q2
        //     ctx_.zstack_push(c);
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
        // ctx_.zstack_drop(32 * 9 + 31 * 2);
        // ctx_.zstack_push(std::move(top));     // push z back
    }

    result_t run(const op::inn_shl& ins) override {
        assert(ins.type == int_kind::i32);
        std::cout << ins.name() << std::endl;

        return run_binop<typename opt::shiftl>(ins.type);
        
        // u32 y = ctx_.template vstack_pop<u32>();
        // u32 x = ctx_.template vstack_pop<u32>();
        // u32 z = x << y;
        // ctx_.vstack_push(z);

        // field fz = z;
        // field acc = 0;
        // for (size_t i = 0; i < 32; i++) {
        //     field a = bit_of(x, i);
        //     field b = bit_of(y, i);
        //     field c = bit_of(z, i);
        //     field n = 1 << i;
        //     field t = c * n;

        //     ctx_.zstack_push(a);
        //     ctx_.bound_quadratic(0, 0, 0);
        //     ctx_.zstack_push(b);
        //     ctx_.bound_quadratic(0, 0, 0);
        //     ctx_.zstack_push(c);
        //     ctx_.bound_quadratic(0, 0, 0);

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
struct zkp_executor :
        public basic_exe_control<Context>,
        public basic_exe_variable<Context>,
        public zkp_exe_numeric<Context>,
        public basic_exe_memory<Context>
{
    using basic_exe_control<Context>::run;
    using basic_exe_variable<Context>::run;
    using zkp_exe_numeric<Context>::run;
    using basic_exe_memory<Context>::run;

    zkp_executor(Context& ctx)
        : ctx_(ctx),
          basic_exe_control<Context>(ctx),
          basic_exe_variable<Context>(ctx),
          zkp_exe_numeric<Context>(ctx),
          basic_exe_memory<Context>(ctx)
        { }

    Context& context() { return ctx_; }

protected:
    Context& ctx_;
};

}  // namespace ligero::vm::zkp
