#pragma once

#include <memory>
#include <vector>
#include <optional>

#include <params.hpp>
#include <base.hpp>
#include <util/timer.hpp>

namespace ligero::vm::zkp {

struct ref_expr_base { };

template <typename Expr>
concept IsRefExpr = std::derived_from<Expr, ref_expr_base>;

template <typename Op, typename... Args>
struct ref_expr : ref_expr_base {
    Op op_;
    std::tuple<Args...> args_;

    ref_expr(Op op, Args... args) : op_(op), args_(args...) { }

    template <typename Evaluator>
    auto eval(Evaluator& e) {
        return std::apply([&](auto&&... args) {
            return e.eval(op_, args.eval(e)...);
        }, std::move(args_));
    }
};

template <IsRefExpr Op1, IsRefExpr Op2>
auto operator+(Op1 op1, Op2 op2) {
    return ref_expr(zkp_ops::add{}, std::move(op1), std::move(op2));
}

template <IsRefExpr Op1, IsRefExpr Op2>
auto operator-(Op1 op1, Op2 op2) {
    return ref_expr(zkp_ops::sub{}, std::move(op1), std::move(op2));
}

template <IsRefExpr Op1, IsRefExpr Op2>
auto operator*(Op1 op1, Op2 op2) {
    return ref_expr(zkp_ops::mul{}, std::move(op1), std::move(op2));
}

template <IsRefExpr Op1, IsRefExpr Op2>
auto operator/(Op1 op1, Op2 op2) {
    return ref_expr(zkp_ops::div{}, std::move(op1), std::move(op2));
}

// ------------------------------------------------------------ //

template <typename Poly>
struct gc_row {
    using poly_type = Poly;
    using field_type = typename Poly::field_type;
    using value_type = typename Poly::value_type;

    struct location {
        location(gc_row* p, size_t off) : ptr(p), offset(off) { }
        
        gc_row *ptr;
        size_t offset;
        int count = 0;
    };

    struct reference : ref_expr_base {
        reference() : loc_(nullptr) { }
        // reference(std::shared_ptr<location> loc) : loc_(std::move(loc)) { }
        reference(location *ptr) : loc_(ptr) {
            if (loc_) {
                loc_->count++;
            }
        }

        reference(const reference& ref) : loc_(ref.loc_) {
            if (loc_) {
                loc_->count++;
            }
        }
        
        reference& operator=(const reference& ref) {
            if (loc_) {
                loc_->count--;
            }
            loc_ = ref.loc_;
            if (loc_) {
                loc_->count++;
            }
            return *this;
        }

        ~reference() {
            if (loc_) {
                loc_->count--;
            }
        }

        s32 val() const {
            assert(loc_);
            assert(loc_->ptr != nullptr);
            return loc_->ptr->val_[loc_->offset];
        }

        value_type& rand(size_t idx) { return loc_->ptr->randoms_[idx][loc_->offset]; }
        const value_type& rand(size_t idx) const { return loc_->ptr->randoms_[loc_->offset]; }

        location* loc() { return loc_; }

        template <typename... Args>
        auto eval(Args&&...) { return *this; }

        // auto operator[](size_t i) const {
        //     return ref_expr(zkp_ops::index_of{ i }, *this);
        // }
        
    protected:
        location* loc_;
        // location *loc_;
    };

    // struct decomposed_reference {
    //     decomposed_reference(reference ref, std::pair<field_type, field_type> random)
    //         : ref_(std::move(ref)), randomness_(std::move(random)) { }

    //     auto operator[](size_t i) const {
    //         return packed_ref_expr(zkp_ops::index_of{i}, randomness_, ref_);
    //     }
        
    // protected:
    //     reference ref_;
    //     std::pair<field_type, field_type> randomness_;
    // };

    gc_row() = default;
    gc_row(size_t packing_size)
        : packing_size_(packing_size),
          val_(packing_size), randoms_(params::num_linear_test, Poly(packing_size))
        {
            for (size_t i = 0; i < packing_size; i++) {
                count_.emplace_back(std::make_unique<location>(this, i));
            }
        }

    // Disable copy to avoid increasing unnecessary reference counting
    gc_row(const gc_row&) = delete;
    gc_row& operator=(const gc_row&) = delete;

    // Enable move
    gc_row(gc_row&&) = default;
    gc_row& operator=(gc_row&&) = default;
    // gc_row(gc_row&& o)
    //     : packing_size_(o.packing_size_),
    //       curr_(std::move(o.curr_)),
    //       val_(std::move(o.val_)),
    //       random_(std::move(o.random_)),
    //       count_(std::move(o.count_)) { }
    
