#pragma once

#include <zkp/poly_field.hpp>
// #include <relation.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

namespace ligero::vm::zkp {

using namespace boost;

template <typename Poly, typename RandomEngine>
requires requires { typename RandomEngine::seed_type; }
struct quasi_argument {
    using value_type = typename Poly::value_type;

    quasi_argument(size_t poly_size, const typename RandomEngine::seed_type& seed)
        : code_(poly_size), linear_(poly_size), quad_(poly_size),
          rc_(seed), rl_(seed), rq_(seed)
        { }

    bool operator==(const quasi_argument& other) const {
        return code_ == other.code_ && linear_ == other.linear_ && quad_ == other.quad_;
    }
    
    quasi_argument& update_code(const Poly& x) {
        static random::uniform_int_distribution<value_type> dist(value_type{0}, Poly::modulus - value_type{1});
        code_.fma_mod(x, dist(rc_));
        return *this;
    }

    quasi_argument& update_linear(const Poly& a, const Poly& b, const Poly& c) {
        static random::uniform_int_distribution<value_type> dist(value_type{0}, Poly::modulus - value_type{1});

        Poly r = a + b - c;
        linear_.fma_mod(r, dist(rl_));
        return *this;
    }

    quasi_argument& update_quadratic(const Poly& a, const Poly& b, const Poly& c) {
        static random::uniform_int_distribution<value_type> dist(value_type{0}, Poly::modulus - value_type{1});
        Poly r = a * b - c;
        quad_.fma_mod(r, dist(rq_));
        return *this;
    }

    template <typename Archive>
    void serialize(Archive& ar, unsigned int version) {
        ar & code_;
        ar & linear_;
        ar & quad_;
    }

    const Poly& code() const { return code_; }
    const Poly& linear() const { return linear_; }
    const Poly& quadratic() const { return quad_; }

protected:
    Poly code_, linear_, quad_;
    RandomEngine rc_, rl_, rq_;
};



template <typename Encoder, typename Row, typename RandomDist>
requires requires { typename RandomDist::seed_type; }
struct nonbatch_argument {
    using poly_type = typename Row::poly_type;
    using value_type = typename Row::value_type;

    nonbatch_argument(Encoder& encoder, size_t size, const typename RandomDist::seed_type& seed)
        : encoder_(encoder),
          code_(size), linear_(size), quad_(size),
          dc_(poly_type::modulus, seed),
          dl_(poly_type::modulus, seed),
          dq_(poly_type::modulus, seed)
        { }

    bool operator==(const nonbatch_argument& other) const {
        return code_ == other.code_ && linear_ == other.linear_ && quad_ == other.quad_;
    }
    
    nonbatch_argument& update_code(const poly_type& x) {
        code_.fma_mod(x, dc_());
        return *this;
    }

    // nonbatch_argument& update_code(const Row& r) {
    //     poly_type p(r.val_begin(), r.val_end());
    //     encoder_.encode(p);
    //     update_code(p);
    //     return *this;
    // }

    nonbatch_argument& update_linear(const poly_type& p, const poly_type& rand) {
        poly_type tmp = p * rand;
        linear_ += tmp;

        // std::cout << "linear: ";
        // for (const auto& v : linear_) {
        //     std::cout << v << " ";
        // }
        // std::cout << std::endl << std::endl;
        return *this;
    }

    // nonbatch_argument& update_linear(const Row& r) {
    //     poly_type p(r.val_begin(), r.val_end());
    //     encoder_.encode(p);

    //     poly_type random = r.random();
    //     // std::cout << "random: ";
    //     // for (const auto& r : random) {
    //     //     std::cout << r << " ";
    //     // }
    //     // std::cout << std::endl;
    //     encoder_.encode(random);

    //     update_code(p);
    //     update_linear(p, random);
    //     return *this;
    // }

    nonbatch_argument& update_quadratic(const poly_type& x, const poly_type& y, const poly_type& z) {
        poly_type p = x * y - z;
        quad_.fma_mod(p, dq_());
        return *this;
    }

    // nonbatch_argument& update_quadratic(const Row& a, const Row& b, const Row& c) {
    //     poly_type pa(a.val_begin(), a.val_end());
    //     poly_type pb(b.val_begin(), b.val_end());
    //     poly_type pc(c.val_begin(), c.val_end());
    //     poly_type ra(a.random()), rb(b.random()), rc(c.random());

    //     encoder_.encode(pa);
    //     encoder_.encode(pb);
    //     encoder_.encode(pc);

    //     encoder_.encode(ra);
    //     encoder_.encode(rb);
    //     encoder_.encode(rc);

    //     update_linear(pa, ra);
    //     update_linear(pb, rb);
    //     update_linear(pc, rc);
        
    //     update_quadratic(pa, pb, pc);
    //     return *this;
    // }

    template <typename Archive>
    void serialize(Archive& ar, unsigned int version) {
        ar & code_;
        ar & linear_;
        ar & quad_;
    }

    const poly_type& code() const { return code_; }
    const poly_type& linear() const { return linear_; }
    const poly_type& quadratic() const { return quad_; }

protected:
    Encoder& encoder_;
    poly_type code_, linear_, quad_;
    RandomDist dc_, dl_, dq_;
};

}  // namespace ligero::zkp
