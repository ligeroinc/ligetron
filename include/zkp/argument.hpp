#pragma once

#include <poly_field.hpp>
#include <relation.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

namespace ligero::zkp {

using namespace boost;

template <typename Poly, typename RandomEngine>
requires requires { typename RandomEngine::seed_type; }
struct quasi_argument {
    using value_type = typename Poly::value_type;

    quasi_argument(size_t poly_size, const typename RandomEngine::seed_type& seed)
        : code_(poly_size), linear_(poly_size), quad_(poly_size),
          rc_(seed), rl_(seed), rq_(seed)
        { }

    quasi_argument& dispatch(const relation<Poly> *r) {
        if (auto *p = dynamic_cast<const linear_relation<Poly>*>(r); p != nullptr) {
            return dispatch(p);
        }
        else if (auto *p = dynamic_cast<const quadratic_relation<Poly>*>(r); p != nullptr) {
            return dispatch(p);
        }
        else {
            throw std::runtime_error("Unexpected relation type");
        }
    }
    
    quasi_argument& dispatch(const linear_relation<Poly>* p) {
        return dispatch_code(*p).dispatch_linear(*p);
    }

    quasi_argument& dispatch(const quadratic_relation<Poly> *p) {
        return dispatch_code(*p).dispatch_quadratic(*p);
    }
    
    quasi_argument& dispatch_code(const binary_relation<Poly>& r) {
        static random::uniform_int_distribution<value_type> dist(value_type{0}, Poly::modulus - value_type{1});
        code_.fma_mod(r.a(), dist(rc_));
        code_.fma_mod(r.b(), dist(rc_));
        code_.fma_mod(r.c(), dist(rc_));
        return *this;
    }

    quasi_argument& dispatch_linear(const linear_relation<Poly>& r) {
        static random::uniform_int_distribution<value_type> dist(value_type{0}, Poly::modulus - value_type{1});
        linear_.fma_mod(r.eval(), dist(rl_));
        return *this;
    }

