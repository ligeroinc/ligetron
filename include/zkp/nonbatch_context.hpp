#pragma once

#include <unordered_map>
#include <array>

#include <context.hpp>
#include <zkp/encoding.hpp>
#include <zkp/merkle_tree.hpp>
#include <zkp/argument.hpp>
#include <zkp/variable.hpp>
#include <zkp/gc.hpp>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>

namespace ligero::vm::zkp {

// template <typename Poly>
// void print(Poly& p) {
//     std::cout << "poly: ";
//     for (const auto& v : p.data()) {
//         std::cout << v << " ";
//     }
//     std::cout << std::endl;
// }

struct zero_dist {
    static constexpr bool enabled = false;
    
    auto operator()() const noexcept { return 0; }
};

struct one_dist {
    using seed_type = hash_random_engine<sha256>;
    static constexpr bool enabled = true;

    template <typename... Args>
    one_dist(Args&&...) { }
    
    auto operator()() const noexcept { return 1; }
};

template <typename T, typename Engine = hash_random_engine<sha256>>
struct hash_random_dist {
    using seed_type = typename Engine::seed_type;
    static constexpr bool enabled = true;

    hash_random_dist(T modulus, const typename Engine::seed_type& seed)
        : engine_(seed), dist_(T{0}, modulus - T{1}) { }

    T operator()() {
        // auto t = make_timer("Random", "HashDistribution");
        return dist_(engine_);
        // return 0ULL;
    }

    Engine engine_;
    random::uniform_int_distribution<T> dist_;
};

template <typename T = uint64_t>
struct aes256ctr {
    static constexpr bool enabled = true;
    
    template <typename Key, typename IV>
    aes256ctr(T modulus, Key& key, IV& iv)
        : engine_(key, iv), dist_(T{0}, modulus - T{1}) { }

    T operator()() {
        return dist_(engine_);
    }

protected:
    aes256ctr_engine<T> engine_;
    std::uniform_int_distribution<T> dist_;
};

struct random_seeds {
    unsigned int linear;
    unsigned int quadratic_left, quadratic_right, quadratic_out;

    unsigned int rand_linear;
    unsigned int rand_quad_left, rand_quad_right, rand_quad_out;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int) {
        // NB: Only send the random seed used to encode randomness row
        //     since the verifier only need to encode random rows again
        ar & rand_linear;
        ar & rand_quad_left;
        ar & rand_quad_right;
        ar & rand_quad_out;
    }
};

template <typename LocalValue,
          typename StackValue,
          typename Fp,
          typename RandomDist>
struct nonbatch_context_base : public context_base<LocalValue, StackValue> {
    using Base = context_base<LocalValue, StackValue>;
    using field_poly = Fp;
    using field_type = typename Fp::field_type;
    using operator_type = standard_op;

    using value_type = typename Base::value_type;
    using svalue_type = typename Base::svalue_type;
    
    using u32_type = typename Base::u32_type;
    using s32_type = typename Base::s32_type;
    using u64_type = typename Base::u64_type;
    using s64_type = typename Base::s64_type;

    using label_type = typename Base::label_type;
    using frame_type = typename Base::frame_type;
    using frame_pointer = typename Base::frame_pointer;

    using region_type = gc_managed_region<field_poly, RandomDist>;
    using row_type = typename region_type::row_type;
    using var_type = typename region_type::ref_type;

    nonbatch_context_base(reed_solomon64& encoder, random_seeds seeds, RandomDist&& dist)
        : encoder_(encoder),
          dist_(std::move(dist)),
          region_(encoder.plain_size(), dist_),
          seeds_(seeds)
        {
            region_.on_linear([&](auto& row) { return on_linear_full(row); });
            region_.on_quadratic([&](auto&... rows) { return on_quadratic_full(rows...); });
            
            linear_rand_.seed(seeds.linear);
            ql_rand_.seed(seeds.quadratic_left);
            qr_rand_.seed(seeds.quadratic_right);
            qo_rand_.seed(seeds.quadratic_out);

            random_linear_rand_.seed(seeds.rand_linear);
            random_ql_rand_.seed(seeds.rand_quad_left);
            random_qr_rand_.seed(seeds.rand_quad_right);
            random_qo_rand_.seed(seeds.rand_quad_out);
        }
    
