#pragma once

#include <string>
#include <stdexcept>

namespace ligero::vm {

// Name
/* ------------------------------------------------------------ */
using name_t = std::string;

// Numeric Types
/* ------------------------------------------------------------ */
using u8  = uint8_t;
using s8  = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;
using s64 = int64_t;

using index_t = u32;
using address_t = u64;

struct wasm_trap : std::runtime_error {
    using std::runtime_error::runtime_error;
};

}  // namespace ligero::vm