    quasi_argument& dispatch_quadratic(const quadratic_relation<Poly>& r) {
        static random::uniform_int_distribution<value_type> dist(value_type{0}, Poly::modulus - value_type{1});
        quad_.fma_mod(r.eval(), dist(rq_));
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

// template <typename Poly>
// struct code_argument {
//     using value_type = typename Poly::value_type;
    
//     code_argument() = default;
//     explicit code_argument(const Poly& acc) : row_(acc) { }

//     template <typename RandomEngine>
//     code_argument& update(const Poly& poly, RandomEngine& engine) {
//         random::uniform_int_distribution<value_type> dist(value_type{0}, Poly::modulus - value_type{1});
//         const value_type randomness = dist(engine);

//         if (row_.empty()) {
//             row_ = Poly(poly.size());     /* Initialize with zero filled polynomial */
//         }

//         row_.fma_mod(poly, randomness);
//         return *this;
//     }

//     template <typename Iter, typename RandomEngine>
//     code_argument& update(Iter begin, Iter end, RandomEngine& engine) {
//         using content_type = typename std::iterator_traits<Iter>::value_type;
        
//         size_t len = std::distance(begin, end);
//         random::uniform_int_distribution<value_type> dist(value_type{0}, Poly::modulus - value_type{1});
//         // std::vector<value_type> randomness;

//         // for (auto it = begin; it != end; it++) {
//         //     if (auto *p = dynamic_cast<binary_relation<Poly>*>(it->get()); p != nullptr) {
//         //         std::generate_n(std::back_inserter(randomness), 3, [&dist, &engine] { return dist(engine); });
//         //     }
//         //     else {
//         //         randomness.push_back(dist(engine));
//         //     }
//         // }

//         size_t i;
//         Poly acc(begin->size());

//         // #pragma omp declare reduction(fma : Poly : omp_out+=omp_in) initializer (omp_priv=omp_orig)
//         // #pragma omp parallel reduction(fma : acc) private(i)
//         for (i = 0; i < len; i++) {
//             auto it = std::next(begin, i);
//             if (auto *p = dynamic_cast<binary_relation<Poly>*>(it->get()); p != nullptr) {
//                 acc.fma_mod(it->a(), dist(engine));
//                 acc.fma_mod(it->b(), dist(engine));
//                 acc.fma_mod(it->c(), dist(engine));
//             }
//             else {
//                 acc.fma_mod(*std::next(begin, i), dist(engine));
//             }
//         }

//         if (row_.empty()) {
//             row_ = std::move(acc);
//         }
//         else {
//             row_ += acc;
//         }
//         return *this;
//     }
    
// protected:
//     Poly row_;
// };


// template <typename Poly>
// struct linear_argument {
//     using value_type = typename Poly::value_type;
    
//     linear_argument() = default;
//     explicit linear_argument(const Poly& acc) : row_(acc) { }

//     template <typename RandomEngine>
//     linear_argument& update(const linear_relation<Poly>& r, RandomEngine& engine) {
//         random::uniform_int_distribution<value_type> dist(value_type{0}, Poly::modulus - value_type{1});
//         const value_type randomness = dist(engine);

//         if (row_.empty()) {
//             row_ = Poly(r.a().size());     /* Initialize with zero filled polynomial */
//         }

//         row_.fma_mod(r.eval(), randomness);
//         return *this;
//     }

//     template <typename Iter, typename RandomEngine>
//     linear_argument& update(Iter begin, Iter end, RandomEngine& engine) {
//         size_t len = std::distance(begin, end);
//         random::uniform_int_distribution<value_type> dist(value_type{0}, Poly::modulus - value_type{1});
//         std::vector<value_type> randomness;
//         std::generate_n(std::back_inserter(randomness), len, [&dist, &engine] { return dist(engine); });

//         size_t i;
//         Poly acc((*begin).size());

//         #pragma omp declare reduction(fma : Poly : omp_out+=omp_in) initializer (omp_priv=omp_orig)
//         #pragma omp parallel reduction(fma : acc) private(i)
//         for (i = 0; i < len; i++) {
//             auto& ref = dynamic_cast<linear_relation<Poly>&>(**std::next(begin, i));
//             acc.fma_mod(ref.eval(), randomness[i]);
//         }

//         if (row_.empty()) {
//             row_ = std::move(acc);
//         }
//         else {
//             row_ += acc;
//         }
//         return *this;
//     }
// protected:
//     Poly row_;
// };


// template <typename Poly, typename PRNG>
// struct challenge_context {
//     challenge_context()
//         : code_row_(std::nullopt), linear_row_(std::nullopt), quad_row_(std::nullopt), prng_() { }
//     challenge_context(const typename PRNG::seed_type& seed)
//         : code_row_(std::nullopt), linear_row_(std::nullopt), quad_row_(std::nullopt), prng_(seed) { }

//     #pragma omp declare reduction(fma : Poly : omp_out+=omp_in) initializer (omp_priv=omp_orig)

//     challenge_context& update_rows(const std::vector<Poly>& polys) {
//         using element = typename Poly::value_type;
//         std::vector<element> randomness;
        
//         std::generate_n(std::back_inserter(randomness), polys.size(),
//                       [this] { return prng_.template uniform_fdr<element>(Poly::modulus); });
        
//         /* Perform Code Test */
//         {
//             size_t i;
//             Poly acc(polys[0].size());
            
//             #pragma omp parallel reduction(fma:acc) private(i)
//             for (i = 0; i < polys.size(); i++) {
//                 acc.fma_mod(polys[i], randomness[i]);  // acc += polys[i] * randomness[i]
//             }
        
//             if (!code_row_)
//                 code_row_ = std::move(acc);
//             else
//                 *code_row_ += acc;
//         }
        
//         return *this;
//     }

//     challenge_context& linear(const std::vector<Poly>& polys) {
//         using element = typename Poly::value_type;
//         std::vector<element> randomness;
//         const size_t gate_size = polys.size() / 3;
//         std::generate_n(std::back_inserter(randomness), gate_size,
//                         [this] { return prng_.template uniform_fdr<element>(Poly::modulus); });

//         {
//             size_t i;
//             Poly acc(polys[0].size());

//             std::vector<Poly> tmp(gate_size);
//             #pragma omp parallel for
//             for (i = 0; i < gate_size; i++) {
//                 const size_t j = i * 3;
//                 tmp[i] = polys[j];
//                 tmp[i] += polys[j + 1];
//                 tmp[i] -= polys[j + 2];
//             }
            
//             #pragma omp parallel reduction(fma:acc) private(i)
//             for (i = 0; i < tmp.size(); i++) {
//                 acc.fma_mod(tmp[i], randomness[i]);
//             }

//             if (!linear_row_)
//                 linear_row_ = std::move(acc);
//             else
//                 *linear_row_ += acc;
//         }
//         return *this;
//     }

//     challenge_context& quadratic(const std::vector<Poly>& polys) {
//         using element = typename Poly::value_type;
//         std::vector<element> randomness;
//         std::generate_n(std::back_inserter(randomness), polys.size(),
//                       [this] { return prng_.template uniform_fdr<element>(Poly::modulus); });

//         const size_t gate_size = polys.size() / 3;

//         {
//             size_t i;
//             Poly acc(polys[0].size());

//             std::vector<Poly> tmp(gate_size);
//             #pragma omp parallel for
//             for (i = 0; i < gate_size; i++) {
//                 const size_t j = i * 3;
//                 tmp[i] = polys[j];
//                 tmp[i] *= polys[i + 1];
//                 tmp[i] -= polys[i + 2];
//             }
            
//             #pragma omp parallel reduction(fma:acc) private(i)
//             for (i = 0; i < tmp.size(); i++) {
//                 acc.fma_mod(tmp[i], randomness[i]);
//             }

//             if (!quad_row_)
//                 quad_row_ = std::move(acc);
//             else
//                 *quad_row_ += acc;
//         }
//         return *this;
//     }

//     std::optional<Poly>& linear_row() { return linear_row_; }
//     std::optional<Poly>& quadratic_row() { return quad_row_; }
    
// protected:
//     std::optional<Poly> code_row_, linear_row_, quad_row_;
//     PRNG prng_;
// };

}  // namespace ligero::zkp