    virtual ~nonbatch_context_base() = default;

    void stack_push(svalue_type&& val) override {
        std::visit(prelude::overloaded{
                [this](const value_type& v) { zstack_push(v.template as<u32_type>()); },
                // [this](u64_type& v) { zstack_push(v); },
                [](const auto&) { }
            }, val.data());
        Base::stack_push(std::move(val));
    }

    svalue_type stack_pop() override {
        svalue_type val = Base::stack_pop();
        std::visit(prelude::overloaded{
                [this](const value_type&) { zstack_pop(); },
                // [this](const u64_type&) { zstack_pop(); },
                [](const auto&) { }
            }, val.data());
        return val;
    }

    void drop_n_below(size_t n, size_t pos = 0) override {
        constexpr auto is_value = [&](const svalue_type& v) {
            return std::holds_alternative<value_type>(v.data());
            // return std::holds_alternative<u32_type>(v) || std::holds_alternative<u64_type>(v);
        };
        size_t zpos = std::count_if(this->stack_.rbegin(),
                                    this->stack_.rbegin() + pos,
                                    is_value);
        size_t zdrop = std::count_if(this->stack_.rbegin() + pos,
                                     this->stack_.rbegin() + pos + n,
                                     is_value);
        Base::drop_n_below(n, pos);

        auto zit = zstack_.rbegin() + zpos;
        zstack_.erase((zit + zdrop).base(), zit.base());
    }

    void push_arguments(frame_pointer fp, u32 n) override {
        std::vector<var_type> arguments;
        for (size_t i = 0; i < n; i++) {
            arguments.emplace_back(stack_pop_var());
        }
        std::reverse(arguments.begin(), arguments.end());
        fp->refs = std::move(arguments);
    }

    void push_local(frame_pointer fp, value_kind k) override {
        fp->refs.emplace_back(make_var(u32{0}));
    }

    void stack_push(var_type ref) {
        u32 val = static_cast<u32>(ref.val());
        Base::stack_push(val);
        zstack_.push_back(std::move(ref));
    }

    var_type stack_pop_var() {
        Base::stack_pop();
        return zstack_pop();
    }

    var_type stack_peek_var() {
        assert(!zstack_.empty());
        return zstack_.back();
    }

    var_type make_var(s32 v) {
        return region_.push_back_linear(v);
    }

    template <size_t NumBits>
    auto bit_decompose(var_type v) {
        std::array<var_type, NumBits> bits;
        std::vector<std::pair<field_type, field_type>> randomness;

        for (size_t i = 0; i < params::num_linear_test; i++) {
            randomness.push_back(region_.adjust_random(v, i));
        }
        
        for (size_t i = 0; i < NumBits - 1; i++) {
            bits[i] = region_.index(v, i, randomness);
        }
        bits[NumBits-1] = region_.index_sign(v, NumBits-1, randomness);
        return bits;
    }

    virtual void zstack_push(s32 v) {
        zstack_.push_back(make_var(v));
    }

    virtual var_type zstack_pop() {
        assert(!zstack_.empty());
        auto x = zstack_.back();
        zstack_.pop_back();
        return x;
    }

    virtual void on_linear_full(row_type& row) = 0;
    virtual void on_quadratic_full(row_type& x, row_type& y, row_type& z) = 0;

    void assert_one(var_type ref) {
        var_type one = make_var(1);
        region_.build_equal(ref, one);
    }

    void finalize() {
        region_.finalize();
    }

    template <typename Expr>
    auto eval_signed(Expr&& expr) { return expr.eval_signed(*this); }

    template <typename Expr>
    auto eval_unsigned(Expr&& expr) { return expr.eval_unsigned(*this); }

    var_type eval(zkp_ops::eval_signed, zkp_ops::add, var_type x, var_type y) {
        auto v = static_cast<s32>(x.val()) + static_cast<s32>(y.val());
        auto z = make_var(v);
        region_.build_linear(z, x, y);
        return z;
    }