    // gc_row& operator=(gc_row&& o) {
    //     assert(packing_size_ == o.packing_size_);
    //     std::swap(curr_, o.curr_);
    //     std::swap(val_, o.val_);
    //     std::swap(random_, o.random_);
    //     std::swap(count_, o.count_);
    //     return *this;
    // }

    size_t size() const { return curr_; }
    size_t capacity() const { return packing_size_; }
    bool available() const { return curr_ < packing_size_; }

    auto& val() { return val_; }
    const auto& val() const { return val_; }
    auto& randoms() { return randoms_; }
    const auto& randoms() const { return randoms_; }

    auto val_begin() { return val_.begin(); }
    auto val_begin() const { return val_.begin(); }
    auto val_end() { return val_.end(); }
    auto val_end() const { return val_.end(); }
    
    // auto rand_begin() { return random_.begin(); }
    // auto rand_begin() const { return random_.begin(); }
    // auto rand_end() { return random_.end(); }
    // auto rand_end() const { return random_.end(); }

    reference push_back(s32 val) {
        assert(curr_ < packing_size_);
        const size_t idx = curr_++;
        
        val_[idx] = val;

        // auto t = make_timer("shared_ptr");
        return reference{ count_[idx].get() };
        
        // auto ptr = std::make_shared<value_type*>(&random_[idx]);
        // *ptr = &random_[idx];
        // count_[idx] = ptr;

        // return reference{ &val_[idx], std::move(ptr) };
    }

    std::optional<reference> try_push_back(s32 val) {
        if (available()) {
            return push_back(val);
        }
        else {
            return std::nullopt;
        }
    }

    template <typename RandomDist>
    // std::optional<std::unique_ptr<gc_row>>
    bool mark_and_sweep(gc_row* next_gen, RandomDist& dist, gc_row* extra = nullptr) {
        assert(packing_size_ == next_gen->packing_size_);

        // auto t = make_timer("GC", "MarkSweep");

        // std::cout << "gc triggered" << std::endl;

        size_t copy_count = 0;
        // std::optional<std::unique_ptr<gc_row>> extra_row = std::nullopt;
        bool recursive_gc = false;
        for (size_t i = 0; i < count_.size(); i++) {
            // Still referenced by other variable
            if (count_[i]->count > 0) {
                copy_count++;

                auto ref = next_gen->try_push_back(val_[i]);

                // Next gen is also full - avoid recursion by creating a fresh row
                if (!ref) {
                    recursive_gc = true;
                    // throw std::runtime_error("GC failed: next gen is also full");

                    if (extra) {
                        assert(extra->size() == 0);
                        next_gen->mark_and_sweep(extra, dist);

                        // IMPORTANT: pointer swap only happens in this scope,
                        //            outside scope unique_ptr remains unchanged.
                        std::swap(next_gen, extra);
                        
                        ref = next_gen->try_push_back(val_[i]);
                    }
                    else {
                        throw std::runtime_error("Recursive GC without extra row failed");
                    }
                    // auto fresh = std::make_unique<gc_row>(packing_size_);
                    // auto exr = next_gen->mark_and_sweep(fresh, dist);
                    
                    // assert(!exr); // TODO: handle grow case
                    
                    // std::swap(next_gen, fresh);
                    // // next_gen->update_this();
                    
                    // extra_row.emplace(std::move(fresh));

                    // // Try push again
                    // ref = next_gen->try_push_back(val_[i]);
                }

                // At this point `ref` is guaranteed good
                assert(ref);
                location* loc = ref->loc();
                
                // const size_t idx = next_gen.curr_++;
                // next_gen.val_[idx] = val_[i];

                // Generating Equality constraint
                if constexpr (RandomDist::enabled) {
                    for (size_t ri = 0; ri < params::num_linear_test; ri++) {
                        field_type r = dist();

                        // ptr still point to old generation's randomness
                        // auto ptr = std::move(count_[i]);
                
                        // Adjust randomness for old generation
                        field_type rd = field_type{randoms_[ri][i]} - r;
                        randoms_[ri][i] = rd.data();

                        // ptr now points to new generation's randomness
                        // *ptr = &next_gen.random_[idx];

                        // Adjust randomness for new generation too
                        field_type refd = field_type{ref->rand(ri)} + r;
                        ref->rand(ri) += refd.data();
                    }
                }
                // next_gen.random_[idx] += r;

                // std::cout << "this " << this << ", ptr " << count_[i]->ptr;
                // std::cout << " next " << next_gen.count_[idx]->ptr;
                
                // next_gen->count_[loc.offset] = count_[i];
                // *next_gen->count_[loc.offset] = loc;
                auto& curr_ptr = count_[i];
                auto& next_ptr = next_gen->count_[loc->offset];
                
                std::swap(curr_ptr->ptr, next_ptr->ptr);
                std::swap(curr_ptr->offset, next_ptr->offset);
                std::swap(curr_ptr, next_ptr);
            }
        }
        
        return recursive_gc;
    }

