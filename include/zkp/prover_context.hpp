#pragma once

#include <context.hpp>
#include <zkp/encoding.hpp>
#include <zkp/merkle_tree.hpp>
#include <zkp/argument.hpp>
#include <zkp/variable.hpp>
#include <unordered_map>

namespace ligero::vm::zkp {

template <typename Poly>
void print(Poly& p) {
    std::cout << "poly: ";
    for (const auto& v : p.data()) {
        std::cout << v << " ";
    }
    std::cout << std::endl;
}

template <typename LocalValue, typename StackValue, typename Fp>
struct zkp_context_base : public context_base<LocalValue, StackValue> {
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

    using var_type = zkp_var<u32_type, field_poly>;

    zkp_context_base(reed_solomon64& encoder) : encoder_(encoder) { }
    virtual ~zkp_context_base() = default;

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

    void stack_push(const var_type& var) {
        Base::stack_push(var.val());
        zstack_.push_back(var.poly());
    }

    var_type stack_pop_var() {
        auto val = Base::stack_pop().template as<u32_type>();
        // assert(std::holds_alternative<u32_type>(val));
        return var_type{ val, zstack_pop() };
    }
    
    virtual field_poly encode(const u32_type& v) {
        // s32_type sv;
        // prelude::transform(v, sv, to_signed);
        // field_poly p(1, v);
        if constexpr (requires(u32_type u) { u.begin(); }) {
            field_poly p = field_poly(v.begin(), v.end());
            encoder_.encode(p);
            return p;
        }
        else {
            field_poly p = field_poly{ v };
            encoder_.encode(p);
            return p;
        }
    }

    virtual field_poly& encode_const(s32 k) {
        if (consts_.contains(k)) {
            return consts_[k];
        }
        else {
            field_poly p(encoder_.plain_size(), k);
            encoder_.encode_const(p);
            auto [pair, _] = consts_.emplace(k, std::move(p));
            auto& [k, v] = *pair;
            return v;
        }
    }

    virtual void zstack_push(const u32_type& v) {
        zstack_.push_back(encode(v));
    }

    virtual field_poly zstack_pop() {
        assert(!zstack_.empty());
        auto x = zstack_.back();
        zstack_.pop_back();
        return x;
    }

    // void show_stack() const {
    //     Base::show_stack();
    //     std::cout << zstack_.size() << std::endl;
    // }

    // void count() const {
    //     std::cout << "field count: " << fcounter
    //               << " scounter: " << scounter << std::endl;
    // }

    // virtual void assert_linear(const field_poly& z, const field_poly& x, const field_poly& y) {
    //     // do nothing - override in child class
    //     linear_count++;
    // }

    virtual void assert_quadratic(const field_poly& z, const field_poly& x, const field_poly& y) {
        // do nothing - override in child class
        quad_count++;
    }

    virtual void process_witness(const field_poly& p) {
        // do nothing
    }

    template <typename Expr>
    auto eval(const Expr& expr) {
        return expr.eval(*this);
    }

    virtual var_type eval(zkp_ops::add, const var_type& x, const var_type& y) {
        u32_type v = x.val() + y.val();
        field_poly p = x.poly() + y.poly();
        process_witness(p);
        // assert_linear(p, x.poly(), y.poly());  // p = x + y
        return var_type{ std::move(v), std::move(p) };
    }

    virtual var_type eval(zkp_ops::sub, const var_type& x, const var_type& y) {
        u32_type v = x.val() - y.val();
        field_poly p = x.poly() - y.poly();
        process_witness(p);
        // assert_linear(x.poly(), p, y.poly());  // p = x - y ==> x = p + y
        return var_type{ v, std::move(p) };
    }

    virtual var_type eval(zkp_ops::mul, const var_type& x, const var_type& y) {
        u32_type v = x.val() * y.val();
        auto p = encode(v);
        process_witness(p);
        assert_quadratic(p, x.poly(), y.poly());  // p = x * y
        return var_type{ std::move(v), std::move(p) };
    }

    virtual var_type eval(zkp_ops::div, const var_type& x, const var_type& y) {
        assert(false);
        
        u32_type v = x.val() / y.val();
        auto p = encode(v);
        process_witness(p);
        assert_quadratic(x.poly(), p, y.poly());  // p = x / y ==> x = p * y
        return var_type{ v, std::move(p) };
    }