    var_type eval(zkp_ops::eval_unsigned, zkp_ops::add, var_type x, var_type y) {
        auto v = static_cast<u32>(x.val()) + static_cast<u32>(y.val());
        auto z = make_var(static_cast<s32>(v));    // needs c++20 to work
        region_.build_linear(z, x, y);
        return z;
    }

    var_type eval(zkp_ops::eval_signed, zkp_ops::sub, var_type x, var_type y) {
        auto v = static_cast<s32>(x.val()) - static_cast<s32>(y.val());
        auto z = make_var(v);
        region_.build_linear(x, y, z);
        return z;
    }

    var_type eval(zkp_ops::eval_unsigned, zkp_ops::sub, var_type x, var_type y) {
        auto v = static_cast<u32>(x.val()) - static_cast<u32>(y.val());
        auto z = make_var(static_cast<s32>(v));
        region_.build_linear(x, y, z);
        return z;
    }

    template <typename Sign>
    var_type eval(Sign, zkp_ops::mul, var_type x, var_type y) {
        return region_.multiply(x, y);
    }

    
    var_type eval(zkp_ops::eval_signed, zkp_ops::div, var_type x, var_type y) {
        return region_.divide_signed(x, y);
    }

    var_type eval(zkp_ops::eval_unsigned, zkp_ops::div, var_type x, var_type y) {
        return region_.divide_unsigned(x, y);
    }

public:
    size_t linear_count = 0, quad_count = 0;
protected:
    std::vector<var_type> zstack_;
    reed_solomon64& encoder_;

    RandomDist dist_;
    gc_managed_region<field_poly, RandomDist> region_;
    random_seeds seeds_;
    std::mt19937 linear_rand_, ql_rand_, qr_rand_, qo_rand_;
    std::mt19937 random_linear_rand_, random_ql_rand_, random_qr_rand_, random_qo_rand_;
};

template <typename LV, typename SV, typename Fp, typename RandomDist>
struct nonbatch_context
    : public nonbatch_context_base<LV, SV, Fp, RandomDist>
      // public extend_call_host<nonbatch_context<Fp>>
{
    using nonbatch_context_base<LV, SV, Fp, RandomDist>::nonbatch_context_base;
};

// Stage 1 (Merkle Tree)
/* ------------------------------------------------------------ */
template <typename LV, typename SV, typename Fp, typename Hasher = sha256>
struct nonbatch_stage1_context : public nonbatch_context<LV, SV, Fp, zero_dist> {
    using Base = nonbatch_context<LV, SV, Fp, zero_dist>;
    using field_poly = Fp;
    using field_type = typename Fp::field_type;
    using operator_type = standard_op;

    using u32_type = typename Base::u32_type;
    using s32_type = typename Base::s32_type;
    using u64_type = typename Base::u64_type;
    using s64_type = typename Base::s64_type;

    using label_type = typename Base::label_type;
    using frame_type = typename Base::frame_type;
    using frame_pointer = typename Base::frame_pointer;

    using row_type = typename Base::row_type;
    
    nonbatch_stage1_context(reed_solomon64& encoder, random_seeds seeds)
        // : lrs_(l), qrs_(q), builder_(l.encoded_size()) { }
        : nonbatch_context<LV, SV, Fp, zero_dist>(encoder, seeds, zero_dist{}),
          builder_(encoder.encoded_size()) { }

    // void push_witness(const u32_type& v) override {
    //     auto p = this->encode(v);
    //     builder_ << p;
    //     this->stack_.push_back(v);
    //     this->zstack_.push_back(p);
    // }
    
    // void process_witness(const field_poly& p) override {
    //     builder_ << p;
    // }

    void on_linear_full(row_type& row) override {
        field_poly p(row.val_begin(), row.val_end());
        auto t = make_timer("stage1", __func__, "encode");
        this->encoder_.encode_with(p, this->linear_rand_);
        t.stop();

        // auto th = make_timer("stage1", "hash");
        builder_ << p;
    }

