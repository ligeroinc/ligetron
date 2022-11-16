#pragma once

#include <unordered_map>

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
    auto operator()() const noexcept { return 0; }
};

struct one_dist {
    using seed_type = hash_random_engine<sha256>;

    template <typename... Args>
    one_dist(Args&&...) { }
    
    auto operator()() const noexcept { return 1; }
};

template <typename T, typename Engine = hash_random_engine<sha256>>
struct hash_random_dist {
    using seed_type = typename Engine::seed_type;

    hash_random_dist(T modulus, const typename Engine::seed_type& seed)
        : engine_(seed), dist_(T{0}, modulus - T{1}) { }

    auto operator()() { return dist_(engine_); }

    Engine engine_;
    random::uniform_int_distribution<T> dist_;
};

template <typename LocalValue, typename StackValue, typename Fp, typename RandomDist>
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

    nonbatch_context_base(reed_solomon64& encoder, RandomDist dist = RandomDist{})
        : encoder_(encoder),
          dist_(std::move(dist)),
          region_(encoder.plain_size(), dist_)
        {
            region_.on_linear([&](auto row) { return on_linear_full(std::move(row)); });
            region_.on_quadratic([&](auto... rows) { return on_quadratic_full(std::move(rows)...); });
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

    void stack_push(var_type ref) {
        Base::stack_push(ref.val());
        zstack_.push_back(std::move(ref));
    }

    var_type stack_pop_var() {
        Base::stack_pop();
        // assert(std::holds_alternative<u32_type>(val));
        return zstack_pop();
    }

    var_type make_var(s32 v) {
        return region_.push_back_linear(v);
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

    virtual void on_linear_full(row_type row) = 0;
    virtual void on_quadratic_full(row_type x, row_type y, row_type z) = 0;

    void finalize() {
        region_.finalize();
    }

    template <typename Expr>
    auto eval(Expr&& expr) {
        return expr.eval(*this);
    }

    virtual var_type eval(zkp_ops::add, var_type x, var_type y) {
        u32_type v = x.val() + y.val();
        auto z = make_var(static_cast<s64>(v));
        region_.build_linear(z, x, y);
        return z;
    }

    virtual var_type eval(zkp_ops::sub, var_type x, var_type y) {
        s32 v = static_cast<s32>(x.val()) - static_cast<s32>(y.val());
        auto z = make_var(v);
        region_.build_linear(x, y, z);
        return z;
    }

    virtual var_type eval(zkp_ops::mul, var_type x, var_type y) {
        return region_.multiply(x, y);
    }

    virtual var_type eval(zkp_ops::div, var_type x, var_type y) {
        return region_.divide(x, y);
    }

    virtual var_type eval(zkp_ops::index_of op, const var_type& x) {
        size_t index = op.index;
        return region_.index(x, index);
    }

public:
    size_t linear_count = 0, quad_count = 0;
protected:
    std::vector<var_type> zstack_;
    reed_solomon64& encoder_;

    RandomDist dist_;
    gc_managed_region<field_poly, RandomDist> region_;
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
    
    nonbatch_stage1_context(reed_solomon64& encoder)
        // : lrs_(l), qrs_(q), builder_(l.encoded_size()) { }
        : nonbatch_context<LV, SV, Fp, zero_dist>(encoder), builder_(encoder.encoded_size()) { }

    // void push_witness(const u32_type& v) override {
    //     auto p = this->encode(v);
    //     builder_ << p;
    //     this->stack_.push_back(v);
    //     this->zstack_.push_back(p);
    // }
    
    // void process_witness(const field_poly& p) override {
    //     builder_ << p;
    // }

    void on_linear_full(row_type row) override {
        field_poly p(row.val_begin(), row.val_end());
        this->encoder_.encode(p);
        builder_ << p;
    }

    void on_quadratic_full(row_type x, row_type y, row_type z) override {
        on_linear_full(std::move(x));
        on_linear_full(std::move(y));
        on_linear_full(std::move(z));
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
template <typename LV, typename SV, typename Fp,
          typename RandomDist = hash_random_dist<typename Fp::value_type, hash_random_engine<sha256>>>
struct nonbatch_stage2_context : public nonbatch_context<LV, SV, Fp, RandomDist> {
    using Base = nonbatch_context<LV, SV, Fp, RandomDist>;
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
    
    nonbatch_stage2_context(reed_solomon64& encoder, const typename RandomDist::seed_type& seed)
        : nonbatch_context<LV, SV, Fp, RandomDist>(encoder, RandomDist{field_poly::modulus, seed}),
          arg_(encoder, encoder.encoded_size(), seed) { }

    // void assert_linear(const field_poly& z, const field_poly& x, const field_poly& y) override {
    //     #pragma omp parallel sections
    //     {
    //         #pragma omp section
    //         { arg_ .update_code(z); }

    //         #pragma omp section
    //         { arg_.update_linear(x, y, z); }
    //     }
    // }

    void on_linear_full(row_type row) override {
        arg_.update_linear(row);
    }

    void on_quadratic_full(row_type x, row_type y, row_type z) override {
        arg_.update_quadratic(x, y, z);
    }

    // void process_witness(const field_poly& p) override {
    //     arg_.update_code(p);
    // }

    // void assert_quadratic(const field_poly& z, const field_poly& x, const field_poly& y) override {
    //     arg_.update_quadratic(x, y, z);
    // }

    const auto& get_argument() const { return arg_; }
    
protected:
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

    nonbatch_stage3_context(reed_solomon64& encoder, const std::vector<size_t>& si, SaveFunc func)
        : Base(encoder), sample_index_(si), func_(func)
        { }

    void on_linear_full(row_type row) override {
        field_poly p(row.val_begin(), row.val_end());
        this->encoder_.encode(p);
        field_poly sp(sample_index_.size());
        for (size_t i = 0; i < sample_index_.size(); i++) {
            sp[i] = p[sample_index_[i]];
        }
        push_sample(sp);
    }

    void on_quadratic_full(row_type x, row_type y, row_type z) override {
        on_linear_full(std::move(x));
        on_linear_full(std::move(y));
        on_linear_full(std::move(z));
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
          typename Hasher = sha256,
          typename RandomDist = hash_random_dist<typename Fp::value_type, hash_random_engine<sha256>>>
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

    nonbatch_verifier_context(reed_solomon64& encoder,
                              const typename RandomDist::seed_type& seed,
                              const std::vector<size_t>& si,
                              LoadFunc func)
        : Base(encoder, RandomDist{field_poly::modulus, seed}), sample_index_(si),
          builder_(sample_index_.size()),
          arg_(encoder, sample_index_.size(), seed),
          func_(func)
        {
            // std::reverse(sampled_val_.begin(), sampled_val_.end());
        }

    void on_linear_full(row_type row) override {
        field_poly saved_row = pop_sample();
        builder_ << saved_row;

        field_poly rand = row.random();
        this->encoder_.encode(rand);
        field_poly sprand(sample_index_.size());
        for (size_t i = 0; i < sample_index_.size(); i++) {
            sprand[i] = rand[sample_index_[i]];
        }
        
        arg_.update_linear(saved_row, sprand);
    }

    void on_quadratic_full(row_type x, row_type y, row_type z) override {
        field_poly saved_x = pop_sample();
        field_poly saved_y = pop_sample();
        field_poly saved_z = pop_sample();

        builder_ << saved_x << saved_y << saved_z;

        {
            field_poly rand = x.random();
            this->encoder_.encode(rand);
            field_poly sprand(sample_index_.size());
            for (size_t i = 0; i < sample_index_.size(); i++) {
                sprand[i] = rand[sample_index_[i]];
            }
            arg_.update_linear(saved_x, sprand);
        }

        {
            field_poly rand = y.random();
            this->encoder_.encode(rand);
            field_poly sprand(sample_index_.size());
            for (size_t i = 0; i < sample_index_.size(); i++) {
                sprand[i] = rand[sample_index_[i]];
            }
            arg_.update_linear(saved_y, sprand);
        }

        {
            field_poly rand = z.random();
            this->encoder_.encode(rand);
            field_poly sprand(sample_index_.size());
            for (size_t i = 0; i < sample_index_.size(); i++) {
                sprand[i] = rand[sample_index_[i]];
            }
            arg_.update_linear(saved_z, sprand);
        }

        arg_.update_quadratic(saved_x, saved_y, saved_z);
    }

    auto&& builder() {
        return std::move(builder_);
    }

    const auto& get_argument() const { return arg_; }

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
