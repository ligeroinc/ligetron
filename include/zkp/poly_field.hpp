#pragma once

#include <random>
#include <memory>

#include <hexl/hexl.hpp>

#include <zkp/prime_field.hpp>

namespace ligero::vm::zkp {

using namespace intel;

template <typename> struct poly_base { };

template <uint64_t Modulus>
struct primitive_poly : public poly_base<primitive_poly<Modulus>> {
    using value_type = uint64_t;
    using field_type = prime_field<Fp64<Modulus>>;

    constexpr static value_type modulus = Modulus;

    primitive_poly() : data_() { }
    explicit primitive_poly(size_t n) : data_(n, 0) { }
    primitive_poly(size_t n, value_type v) : data_(n, v) { }
    primitive_poly(std::initializer_list<value_type> vals) : data_(vals) { }
    
    template <typename Iter>
    primitive_poly(Iter begin, Iter end) : data_(begin, end) { }

    // primitive_poly(const primitive_poly& other) : data_(other.data_) { }
    // primitive_poly(primitive_poly&& other) : data_(std::move(other.data_)) { }

    template <value_type M> requires (M <= Modulus)
        primitive_poly(const primitive_poly<M>& other) : data_(other.data()) { }

    template <value_type M> requires (M <= Modulus)
        primitive_poly(primitive_poly<M>&& other) : data_(std::move(other.data())) { }

    bool operator==(const primitive_poly& o) const {
        if (size() != o.size())
            return false;
        for (size_t i = 0; i < size(); i++) {
            if (data_[i] != o.data_[i])
                return false;
        }
        return true;
    }

    bool operator!=(const primitive_poly& o) const { return !(*this == o); }

    auto begin() { return data_.begin(); }
    auto end()   { return data_.end(); }
    auto cbegin() const { return data_.cbegin(); }
    auto cend()   const { return data_.cend(); }

    auto& data() { return data_; }
    const auto& data() const { return data_; }

    size_t size() const { return data_.size(); }
    void resize(size_t N) { data_.resize(N); }

    bool empty() const { return data_.empty(); }
    void clear() { data_.clear(); }

    primitive_poly& push_back(value_type&& v) {
        data_.push_back(std::forward<value_type>(v));
        return *this;
    }

    template <typename... Args>
    primitive_poly& emplace_back(Args&&... args) {
        data_.emplace_back(std::forward<Args>(args)...);
        return *this;
    }

    primitive_poly& pad(size_t N, value_type v = 0) {
        if (size_t old_size = data_.size(); N > old_size) {
            data_.resize(N);
            #pragma omp simd
            for (size_t i = old_size; i < N; i++)
                data_[i] = v;
        }
        return *this;
    }

    primitive_poly& pad_random(size_t N) {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dist(0, modulus - 1);

        if (size_t old = data_.size(); N > old) {
            data_.resize(N);
            for (size_t i = old; i < N; i++)
                data_[i] = dist(gen);
        }
        return *this;
    }
    
    const value_type& operator[](size_t index) const { return data_[index]; }
    value_type& operator[](size_t index) { return data_[index]; }

    void reduce(size_t in = 1, size_t out = 1) {
        hexl::EltwiseReduceMod(data_.data(), data_.data(), data_.size(), modulus, in, out);
    }

    // void ntt() { ntt_.ComputeForward(data_.data(), data_.data(), 1, 1); }
    // void invntt() { ntt_.ComputeInverse(data_.data(), data_.data(), 1, 1); }

    primitive_poly& operator+=(const primitive_poly& o) {
        hexl::EltwiseAddMod(data_.data(), data_.data(), o.data().data(), data_.size(), modulus);
        return *this;
    }

    primitive_poly& operator+=(value_type v) {
        hexl::EltwiseAddMod(data_.data(), data_.data(), v, data_.size(), modulus);
        return *this;
    }

    primitive_poly& operator-=(const primitive_poly& o) {
        hexl::EltwiseSubMod(data_.data(), data_.data(), o.data().data(), data_.size(), modulus);
        return *this;
    }

    primitive_poly& operator-=(value_type v) {
        hexl::EltwiseSubMod(data_.data(), data_.data(), v, data_.size(), modulus);
        return *this;
    }

    primitive_poly& operator*=(const primitive_poly& o) {
        hexl::EltwiseMultMod(data_.data(), data_.data(), o.data().data(), data_.size(), modulus, 1);
        return *this;
    }

    primitive_poly& operator*=(value_type v) {
        #pragma omp simd
        for (size_t i = 0; i < data_.size(); i++) {
            data_[i] = hexl::MultiplyMod(data_[i], v, modulus);
        }
        return *this;
    }

    primitive_poly& fma_mod(const primitive_poly& m, value_type scalar) {
        hexl::EltwiseFMAMod(data_.data(), m.data().data(), scalar, data_.data(), data_.size(), modulus, 1);
        return *this;
    }

    primitive_poly& fma_mod(const primitive_poly& a, value_type scalar, const primitive_poly& c) {
        hexl::EltwiseFMAMod(data_.data(), a.data().data(), scalar, c.data().data(), data_.size(), modulus, 1);
        return *this;
    }

    template <typename Archive>
    void serialize(Archive& ar, unsigned int version) {
        ar & data_;
    }
    
protected:
    hexl::AlignedVector64<uint64_t> data_;
};

template <size_t N, uint64_t Modulus>
struct NTT {
    static hexl::NTT& ntt_obj() {
        static hexl::NTT ntt{N, Modulus};
        return ntt;
    }

    static void forward(primitive_poly<Modulus>& p) {
        ntt_obj().ComputeForward(p.data().data(), p.data().data(), 1, 1);
    }

    static void inverse(primitive_poly<Modulus>& p) {
        ntt_obj().ComputeInverse(p.data().data(), p.data().data(), 1, 1);
    }
};

// template <template <auto> typename Op, typename... Args>
// struct poly_expr : public poly_non_terminal<poly_expr<Op, Args...>> {
//     std::tuple<const Args&...> args_;

//     using value_type = std::common_type_t<typename Args::value_type...>;
//     using first_type = typename std::tuple_element<0, std::tuple<Args...>>::type;
//     constexpr static value_type modulus = first_type::modulus;

//     constexpr poly_expr(const Args&... args) : args_(args...) { }

//     void eval(value_type *out) const {
//         return std::apply([](const Args&... ops) {
//             (Op<modulus>{}(), ...);
//         }, args_);
//     }
// };

}  // namespace ligero::zkp