    void on_quadratic_full(row_type& x, row_type& y, row_type& z) override {
        field_poly px(x.val_begin(), x.val_end());
        field_poly py(y.val_begin(), y.val_end());
        field_poly pz(z.val_begin(), z.val_end());

        auto tt = make_timer("stage1", __func__, "encode");
        #pragma omp parallel sections num_threads(3)
        {
            #pragma omp section
            {
                this->encoder_.encode_with(px, this->ql_rand_);
            }
            #pragma omp section
            {
                this->encoder_.encode_with(py, this->qr_rand_);
            }
            #pragma omp section
            {
                this->encoder_.encode_with(pz, this->qo_rand_);
            }
        }
        tt.stop();

        // auto t = make_timer("stage1", "hash");
        builder_ << px << py << pz;
    }

    auto&& builder() {
        return std::move(builder_);
        // mkt_ = builder_;
        // return mkt_;
    }
    
protected:
    // reed_solomon64& lrs_, qrs_;
    // merkle_tree<Hasher> mkt_;
    typename merkle_tree<Hasher>::builder builder_;
};


// Stage 2 (code test, linear test, qudratic test)
/* ------------------------------------------------------------ */
template <typename LV, typename SV, typename Fp, typename RandomDist>
struct nonbatch_stage2_context : public nonbatch_context<LV, SV, Fp, RandomDist> {
    using Base = nonbatch_context<LV, SV, Fp, RandomDist>;
    using field_poly = Fp;
    using pv_type = typename field_poly::value_type;
    using field_type = typename Fp::field_type;
    using operator_type = standard_op;    

    using u32_type = typename Base::u32_type;
    using s32_type = typename Base::s32_type;
    using u64_type = typename Base::u64_type;
    using s64_type = typename Base::s64_type;

    using label_type = typename Base::label_type;
    using frame_type = typename Base::frame_type;
    using frame_pointer = typename Base::frame_pointer;

    using row_type = typename Base::row_type;
    
    nonbatch_stage2_context(reed_solomon64& encoder, random_seeds eseed, RandomDist&& dist,
                            RandomDist&& dc, RandomDist&& dl, RandomDist&& dq)
        : nonbatch_context<LV, SV, Fp, RandomDist>(encoder, eseed,
                                                   std::forward<RandomDist>(dist)),
          arg_(encoder, encoder.encoded_size(),
               std::forward<RandomDist>(dc),
               std::forward<RandomDist>(dl),
               std::forward<RandomDist>(dq))
        { }

    // void assert_linear(const field_poly& z, const field_poly& x, const field_poly& y) override {
    //     #pragma omp parallel sections
    //     {
    //         #pragma omp section
    //         { arg_ .update_code(z); }

    //         #pragma omp section
    //         { arg_.update_linear(x, y, z); }
    //     }
    // }

    void on_linear_full(row_type& row) override {        
        field_poly p(row.val_begin(), row.val_end());
        auto tt = make_timer("stage2", __func__, "encode");

        // Prepare all randomness
        pv_type rand_code[params::num_code_test] = { 0UL };

        // NB: Assume data is contigious
        field_poly* rand_poly = row.randoms().data();
        
        #pragma omp parallel
        {
            // Prepare all randomness
            #pragma omp single
            {
                // Encode the value row
                #pragma omp task depend(out: p, this->linear_rand_)
                this->encoder_.encode_with(p, this->linear_rand_);

                for (size_t i = 0; i < params::num_code_test; i++) {
                    #pragma omp task depend(out: rand_code[i], this->arg_)
                    rand_code[i] = arg_.randgen_code();

                    #pragma omp task depend(in: p, rand_code[i])
                    arg_.update_code(i, p, rand_code[i]);
                }

                for (size_t i = 0; i < params::num_linear_test; i++) {
                    #pragma omp task depend(out: rand_poly[i], this->random_linear_rand_)
                    this->encoder_.encode_prepare(rand_poly[i], this->random_linear_rand_);

                    #pragma omp task depend(inout: rand_poly[i])
                    this->encoder_.encode_ntt(rand_poly[i]);

                    #pragma omp task depend(in: p, rand_poly[i])
                    arg_.update_linear(i, p, rand_poly[i]);
                }
            }
        }
    }