    void reset() {
        curr_ = 0;
        std::fill(val_.begin(), val_.end(), 0);
        for (auto& random : randoms_) {
            std::fill(random.begin(), random.end(), 0);
        }
        for (size_t i = 0; i < packing_size_; i++) {
            // count_[i]->ptr = this;
            // count_[i]->offset = i;
            // count_[i]->count = 1;
            if (count_[i]->count > 0) {
                // count_[i] = std::make_shared<location>(this, i);
                throw std::runtime_error("GC: reset a position being ref counted");
            }
            else {
                // count_[i]->ptr = this;
                // count_[i]->offset = i;
                count_[i]->ptr = this;
                count_[i]->offset= i;
                count_[i]->count = 0;
            }
        }
    }
    
protected:
    size_t curr_ = 0;
    size_t packing_size_;
    std::vector<s32> val_;
    std::vector<Poly> randoms_;
    std::vector<std::unique_ptr<location>> count_;
};


template <typename Poly, typename RandomDist>
struct gc_managed_region {
    using field_type = typename Poly::field_type;
    using value_type = typename Poly::value_type;
    using signed_value_type = typename Poly::signed_value_type;
    
    using row_type = gc_row<Poly>;
    using ref_type = typename row_type::reference;
    using row_ptr = std::unique_ptr<row_type>;

    gc_managed_region(size_t packing_size, RandomDist& dist)
        : packing_size_(packing_size),
          dist_(dist),
          quad_l_(std::make_unique<row_type>(packing_size)),
          quad_r_(std::make_unique<row_type>(packing_size)),
          quad_o_(std::make_unique<row_type>(packing_size)),
          next_linear_(std::make_unique<row_type>(packing_size)),
          next_ql_(std::make_unique<row_type>(packing_size)),
          next_qr_(std::make_unique<row_type>(packing_size)),
          next_qo_(std::make_unique<row_type>(packing_size))
        {
            linear_.emplace_back(std::make_unique<row_type>(packing_size_));
        }

    template <typename Func>
    void on_linear(Func f) {
        on_linear_ = std::move(f);
    }

    template <typename Func>
    void on_quadratic(Func f) {
        on_quad_ = std::move(f);
    }

    void build_linear(ref_type& z, ref_type& x, ref_type& y) {
        // auto t = make_timer(__func__);
        if constexpr (RandomDist::enabled) {
            auto t1 = make_timer("Random", __func__);
            for (size_t ri = 0; ri < params::num_linear_test; ri++) {
                auto r = static_cast<signed_value_type>(dist_());
                field_type pz = static_cast<signed_value_type>(z.rand(ri)) - r;
                field_type px = static_cast<signed_value_type>(x.rand(ri)) + r;
                field_type py = static_cast<signed_value_type>(y.rand(ri)) + r;
                z.rand(ri) = pz.data();
                x.rand(ri) = px.data();
                y.rand(ri) = py.data();
            }
        }
    }

    void build_equal(ref_type& z, ref_type& x) {
        // auto t = make_timer(__func__);
        if constexpr (RandomDist::enabled) {
            auto t1 = make_timer("Random", __func__);
            for (size_t ri = 0; ri < params::num_linear_test; ri++) {
                auto r = static_cast<signed_value_type>(dist_());
                field_type pz = static_cast<signed_value_type>(z.rand(ri)) - r;
                field_type px = static_cast<signed_value_type>(x.rand(ri)) + r;
                z.rand(ri) = pz.data();
                x.rand(ri) = px.data();
            }
        }
    }

    // std::pair<ref_type, std::optional<row_type>> push_back_at(row_type& curr_row, const u32& val) {
    //     if (auto ref = try_push_back(curr_row, val); ref) {
    //         return std::make_pair(ref, std::nullopt);
    //     }
    //     else {
    //         old_row = copy_and_replace(curr_row);
    //         auto ref = try_push_back(curr_row, val);
    //         assert(ref);  // TODO: handle the case gc cannot free enough space
    //         return std::make_pair(*ref, std::move(old_row));
            
