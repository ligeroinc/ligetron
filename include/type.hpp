#pragma once

#include <variant>
#include <vector>
#include <optional>

#include <base.hpp>

namespace ligero::vm {

// Numeric Types
/* ------------------------------------------------------------ */
enum class int_kind : uint8_t {
    i32 = 0x7F,
    i64 = 0x7E
};

enum class float_kind : uint8_t {
    f32 = 0x7D,
    f64 = 0x7C
};

enum class sign_kind : bool {
    sign,
    unsign
};

std::string to_string(int_kind k) {
    switch(k) {
    case int_kind::i32:
        return "i32";
    case int_kind::i64:
        return "i64";
    default:
        return "<error>";
    }
}

std::string to_string(sign_kind k) {
    switch(k) {
    case sign_kind::sign:
        return "s";
    case sign_kind::unsign:
        return "u";
    default:
        return "<error>";
    }
}

// Vector Type
/* ------------------------------------------------------------ */
enum class vec_kind : uint8_t {
    v128 = 0x7B
};

// Reference Types
/* ------------------------------------------------------------ */
enum class ref_kind : uint8_t {
    funcref = 0x70,
    externref = 0x6F
};


// Value Type
/* ------------------------------------------------------------ */
enum class value_kind : uint8_t {
    i32 = static_cast<uint8_t>(int_kind::i32),
    i64 = static_cast<uint8_t>(int_kind::i64),
    funcref = static_cast<uint8_t>(ref_kind::funcref),
    externref = static_cast<uint8_t>(ref_kind::externref),
};

using result_kind = std::vector<value_kind>;

// Function Type
/* ------------------------------------------------------------ */
struct function_kind {
    result_kind params;
    result_kind returns;
};


// Limits
/* ------------------------------------------------------------ */
struct limits {
    limits() : min(0), max(std::nullopt) { }
    limits(u64 init) : min(init), max(std::nullopt) { }
    limits(u64 init, u64 max) : min(init), max(max) { }
    
    u64 min;
    std::optional<u64> max;
};


// Memory Type
/* ------------------------------------------------------------ */
struct memory_kind {
    memory_kind(limits l) : limit(l) { }
    
    limits limit;
};


// Table Type
/* ------------------------------------------------------------ */
struct table_kind {
    ref_kind kind;
    limits limit;
};


// Global Type
/* ------------------------------------------------------------ */
struct global_kind {
    value_kind kind;
    bool mut = false;
};

// Extern Types
/* ------------------------------------------------------------ */
enum class extern_kind : uint8_t {
    func,
    table,
    mem,
    global
};

// template <typename Kind>
// std::vector<Kind> extract_kind(const std::vector<extern_kind>& ext) {
//     std::vector<Kind> fs;
//     for (const auto& e : ext) {
//         if (const Kind *p = std::get_if<Kind>(&e)) {
//             fs.push_back(*p);
//         }
//     }
//     return fs;
// }

}  // namespace ligero::vm