    void on_quadratic_full(row_type& x, row_type& y, row_type& z) override {
        num_constraints_++;

        field_poly px(x.val_begin(), x.val_end());
        field_poly py(y.val_begin(), y.val_end());
        field_poly pz(z.val_begin(), z.val_end());

        pv_type rand_code[params::num_code_test * 3] = { 0UL };
        pv_type rand_quad[params::num_quadratic_test] = { 0UL };

        field_poly* x_rand = x.randoms().data();
        field_poly* y_rand = y.randoms().data();
        field_poly* z_rand = z.randoms().data();

        auto tt = make_timer("stage2", __func__, "encode");
        #pragma omp parallel
        {
            #pragma omp single
            {
                #pragma omp task depend(inout: px, this->ql_rand_)
                this->encoder_.encode_with(px, this->ql_rand_);

                #pragma omp task depend(inout: py, this->qr_rand_)
                this->encoder_.encode_with(py, this->qr_rand_);

                #pragma omp task depend(inout: pz, this->qo_rand_)
                this->encoder_.encode_with(pz, this->qo_rand_);

                for (size_t i = 0; i < params::num_linear_test; i++) {
                    #pragma omp task depend(inout: x_rand[i], this->random_ql_rand_)
                    this->encoder_.encode_prepare(x_rand[i], this->random_ql_rand_);
                    
                    #pragma omp task depend(inout: y_rand[i], this->random_qr_rand_)
                    this->encoder_.encode_prepare(y_rand[i], this->random_qr_rand_);
                    
                    #pragma omp task depend(inout: z_rand[i], this->random_qo_rand_)
                    this->encoder_.encode_prepare(z_rand[i], this->random_qo_rand_);

                    #pragma omp task depend(inout: x_rand[i])
                    this->encoder_.encode_ntt(x_rand[i]);

                    #pragma omp task depend(inout: y_rand[i])
                    this->encoder_.encode_ntt(y_rand[i]);

                    #pragma omp task depend(inout: z_rand[i])
                    this->encoder_.encode_ntt(z_rand[i]);
                }

                for (size_t i = 0; i < params::num_code_test * 3; i++) {
                    #pragma omp task depend(inout: rand_code[i], this->arg_)
                    rand_code[i] = arg_.randgen_code();
                }

                for (size_t i = 0; i < params::num_quadratic_test; i++) {
                    #pragma omp task depend(inout: rand_quad[i], this->arg_)
                    rand_quad[i] = arg_.randgen_quad();
                }

                for (size_t i = 0; i < params::num_code_test; i++) {
                    #pragma omp task depend(in: px, py, pz, \
                                            rand_code[i], \
                                            rand_code[i + params::num_code_test], \
                                            rand_code[i + 2 * params::num_code_test])
                    {
                        arg_.update_code(i, px, rand_code[i]);
                        arg_.update_code(i, py, rand_code[i + params::num_code_test]);
                        arg_.update_code(i, pz, rand_code[i + 2 * params::num_code_test]);
                    }
                }

                for (size_t i = 0; i < params::num_linear_test; i++) {
                    #pragma omp task depend(in: px, py, pz, x_rand[i], y_rand[i], z_rand[i])
                    {
                        arg_.update_linear(i, px, x_rand[i]);
                        arg_.update_linear(i, py, y_rand[i]);
                        arg_.update_linear(i, pz, z_rand[i]);
                    }
                }

                for (size_t i = 0; i < params::num_quadratic_test; i++) {
                    #pragma omp task depend(in: px, py, pz, rand_quad[i])
                    arg_.update_quadratic(i, px, py, pz, rand_quad[i]);
                }
        }
        // tt.stop();

        // auto t = make_timer("stage2", __func__, "QuadCheck");
        // #pragma omp parallel sections
        // {
        //     #pragma omp section
        //     {
        //         arg_.update_code(px);
        //         arg_.update_code(py);
        //         arg_.update_code(pz);
        //     }

        //     #pragma omp section
        //     {
        //         arg_.update_linear(px, x.random());
        //         arg_.update_linear(py, y.random());
        //         arg_.update_linear(pz, z.random());
        //     }

        //     #pragma omp section
        //     {
        //         arg_.update_quadratic(px, py, pz);   
        //     }
        // }
        }
    }

    // void process_witness(const field_poly& p) override {
    //     arg_.update_code(p);
    // }

    // void assert_quadratic(const field_poly& z, const field_poly& x, const field_poly& y) override {
    //     arg_.update_quadratic(x, y, z);
    // }

    auto& get_argument() { return arg_; }
    size_t constraints_count() const { return num_constraints_; }
    