    virtual var_type eval(zkp_ops::index_of op, const var_type& x) {
        size_t index = op.index;
        u32_type v = x.val();
        u32_type bit = (v >> index) & 1U;
        auto p = encode(bit);
        process_witness(p);
        assert_quadratic(p, p, p);  // x^2 - x = 0
        return var_type { std::move(bit), std::move(p) };
    }

public:
    size_t linear_count = 0, quad_count = 0;
protected:
    std::vector<field_poly> zstack_;
    reed_solomon64& encoder_;
    std::unordered_map<s32, field_poly> consts_;
};

template <typename LV, typename SV, typename Fp>
struct zkp_context
    : public zkp_context_base<LV, SV, Fp>
      // public extend_call_host<zkp_context<Fp>>
{
    using zkp_context_base<LV, SV, Fp>::zkp_context_base;
};

// Stage 1 (Merkle Tree)
/* ------------------------------------------------------------ */
template <typename LV, typename SV, typename Fp, typename Hasher = sha256>
struct stage1_prover_context : public zkp_context<LV, SV, Fp> {
    using Base = zkp_context<LV, SV, Fp>;
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
    
    stage1_prover_context(reed_solomon64& encoder)
        // : lrs_(l), qrs_(q), builder_(l.encoded_size()) { }
        : zkp_context<LV, SV, Fp>(encoder), builder_(encoder.encoded_size()) { }

    // void push_witness(u32 v) override {
    //     builder_ << this->encode(v);
    //     Base::push_witness(v);
    // }

    void push_witness(const u32_type& v) override {
        auto p = this->encode(v);
        builder_ << p;
        this->stack_.push_back(v);
        this->zstack_.push_back(p);
    }

    // void assert_linear(const field_poly& z, const field_poly& x, const field_poly& y) override {
    //     builder_ << z;
    // }