    //         // // Perform GC if current row is not available
    //         // row_type next_gen(packing_size_);
    //         // size_t copied = curr_row.mark_sweep(next_gen, dist_);

    //         // std::swap(curr_row, next_gen);

    //         // // Important: update pointer to latest address
    //         // curr_row.update_this();
            
    //         // auto ref = curr_row.push_back(val);
    //         // return std::make_pair(std::move(ref), std::move(next_gen));
            
    //     }
    // }

    void replace_linear(row_ptr& row) {
        row->mark_and_sweep(next_linear_.get(), dist_);
        std::swap(row, next_linear_);
        on_linear_(*next_linear_);
        next_linear_->reset();
    }

    void replace_quadratic(row_ptr& x, row_ptr& y, row_ptr& z) {
        // Instead of copying to a new generation, copying to linear row
        row_ptr& curr_linear = linear_.back();

        auto rx = x->mark_and_sweep(curr_linear.get(), dist_, next_linear_.get());
        if (rx) {
            std::swap(curr_linear, next_linear_);
            on_linear_(*next_linear_);
            next_linear_->reset();
        }

        auto ry = y->mark_and_sweep(curr_linear.get(), dist_, next_linear_.get());
        if (ry) {
            std::swap(curr_linear, next_linear_);
            on_linear_(*next_linear_);
            next_linear_->reset();
        }

        auto rz = z->mark_and_sweep(curr_linear.get(), dist_, next_linear_.get());
        if (rz) {
            std::swap(curr_linear, next_linear_);
            on_linear_(*next_linear_);
            next_linear_->reset();
        }

        // auto t3 = make_timer("Region", __func__, "make");
        // auto next_x = std::make_unique<row_type>(packing_size_);
        // auto next_y = std::make_unique<row_type>(packing_size_);
        // auto next_z = std::make_unique<row_type>(packing_size_);
        // t3.stop();

        std::swap(x, next_ql_);
        std::swap(y, next_qr_);
        std::swap(z, next_qo_);
        // x->update_this();
        // y->update_this();
        // z->update_this();


        on_quad_(*next_ql_, *next_qr_, *next_qo_);
        // on_quad_(nullptr, nullptr, nullptr);

        next_ql_->reset();
        next_qr_->reset();
        next_qo_->reset();
    }

    // row_type copy_and_replace(row_type& curr_row) {
    //     // Perform GC if current row is not available
    //     assert(!curr_row.available());

    //     row_type next_gen(packing_size_);
    //     size_t copied = curr_row.mark_sweep(next_gen, dist_);

    //     std::swap(curr_row, next_gen);

    //     // Important: update pointer to latest address
    //     curr_row.update_this();

    //     // At this point `next_gen` holds the full row with all reference invalidated
    //     return next_gen;
    // }

    ref_type push_back_linear(s32 val) {
        if (!linear_.back()->available()) {
            replace_linear(linear_.back());
        }
        return linear_.back()->push_back(val);
        // if (auto ref = linear_.back()->try_push_back(val); ref) {
        //     return *ref;
        // }
        // else {
        //     replace_linear(linear_.back());
        //     return *linear_.back()->try_push_back(val);
        // }
    }

    ref_type push_back_quad(row_ptr& row, s32 val) {
        if (!row->available()) {
            replace_quadratic(quad_l_, quad_r_, quad_o_);
        }
        return row->push_back(val);
        // if (auto ref = row->try_push_back(val); ref) {
        //     t.stop();
        //     return *ref;
        // }
        // else {
        //     replace_quadratic(quad_l_, quad_r_, quad_o_);
        //     return *row->try_push_back(val);
        // }
    }

    ref_type multiply(ref_type& x, ref_type& y) {
        assert(quad_l_->size() == quad_r_->size() && quad_r_->size() == quad_o_->size());

        auto refl = push_back_quad(quad_l_, x.val());
        build_equal(refl, x);

        auto refr = push_back_quad(quad_r_, y.val());
        build_equal(refr, y);

        auto z = x.val() * y.val();
        auto refo = push_back_quad(quad_o_, z);
        
        return refo;
    }

    ref_type divide(ref_type& x, ref_type& y) {
        auto z = x.val() / y.val();
        
        auto refl = push_back_quad(quad_l_, z);
        
        auto refr = push_back_quad(quad_r_, y.val());
        build_equal(refr, y);
        
        auto refo = push_back_quad(quad_o_, x.val());
        build_equal(refo, x);

        return refl;
    }