protected:
    size_t num_constraints_ = 0;
    nonbatch_argument<reed_solomon64, row_type, RandomDist> arg_;
};


template <typename LV, typename SV, typename Fp, typename SaveFunc>
struct nonbatch_stage3_context : public nonbatch_context<LV, SV, Fp, zero_dist>
{
    using Base = nonbatch_context<LV, SV, Fp, zero_dist>;
    using field_poly = Fp;
    using field_type = typename Fp::field_type;
    using operator_type = standard_op;
    using var_type = typename Base::var_type;

    using u32_type = typename Base::u32_type;
    using s32_type = typename Base::s32_type;
    using u64_type = typename Base::u64_type;
    using s64_type = typename Base::s64_type;

    using label_type = typename Base::label_type;
    using frame_type = typename Base::frame_type;
    using frame_pointer = typename Base::frame_pointer;

    using row_type = typename Base::row_type;

    nonbatch_stage3_context(reed_solomon64& encoder, random_seeds seeds, const std::vector<size_t>& si, SaveFunc func)
        : Base(encoder, seeds, zero_dist{}), sample_index_(si), func_(func) { }

    void sample_row(const field_poly& p) {
        field_poly sp(sample_index_.size());
        for (size_t i = 0; i < sample_index_.size(); i++) {
            sp[i] = p[sample_index_[i]];
        }
        push_sample(sp);
    }
    
    void on_linear_full(row_type& row) override {
        field_poly p(row.val_begin(), row.val_end());
        auto t = make_timer("stage3", __func__, "encode");
        this->encoder_.encode_with(p, this->linear_rand_);
        t.stop();
        sample_row(p);
    }

    void on_quadratic_full(row_type& x, row_type& y, row_type& z) override {
        field_poly px(x.val_begin(), x.val_end());
        field_poly py(y.val_begin(), y.val_end());
        field_poly pz(z.val_begin(), z.val_end());

        auto t = make_timer("stage3", __func__, "encode");
        #pragma omp parallel sections num_threads(3)
        {
            #pragma omp section
            {
                this->encoder_.encode_with(px, this->ql_rand_);
            }
            #pragma omp section
            {
                this->encoder_.encode_with(py, this->qr_rand_);
            }
            #pragma omp section
            {
                this->encoder_.encode_with(pz, this->qo_rand_);
            }
        }
        t.stop();
        sample_row(px);
        sample_row(py);
        sample_row(pz);
    }

    void push_sample(const field_poly& sample) {
        func_(sample);
    }

protected:
    std::vector<size_t> sample_index_;
    std::vector<field_poly> samples_;
    SaveFunc func_;
};





template <typename LV, typename SV,
          typename Fp,
          typename LoadFunc,
          typename Hasher,
          typename RandomDist>
struct nonbatch_verifier_context : public nonbatch_context<LV, SV, Fp, RandomDist> {
    using Base = nonbatch_context<LV, SV, Fp, RandomDist>;
    using field_poly = Fp;
    using field_type = typename Fp::field_type;
    using operator_type = standard_op;
    using var_type = typename Base::var_type;

    using u32_type = typename Base::u32_type;
    using s32_type = typename Base::s32_type;
    using u64_type = typename Base::u64_type;
    using s64_type = typename Base::s64_type;

    using label_type = typename Base::label_type;
    using frame_type = typename Base::frame_type;
    using frame_pointer = typename Base::frame_pointer;

    using row_type = typename Base::row_type;
    // using row_ptr = typename Base::row_ptr;

