#pragma once

#include <concepts>

#include <instruction.hpp>
#include <base.hpp>
#include <zkp/prime_field.hpp>
#include <zkp/encoding.hpp>
#include <zkp/merkle_tree.hpp>

namespace ligero::vm::zkp {

template <typename T>
T bit_of(T num, size_t index) {
    return (num >> index) & static_cast<T>(1);
}

// template <template <typename> typename Prover, typename Fp>
// concept IsProver = requires(Prover<Fp> prover, Fp field) {
//     { prover.push_linear(field, field, field) };
//     { prover.push_quadratic(field, field, field) };
// };

template <typename Row>
concept WitnessRow = requires(Row row, u32 val) {
    { row = val };
    { row + row } -> std::convertible_to<Row>;
    { row - row } -> std::convertible_to<Row>;
    { row * row } -> std::convertible_to<Row>;
    { row / row } -> std::convertible_to<Row>;
};


template <typename Derive, WitnessRow Fp>
struct prover_extension : virtual NumericExecutor {
    using row_type = Fp;
    
    prover_extension() : derive_(static_cast<Derive&>(*this)) { }
    ~prover_extension() = default;

    result_t run(const op::inn_const& i) override {
        row_type r = i.val;
        derive_.push_witness(r);
        return {};
    }

    /* Unary operators */
    /* -------------------------------------------------- */
    
    void push_cs(const op::inn_clz&, field x, field z) {
        undefined();
    }
    
    void push_cs(const op::inn_ctz&, field x, field z) {
        undefined();
    }
    
    void push_cs(const op::inn_popcnt&, field x, field z) {
        undefined();
    }

    /* Binary operators */
    /* -------------------------------------------------- */
    result_t run(const op::inn_add& i) {
        row_type x = derive_.pop_witness()
        field z = x + y;
        derive_.push_linear(x, y, z);
    }

    void push_cs(const op::inn_sub&, field x, field y) {
        field z = x - y;
        derive_.push_linear(z, y, x);
    }

    void push_cs(const op::inn_mul&, field x, field y) {
        field z = x * y;
        derive_.push_quadratic(x, y, z);
    }

    void push_cs(const op::inn_div_sx& ins, field x, field y) {
        field q, r;
        x.divide_qr(y, q, r);  // x = q * y + r

        field m = q * y;
        derive_.push_quadratic(q, y, m);

        field n = m + r;
        derive_.push_linear(m, r, n);

        derive_.push_linear(m, n, x);
    }

    void push_cs(const op::inn_rem_sx& ins, field x, field y) {
        field q, r;
        x.divide_qr(y, q, r);  // x = q * y + r

        field m = q * y;
        derive_.push_quadratic(q, y, m);

        field n = m + r;
        derive_.push_linear(m, r, n);

        derive_.push_linear(m, n, x);
    }

    void push_cs(const op::inn_and& ins, field x, field y) {
        size_t bits = ins.type == int_kind::i32 ? 32 : 64;

        /* Bit decomposition */
        for (int i = 0; i < bits; i++) {
            /* f(a, b) = a * b */
            field a = bit_of(x, i);
            derive_.push_quadratic(a, a, a);
            
            field b = bit_of(y, i);
            derive_.push_quadratic(b, b, b);

            field c = a * b;
            derive_.push_quadratic(a, b, c);
        }
    }

    void push_cs(const op::inn_or& ins, field x, field y) {
        size_t bits = ins.type == int_kind::i32 ? 32 : 64;

        /* Bit decomposition */
        for (int i = 0; i < bits; i++) {
            /* f(a, b) = a + b */
            field a = bit_of(x, i);
            derive_.push_quadratic(a, a, a);
            
            field b = bit_of(y, i);
            derive_.push_quadratic(b, b, b);
            
            field c = a + b;
            derive_.push_linear(a, b, c);
        }
    }

    void push_cs(const op::inn_xor& ins, field x, field y) {
        size_t bits = ins.type == int_kind::i32 ? 32 : 64;

        /* Bit decomposition */
        for (int i = 0; i < bits; i++) {
            field a = bit_of(x, i);
            derive_.push_quadratic(a, a, a);
            
            field b = bit_of(y, i);
            derive_.push_quadratic(b, b, b);
            
            field p = a + b;
            derive_.push_linear(a, b, p);
            
            field q = a * b;
            derive_.push_quadratic(a, b, q);
            
            field q2 = 2 * q;
            derive_.push_quadratic(2, q, q2);

            field c = p - q2;
            derive_.push_linear(c, q2, p);
        }
    }

    void push_cs(const op::inn_shl&, field x, field y) {
        // TODO: bit decomposition, proof bit=0/1, proof sum of bit=x, proof shift=y
        undefined();
    }

    void push_cs(const op::inn_shr_sx&, field x, field y) {
        // TODO: bit decomposition, proof bit=0/1, proof sum of bit=x, proof shift=y
        undefined();
    }

    void push_cs(const op::inn_rotl&, field x, field y) {
        // TODO: bit decomposition, proof bit=0/1, proof sum of bit=x, proof shift=y
        undefined();
    }

    void push_cs(const op::inn_rotr&, field x, field y) {
        // TODO: bit decomposition, proof bit=0/1, proof sum of bit=x, proof shift=y
        undefined();
    }

