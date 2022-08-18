#pragma once

#include <concepts>

namespace ligero::zkp {

template <typename Element>
struct relation { virtual ~relation() = default; };

// template <typename Element>
// struct unary_relation : relation {
//     using value_type = Element;

//     unary_relation(const value_type& a) : a_(a) { }

    
// };

template <typename Element>
struct binary_relation : public relation<Element> {
    using value_type = Element;

    binary_relation(const value_type& a, const value_type& b, const value_type& c)
        : a_(a), b_(b), c_(c) { }
    virtual ~binary_relation() = default;

    value_type& a() { return a_; }
    value_type& b() { return b_; }
    value_type& c() { return c_; }
    const value_type& a() const { return a_; }
    const value_type& b() const { return b_; }
    const value_type& c() const { return c_; }

    virtual value_type eval() const = 0;

protected:
    value_type a_, b_, c_;
};

template <typename Element>
struct linear_relation : public binary_relation<Element> {
    using Base = binary_relation<Element>;
    using value_type = typename Base::value_type;
    using Base::Base, Base::a, Base::b, Base::c;

    value_type eval() const override {
        value_type tmp = Base::a_;
        tmp += Base::b_;
        tmp -= Base::c_;
        return tmp;
    }
};

template <typename Element>
struct quadratic_relation : public binary_relation<Element> {
    using Base = binary_relation<Element>;
    using value_type = typename Base::value_type;
    using Base::Base, Base::a, Base::b, Base::c;

    value_type eval() const override {
        value_type tmp = Base::a_;
        tmp *= Base::b_;
        tmp -= Base::c_;
        return tmp;
    }
};

template <typename T>
concept BinaryRelation = std::derived_from<T, binary_relation<T>>;

}  // namespace ligero::zkp