    nonbatch_verifier_context(reed_solomon64& encoder,
                              random_seeds encode_seeds,
                              RandomDist&& dist,
                              RandomDist&& dc, RandomDist&& dl, RandomDist&& dq,
                              const std::vector<size_t>& si,
                              LoadFunc func)
        : Base(encoder, encode_seeds, std::forward<RandomDist>(dist)), sample_index_(si),
          builder_(sample_index_.size()),
          arg_(encoder, sample_index_.size(),
               std::forward<RandomDist>(dc),
               std::forward<RandomDist>(dl),
               std::forward<RandomDist>(dq)),
          func_(func)
        {
            // std::reverse(sampled_val_.begin(), sampled_val_.end());
        }

    void on_linear_full(row_type& row) override {
        field_poly saved_row = pop_sample();
        field_poly* rands = row.randoms().data();
        field_poly sprands[params::num_linear_test];

        std::fill_n(sprands, params::num_linear_test, field_poly(sample_index_.size()));

        typename field_poly::value_type rand_code[params::num_code_test] = { 0UL };
        auto ct = make_timer(__func__, "check");
        
        #pragma omp parallel
        {
            #pragma omp single
            {
                #pragma omp task
                builder_ << saved_row;                
                
                for (size_t i = 0; i < params::num_code_test; i++) {
                    #pragma omp task depend(out: rand_code[i], this->arg_)
                    rand_code[i] = arg_.randgen_code();

                    #pragma omp task depend(in: saved_row, rand_code[i])
                    arg_.update_code(i, saved_row, rand_code[i]);
                }
                
                for (size_t i = 0; i < params::num_linear_test; i++) {
                    #pragma omp task depend(inout: rands[i], this->random_linear_rand_)
                    this->encoder_.encode_prepare(rands[i], this->random_linear_rand_);

                    #pragma omp task depend(in: rands[i]) depend(out: sprands[i])
                    {
                        this->encoder_.encode_ntt(rands[i]);
                        for (size_t j = 0; j < sample_index_.size(); j++) {
                            sprands[i][j] = rands[i][sample_index_[j]];
                        }
                    }

                    #pragma omp task depend(in: sprands[i])
                    arg_.update_linear(i, saved_row, sprands[i]);
                }
            }
        }
    }