    void push_cs(const op::inn_eqz& ins, field x) {
        size_t bits = ins.type == int_kind::i32 ? 32 : 64;

        derive_.push_linear(x, 0, 0);
        // field acc = 1;
        // for (int i = 0; i < bits; i++) {
        //     field a = bit_of(x, i);
        //     derive_.push_quadratic(a, a, a);

        //     field not_a = 1 - a;
        //     derive_.push_linear(not_a, a, 1);

        //     field acc_new = acc * not_a;
        //     derive_.push_quadratic(acc, not_a, acc_new);

        //     acc = acc_new;
        // }
        // derive_.push_quadratic(acc, acc, acc);
    }

    void push_cs(const op::inn_eq&, field x, field y) {
        field z = x - y;
        derive_.push_linear(z, y, x);
    }

    void push_cs(const op::inn_ne& ins, field x, field y) {
        size_t bits = ins.type == int_kind::i32 ? 32 : 64;

        field acc = 0;
        for (int i = 0; i < bits; i++) {
            field a = bit_of(x, i);
            field b = bit_of(y, i);

            derive_.push_quadratic(a, a, a);
            derive_.push_quadratic(b, b, b);

            /* a `xor` b */
            field p = a + b;
            derive_.push_linear(a, b, p);
            
            field q = a * b;
            derive_.push_quadratic(a, b, q);
            
            field q2 = 2 * q;
            derive_.push_quadratic(2, q, q2);

            field c = p - q2;
            derive_.push_linear(c, q2, p);

            field z = acc + c;
            derive_.push_linear(acc, c, z);
        }
    }

    // TODO: fix constraint
    void push_cs(const op::inn_lt_sx& ins, field x, field y) {
        field z = x - y;
        derive_.push_cs(z, y, x);
    }

    void push_cs(const op::inn_gt_sx& ins, field x, field y) {
        field z = x - y;
        derive_.push_cs(z, y, x);
    }

    void push_cs(const op::inn_le_sx& ins, field x, field y) {
        field z = x - y;
        derive_.push_cs(z, y, x);
    }

    void push_cs(const op::inn_ge_sx& ins, field x, field y) {
        field z = x - y;
        derive_.push_cs(z, y, x);
    }

protected:
    Derive& derive_;
};

/* ------------------------------------------------------------ */
template <typename Poly>
struct constraint_buffer {
    using value_type = typename Poly::value_type;
    
    constraint_buffer(size_t size) : size_(size), a_(size), b_(size), c_(size), ptr_(0) { }

    bool push_back(const value_type& x, const value_type& y, const value_type& z) {
        a_[ptr_] = x;
        b_[ptr_] = y;
        c_[ptr_] = z;
        ptr_++;
        return ptr_ == size_;  // Check if full
    }

    void clear() {
        a_.clear();
        b_.clear();
        c_.clear();
        ptr_ = 0;
    }

    const Poly& a() const { return a_; }
    const Poly& b() const { return b_; }
    const Poly& c() const { return c_; }

    const size_t size_;
    Poly a_, b_, c_;
    size_t ptr_;
};

template <typename Fp,
          typename LinearEncoder = reed_solomon64,
          typename QuadraticEncoder = reed_solomon64,
          typename HashScheme = sha256>
struct stage1_prover : prover_extension<stage1_prover<Fp, LinearEncoder, QuadraticEncoder>, Fp>
{
    using field_type = prime_field<Fp>;
    using poly_type = primitive_poly<Fp::modulus>;
    using pval_t = typename poly_type::value_type;

    stage1_prover(LinearEncoder& le, QuadraticEncoder& qe)
        : linear_code_(le.plain_size()),
          quadratic_code_(le.plain_size()),
          linear_encoder_(le), quadratic_encoder_(qe),
          mkb_(le.encoded_size())
        { }
    
    void push_linear(field_type a, field_type b, field_type c) {
        if (linear_code_.push_back(a, b, c)) {
            linear_encoder_.encode(linear_code_.a());
            linear_encoder_.encode(linear_code_.b());
            linear_encoder_.encode(linear_code_.c());

            mkb_ << linear_code_.a()
                 << linear_code_.b()
                 << linear_code_.c();
            linear_code_.clear();
        }
    }

    void push_quadratic(field_type a, field_type b, field_type c) {
        if (quadratic_code_.push_back(a, b, c)) {
            quadratic_encoder_.encode(quadratic_code_.a());
            quadratic_encoder_.encode(quadratic_code_.b());
            quadratic_encoder_.encode(quadratic_code_.c());

            mkb_ << quadratic_code_.a()
                 << quadratic_code_.b()
                 << quadratic_code_.c();
            quadratic_code_.clear();
        }
    }

    LinearEncoder& linear_encoder_;
    QuadraticEncoder& quadratic_encoder_;
    constraint_buffer<poly_type> linear_code_;
    constraint_buffer<poly_type> quadratic_code_;
    typename merkle_tree<HashScheme>::builder mkb_;
};

reed_solomon64 rs(123, 8, 16, 32);
reed_solomon64 rss(123, 8, 16, 32);
stage1_prover<Fp64<123>> stg1{rs, rss};

}  // namespace ligero::vm::zkp