    // void assert_quadratic(const field_poly& z, const field_poly& x, const field_poly& y) override {
    //     builder_ << z;
    // }
    void process_witness(const field_poly& p) override {
        builder_ << p;
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
          typename RandomEngine = hash_random_engine<sha256>>
struct stage2_prover_context : public zkp_context<LV, SV, Fp> {
    using Base = zkp_context<LV, SV, Fp>;
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
    
    stage2_prover_context(reed_solomon64& encoder, const typename RandomEngine::seed_type& seed)
        // : lrs_(l), qrs_(q), arg_(l.encoded_size(), seed) { }
        : zkp_context<LV, SV, Fp>(encoder), arg_(encoder.encoded_size(), seed) { }

    // void assert_linear(const field_poly& z, const field_poly& x, const field_poly& y) override {
    //     #pragma omp parallel sections
    //     {
    //         #pragma omp section
    //         { arg_ .update_code(z); }

    //         #pragma omp section
    //         { arg_.update_linear(x, y, z); }
    //     }
    // }

    void process_witness(const field_poly& p) override {
        arg_.update_code(p);
    }

    void assert_quadratic(const field_poly& z, const field_poly& x, const field_poly& y) override {
        arg_.update_quadratic(x, y, z);
    }

    const auto& get_argument() const { return arg_; }
    
protected:
    quasi_argument<Fp, RandomEngine> arg_;
};


template <typename LV, typename SV, typename Fp>
struct stage3_prover_context : public zkp_context<LV, SV, Fp>
{
    using Base = zkp_context<LV, SV, Fp>;
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

    using Base::eval, Base::encode;

    stage3_prover_context(reed_solomon64& encoder, const std::vector<size_t>& si)
        : Base(encoder), sample_index_(si)
        { }

    field_poly encode(const typename Base::u32_type& v) override {
        auto p = Base::encode(v);
        field_poly sp(sample_index_.size());
        #pragma omp parallel for
        for (size_t i = 0; i < sample_index_.size(); i++) {
            sp[i] = p[sample_index_[i]];
        }
        return sp;
    }

    field_poly& encode_const(s32 k) override {
        if (this->consts_.contains(k)) {
            return this->consts_[k];
        }
        else {
            field_poly p(this->encoder_.plain_size(), k);
            this->encoder_.encode_const(p);
            
            field_poly sp(sample_index_.size());
            #pragma omp parallel for
            for (size_t i = 0; i < sample_index_.size(); i++) {
                sp[i] = p[sample_index_[i]];
            }
            
            auto [pair, _] = this->consts_.emplace(k, std::move(sp));
            auto& [k, v] = *pair;
            return v;
        }
    }

    // void push_witness(u32 v) override {
    //     samples_.push_back(encode(v));
    //     Base::push_witness(v);
    // }
    
    void zstack_push(const typename Base::u32_type& v) override {
        auto p = encode(v);
        samples_.push_back(p);
        this->zstack_.push_back(std::move(p));
    }
    
    // void zstack_push(field_poly&& f) override {
    //     // poly_type p(1, f);
    //     // this->encoder_.encode(p);
        
    //     field_poly sp(sample_index_.size());
    //     #pragma omp parallel for
    //     for (size_t i = 0; i < sample_index_.size(); i++) {
    //         sp[i] = f[sample_index_[i]];
    //     }
        
    //     // this->zstack_.push_back(f);
    //     Base::zstack_push(std::move(f));
    //     samples_.push_back(std::move(sp));
    // }
    
    var_type eval(zkp_ops::mul op, const var_type& x, const var_type& y) override {
        var_type var = Base::eval(op, x, y);
        samples_.push_back(var.poly());
        return var;
    }

    var_type eval(zkp_ops::div op, const var_type& x, const var_type& y) override {
        var_type var = Base::eval(op, x, y);
        samples_.push_back(var.poly());
        return var;
    }

    var_type eval(zkp_ops::index_of op, const var_type& x) override {
        var_type var = Base::eval(op, x);
        samples_.push_back(var.poly());
        return var;
    }

    const auto& get_sample() const { return samples_; }

protected:
    std::vector<size_t> sample_index_;
    std::vector<field_poly> samples_;
};


template <typename LV, typename SV,
          typename Fp,
          typename Hasher = sha256,
          typename RandomEngine = hash_random_engine<Hasher>>
struct verifier_context : public zkp_context<LV, SV, Fp> {
    using Base = zkp_context<LV, SV, Fp>;
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

    using Base::eval;
    using Base::assert_quadratic;

    verifier_context(reed_solomon64& encoder,
                     const typename RandomEngine::seed_type& seed,
                     const std::vector<size_t>& si,
                     const std::vector<field_poly>& sv)
        : Base(encoder), sample_index_(si), sampled_val_(sv),
          builder_(sample_index_.size()),
          arg_(si.size(), seed)
        {
            std::reverse(sampled_val_.begin(), sampled_val_.end());
        }
    
    // void zstack_push(field_poly&& f) override {
    //     assert(!sampled_val_.empty());
    //     field_poly p = sampled_val_.back();
    //     sampled_val_.pop_back();
    //     this->zstack_.push_back(std::move(p));
    // }

    void zstack_push(const u32_type&) override {
        this->zstack_.push_back(pop_sample());
    }

    void push_witness(const u32_type& v) override {
        auto sample = pop_sample();
        builder_ << sample;
        this->stack_.push_back(v);
        this->zstack_.push_back(sample);
    }

    field_poly encode(const u32_type&) override {
        assert(false);
        // auto p = Base::encode(v);
        // // print(p);
        // field_poly sp(sample_index_.size());
        // #pragma omp parallel for
        // for (size_t i = 0; i < sample_index_.size(); i++) {
        //     sp[i] = p[sample_index_[i]];
        // }
        // return sp;
    }

    field_poly& encode_const(s32 k) override {
        if (this->consts_.contains(k)) {
            return this->consts_[k];
        }
        else {
            field_poly p(this->encoder_.plain_size(), k);
            this->encoder_.encode_const(p);
            
            field_poly sp(sample_index_.size());
            #pragma omp parallel for
            for (size_t i = 0; i < sample_index_.size(); i++) {
                sp[i] = p[sample_index_[i]];
            }
            
            auto [pair, _] = this->consts_.emplace(k, std::move(sp));
            auto& [k, v] = *pair;
            return v;
        }
    }

    void assert_quadratic(const field_poly& z, const field_poly& x, const field_poly& y) override {
        arg_.update_quadratic(x, y, z);
    }

    void process_witness(const field_poly& p) override {
        builder_ << p;
        this->arg_.update_code(p);
    }

    var_type eval(zkp_ops::mul, const var_type& x, const var_type& y) override {
        u32_type v = x.val() * y.val();
        auto p = pop_sample();
        process_witness(p);
        assert_quadratic(p, x.poly(), y.poly());  // p = x * y
        return var_type{ std::move(v), std::move(p) };
    }

    var_type eval(zkp_ops::div, const var_type& x, const var_type& y) override {
        u32_type v = x.val() / y.val();
        auto p = pop_sample();
        process_witness(p);
        assert_quadratic(x.poly(), p, y.poly());  // p = x / y ==> x = p * y
        return var_type{ std::move(v), std::move(p) };
    }

    var_type eval(zkp_ops::index_of op, const var_type& x) override {
        size_t index = op.index;
        u32_type v = x.val();
        u32_type bit = (v >> index) & 1U;
        auto p = pop_sample();
        process_witness(p);
        assert_quadratic(p, p, p);  // x^2 - x = 0
        return var_type { std::move(bit), std::move(p) };
    }

    auto&& builder() {
        return std::move(builder_);
    }

    const auto& get_argument() const { return arg_; }

protected:
    std::vector<size_t> sample_index_;
    std::vector<field_poly> sampled_val_;
    typename merkle_tree<Hasher>::builder builder_;
    quasi_argument<Fp, RandomEngine> arg_;

    field_poly pop_sample() {
        field_poly p = sampled_val_.back();
        sampled_val_.pop_back();
        return p;
    }
};

}  // namespace ligero::vm::zkp