    std::pair<field_type, field_type> adjust_random(ref_type& ref, size_t idx) {
        // auto t = make_timer("Random", __func__);
        field_type rf(ref.rand(idx), no_reduce_coeffs);
        field_type rl(dist_(), no_reduce_coeffs);
        field_type rr(dist_(), no_reduce_coeffs);
        // ref.rand() = rf - rl - rr;
        // auto tmp = rl.data();
        // DEBUG << "Val: " << ref.val();
        // DEBUG << "Before Random: " << rf.data();
        // DEBUG << "R: " << tmp;
        ref.rand(idx) = rf - rl - rr;
            // - rl * field_type(ref.val(), no_reduce_coeffs)
            // - rr * field_type(ref.val(), no_reduce_coeffs);
        return std::make_pair(rl, rr);
    }

    // ref_type index(const ref_type& x, u32 i) {
    //     auto xv = x.val();
    //     u32 bit = (xv >> i) & u32{1};
    //     auto rl = push_back_quad(quad_l_, bit);
    //     auto rr = push_back_quad(quad_r_, bit);
    //     auto ro = push_back_quad(quad_o_, bit);

    //     // if constexpr (RandomDist::enabled) {
    //     //     field_type r1(dist_(), no_reduce_coeffs);
    //     //     field_type r2(dist_(), no_reduce_coeffs);
    //     //     x.rand() = field_type(x.rand(), no_reduce_coeffs) - r1 - r2;
            
    //     // }
        
    //     return ro;
    // }

    ref_type index_sign(const ref_type& x, u32 i, const std::vector<std::pair<field_type, field_type>>& randomness) {
        auto xv = x.val();
        u32 bit = (xv >> i) & u32{1};
        auto rl = push_back_quad(quad_l_, bit);
        auto rr = push_back_quad(quad_r_, bit);
        auto ro = push_back_quad(quad_o_, bit);

        if constexpr(RandomDist::enabled) {
            assert(randomness.size() == params::num_linear_test);

            const field_type shift = -(static_cast<signed_value_type>(1) << i);
            for (size_t ri = 0; ri < randomness.size(); ri++) {
                const auto& [left, right] = randomness[ri];
                field_type l(rl.rand(ri), no_reduce_coeffs);
                field_type r(rr.rand(ri), no_reduce_coeffs);

                // Use -2^31 to compensate the difference
                rl.rand(ri) = l + left * shift;
                rr.rand(ri) = r + right * shift;
            }
        }
        
        return ro;
    }

    ref_type index(const ref_type& x, u32 i, const std::vector<std::pair<field_type, field_type>>& randomness) {
        // std::cout << "index " << x.loc_->ptr << " " << x.loc_->offset << std::endl;
        // std::cout << "linear " << linear_.back().count_[x.loc_->offset]->ptr << std::endl;
        auto xv = x.val();
        u32 bit = (xv >> i) & u32{1};
        auto rl = push_back_quad(quad_l_, bit);
        auto rr = push_back_quad(quad_r_, bit);
        auto ro = push_back_quad(quad_o_, bit);

        if constexpr(RandomDist::enabled) {
            // auto t = make_timer("Random", __func__);
            assert(randomness.size() == params::num_linear_test);

            const field_type shift = static_cast<value_type>(1) << i;
            for (size_t ri = 0; ri < params::num_linear_test; ri++) {
                const auto& [left, right] = randomness[ri];
                field_type l(rl.rand(ri), no_reduce_coeffs);
                field_type r(rr.rand(ri), no_reduce_coeffs);

                rl.rand(ri) = l + left * shift;
                rr.rand(ri) = r + right * shift;
            }
        }
        
        return ro;
    }

    void finalize() {
        for (auto& region : linear_) {
            // std::cout << "finalize linear row of size " << region.size() << std::endl;
            on_linear_(*region);
        }
        // std::cout << "finalize quad row of size " << quad_l_.size() << std::endl;
        // std::cout << "finalize quad row of size " << quad_r_.size() << std::endl;
        // std::cout << "finalize quad row of size " << quad_o_.size() << std::endl;
        on_quad_(*quad_l_, *quad_r_, *quad_o_);
    }

protected:
    size_t packing_size_;
    RandomDist& dist_;
    std::vector<row_ptr> linear_;
    row_ptr quad_l_, quad_r_, quad_o_;

    row_ptr next_linear_;
    row_ptr next_ql_, next_qr_, next_qo_;

    std::function<void(row_type&)> on_linear_;
    std::function<void(row_type&, row_type&, row_type&)> on_quad_;
};

}  // namespace ligero::vm::zkp
