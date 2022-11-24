#pragma once

#include <vector>
#include <variant>
#include <memory>
#include <string>

#include <prelude.hpp>
#include <base.hpp>

namespace ligero::vm {

using namespace std::string_literals;

struct module_instance;

struct label { u32 arity; };

template <typename LocalValue>
struct frame {
    using local_type = LocalValue;
    
    frame() = default;
    frame(u32 arity, std::vector<local_type>&& local, module_instance *module)
        : arity(arity), locals(std::move(local)), module(module) { }
    
    u32 arity;
    std::vector<local_type> locals;
    module_instance *module;
};

template <typename LocalValue, typename Reference>
struct zkp_frame : public frame<LocalValue> {
    using frame<LocalValue>::frame;

    std::vector<Reference> refs;
};

template <typename U32 = u32,
          typename S32 = s32,
          typename U64 = u64,
          typename S64 = s64>
struct numeric_value {
    using u32_type = U32;
    using s32_type = S32;
    using u64_type = U64;
    using s64_type = S64;

    using variant_type = std::variant<u32_type, u64_type>;

    numeric_value(const u32_type& v) : data_(v) { }
    numeric_value(const u64_type& v) : data_(v) { }

    // operator u32_type&() {
    //     assert(std::holds_alternative<u32_type>(data_));
    //     return std::get<u32_type>(data_);
    // }

    // operator u64_type&() {
    //     assert(std::holds_alternative<u64_type>(data_));
    //     return std::get<u64_type>(data_);
    // }

    template <typename T>
    const T& as() const { return std::get<T>(data_); }

    template <typename T>
    T& as() & { return std::get<T>(data_); }

    template <typename T>
    T as() const && { return std::move(std::get<T>(data_)); }

    const auto& data() const { return data_; }
    auto& data() { return data_; }

    template <typename T>
    void emplace(T&& v) { data_.emplace(std::forward<T>(v)); }

    std::string to_string() const {
        return std::visit(prelude::overloaded {
                [](const u32_type& v) { return "("s + prelude::to_string(v) + ": u32)"; },
                [](const u64_type& v) { return "("s + prelude::to_string(v) + ": u64)"; }
            }, data_);
    }

protected:
    variant_type data_;
};


template <typename Number, typename Label, typename Frame>
struct stack_value {
    using number_type = Number;
    using u32_type = typename Number::u32_type;
    using s32_type = typename Number::s32_type;
    using u64_type = typename Number::u64_type;
    using s64_type = typename Number::s64_type;
    
    using label_type = Label;
    using frame_type = Frame;
    using frame_pointer = frame_type*;
    using frame_ptr = std::unique_ptr<frame_type>;

    using variant_type = std::variant<number_type, label_type, frame_ptr>;

    template <typename T>
    stack_value(T&& v) : data_(std::forward<T>(v)) { }

    template <typename T>
    void emplace(T&& v) { data_.emplace(std::forward<T>(v)); }

    // operator number_type&() {
    //     assert(std::holds_alternative<number_type>(data_));
    //     return std::get<number_type>(data_);
    // }

    // operator u32_type&() {
    //     assert(std::holds_alternative<number_type>(data_));
    //     return static_cast<u32_type&>(static_cast<number_type&>(*this));
    // }

    // operator u64_type&() {
    //     assert(std::holds_alternative<number_type>(data_));
    //     return static_cast<u64_type&>(static_cast<number_type&>(*this));
    // }

    // operator label_type&() {
    //     assert(std::holds_alternative<label_type>(data_));
    //     return std::get<label_type>(data_);
    // }

    // operator frame_pointer() {
    //     assert(std::holds_alternative<frame_ptr>(data_));
    //     return std::get<frame_ptr>(data_).get();
    // }

    template <typename T>
    const T& as() const {
        if constexpr (std::is_same_v<T, u32_type> || std::is_same_v<T, u64_type>) {
            return std::get<number_type>(data_).template as<T>();
        }
        else {
            return std::get<T>(data_);
        }
    }

    template <typename T>
    T& as() & {
        if constexpr (std::is_same_v<T, u32_type> || std::is_same_v<T, u64_type>) {
            return std::get<number_type>(data_).template as<T>();
        }
        else {
            return std::get<T>(data_);
        }
    }

    template <typename T>
    T as() && {
        if constexpr (std::is_same_v<T, u32_type> || std::is_same_v<T, u64_type>) {
            return std::move(std::get<number_type>(data_).template as<T>());
        }
        else {
            return std::move(std::get<T>(data_));
        }
    }

    const auto& data() const { return data_; }
    auto& data() { return data_; }

    std::string to_string() const {
        return std::visit(prelude::overloaded {
                [](const number_type& v) { return v.to_string(); },
                [](const label_type& l) { return "L"s + std::to_string(l.arity); },
                [](const frame_ptr& f) { return "Frame"s; }
            }, data_);
    }
    
protected:
    variant_type data_;
};


using value_t = numeric_value<>;
using frame_t = frame<value_t>;
using frame_ptr = std::unique_ptr<frame_t>;
using svalue_t = stack_value<value_t, label, frame_t>;

}  // namespace ligero::vm