    void on_quadratic_full(row_type& x, row_type& y, row_type& z) override {
        field_poly saved_x = pop_sample();
        field_poly saved_y = pop_sample();
        field_poly saved_z = pop_sample();

        field_poly *x_rand = x.randoms().data();
        field_poly *y_rand = y.randoms().data();
        field_poly *z_rand = z.randoms().data();

        using pv = typename field_poly::value_type;

        pv rand_code[params::num_code_test * 3] = { 0UL };
        pv rand_quad[params::num_quadratic_test] = { 0UL };

        field_poly sprands_x[params::num_linear_test];
        field_poly sprands_y[params::num_linear_test];
        field_poly sprands_z[params::num_linear_test];

        std::fill_n(sprands_x, params::num_linear_test, field_poly(sample_index_.size()));
        std::fill_n(sprands_y, params::num_linear_test, field_poly(sample_index_.size()));
        std::fill_n(sprands_z, params::num_linear_test, field_poly(sample_index_.size()));
        
        auto et = make_timer(__func__, "encode");
        #pragma omp parallel
        {
            #pragma omp single
            {
                #pragma omp task
                builder_ << saved_x << saved_y << saved_z;

                for (size_t i = 0; i < params::num_code_test * 3; i++) {
                    #pragma omp task depend(inout: rand_code[i], this->arg_)
                    rand_code[i] = arg_.randgen_code();
                }

                for (size_t i = 0; i < params::num_code_test; i++) {
                    #pragma omp task depend(in: saved_x, saved_y, saved_z, \
                                            rand_code[i], \
                                            rand_code[i + params::num_code_test], \
                                            rand_code[i + 2 * params::num_code_test])
                    {
                        arg_.update_code(i, saved_x, rand_code[i]);
                        arg_.update_code(i, saved_y, rand_code[i + params::num_code_test]);
                        arg_.update_code(i, saved_z, rand_code[i + 2 * params::num_code_test]);
                    }
                }

                for (size_t i = 0; i < params::num_linear_test; i++) {
                    #pragma omp task depend(inout: x_rand[i], this->random_ql_rand_)
                    this->encoder_.encode_prepare(x_rand[i], this->random_ql_rand_);
                    
                    #pragma omp task depend(inout: y_rand[i], this->random_qr_rand_)
                    this->encoder_.encode_prepare(y_rand[i], this->random_qr_rand_);
                    
                    #pragma omp task depend(inout: z_rand[i], this->random_qo_rand_)
                    this->encoder_.encode_prepare(z_rand[i], this->random_qo_rand_);

                    #pragma omp task depend(inout: x_rand[i], sprands_x[i])
                    {
                        this->encoder_.encode_ntt(x_rand[i]);
                        for (size_t j = 0; j < sample_index_.size(); j++) {
                            sprands_x[i][j] = x_rand[i][sample_index_[j]];
                        }
                    }

                    #pragma omp task depend(inout: y_rand[i], sprands_y[i])
                    {
                        this->encoder_.encode_ntt(y_rand[i]);
                        for (size_t j = 0; j < sample_index_.size(); j++) {
                            sprands_y[i][j] = y_rand[i][sample_index_[j]];
                        }
                    }

                    #pragma omp task depend(inout: z_rand[i], sprands_z[i])
                    {
                        this->encoder_.encode_ntt(z_rand[i]);
                        for (size_t j = 0; j < sample_index_.size(); j++) {
                            sprands_z[i][j] = z_rand[i][sample_index_[j]];
                        }
                    }

                    #pragma omp task depend(in: saved_x, saved_y, saved_z, sprands_x[i], sprands_y[i], sprands_z[i])
                    {
                        arg_.update_linear(i, saved_x, sprands_x[i]);
                        arg_.update_linear(i, saved_y, sprands_y[i]);
                        arg_.update_linear(i, saved_z, sprands_z[i]);
                    }
                }

                for (size_t i = 0; i < params::num_quadratic_test; i++) {
                    #pragma omp task depend(inout: rand_quad[i], this->arg_)
                    rand_quad[i] = arg_.randgen_quad();

                    #pragma omp task depend(in: saved_x, saved_y, saved_z, rand_quad[i])
                    arg_.update_quadratic(i, saved_x, saved_y, saved_z, rand_quad[i]);
                }                
            }
            
            // #pragma omp section
            // {
            //     field_poly& rand = x.random();
            //     this->encoder_.encode_with(rand, this->random_ql_rand_);
            // }

            // #pragma omp section
            // {
            //     field_poly& rand = y.random();
            //     this->encoder_.encode_with(rand, this->random_qr_rand_);
            //     for (size_t i = 0; i < sample_index_.size(); i++) {
            //         sprand_y[i] = rand[sample_index_[i]];
            //     }
            // }

            // #pragma omp section
            // {
            //     field_poly& rand = z.random();
            //     this->encoder_.encode_with(rand, this->random_qo_rand_);
            //     for (size_t i = 0; i < sample_index_.size(); i++) {
            //         sprand_z[i] = rand[sample_index_[i]];
            //     }
            // }
        }
        et.stop();

        // auto ct = make_timer(__func__, "check");
        // #pragma omp parallel sections
        // {
        //     #pragma omp section
        //     {
        //         arg_.update_code(saved_x);
        //         arg_.update_code(saved_y);
        //         arg_.update_code(saved_z);
        //     }

        //     #pragma omp section
        //     {
        //         arg_.update_linear(saved_x, sprand_x);
        //         arg_.update_linear(saved_y, sprand_y);
        //         arg_.update_linear(saved_z, sprand_z);
        //     }

        //     #pragma omp section
        //     {
        //         arg_.update_quadratic(saved_x, saved_y, saved_z);
        //     }
        // }
    }

    auto&& builder() {
        return std::move(builder_);
    }

    auto& get_argument() { return arg_; }

protected:
    std::vector<size_t> sample_index_;
    // std::vector<field_poly> sampled_val_;
    LoadFunc func_;
    
    typename merkle_tree<Hasher>::builder builder_;
    nonbatch_argument<reed_solomon64, row_type, RandomDist> arg_;

    field_poly pop_sample() {
        // field_poly p = sampled_val_.back();
        // sampled_val_.pop_back();
        // return p;

        return func_();
    }
};


}  // namespace ligero::vm::zkp
