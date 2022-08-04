#pragma once

#include <fstream>
#include <exception>
#include <string>
#include <type.hpp>
#include <instruction.hpp>

namespace ligero::vm {

using namespace std::string_literals;

using byte_t = uint8_t;

struct binary_stream {
    binary_stream(const std::string& file) : ifs_(file, std::ios::binary) {
        if (!ifs_.is_open()) {
            throw std::runtime_error("Cannot open file"s + file);
        }
    }

    uint8_t next_byte() {
        uint8_t b;
        ifs_ >> b;
        return b;
    }

    template <typename T>
    inline std::pair<T, unsigned int> decode_helper(T& ui) {
        constexpr T low_mask = static_cast<T>(0x7F);
        constexpr T high_mask = static_cast<T>(0x80);
        unsigned int shift = 0;
        T byte;
        do {
            byte = next_byte();
            ui |= (byte & low_mask) << shift;
            shift += 7;
        } while (byte & high_mask);
        return shift;
    }

    template <typename U>
    U leb128_unsigned() {
        U ui = 0;
        decode_helper<U>(ui);
        return ui;
    }

    template <typename S, size_t Size>
    S leb128_signed() {
        S si = 0;
        constexpr S sign_mask = static_cast<S>(0x40);
        auto [byte, shift] = decode_helper<S>(si);
        if ((shift < Size) and (byte & sign_mask)) {
            si |= (~static_cast<S>(0) << shift);
        }
        return si;
    }

    template <typename S, size_t Size>
    S leb128_signed(byte_t b) {
        
    }
    
protected:
    std::ifstream ifs_;
    size_t offset_;
};

template <typename Stream>
struct basic_parser {

    basic_parser(Stream& stream) : stream_(stream) { }

    byte_t byte() { return stream_.next_byte(); }

    u32 parse_u32() {
        return stream_.template leb128_unsigned<u32>();
    }

    int_kind parse_int_kind() {
        byte_t b = byte();
        switch(b) {
        case static_cast<byte_t>(int_kind::i32):
            return int_kind::i32;
        case static_cast<byte_t>(int_kind::i64):
            return int_kind::i64;
        default:
            throw std::runtime_error("Unexpected int type");
        }
    }

    ref_kind parse_ref_kind() {
        byte_t b = byte();
        switch(b) {
        case static_cast<byte_t>(ref_kind::funcref):
            return ref_kind::funcref;
        case static_cast<byte_t>(ref_kind::externref):
            return ref_kind::externref;
        default:
            throw std::runtime_error("Unexpected ref type");
        }
    }

    value_kind parse_value_kind() { return parse_value_kind(byte()); }
    value_kind parse_value_kind(byte_t b) {
        switch(b) {
        case static_cast<byte_t>(value_kind::i32):
            return value_kind::i32;
        case static_cast<byte_t>(value_kind::i64):
            return value_kind::i64;
        case static_cast<byte_t>(value_kind::funcref):
            return value_kind::funcref;
        case static_cast<byte_t>(value_kind::externref):
            return value_kind::externref;
        default:
            throw std::runtime_error("Unexpected value type");
        }    
    }

    name_t parse_name() {
        u32 size = parse_u32();
        name_t name;
        for (u32 i = 0; i < size; i++) {
            name += stream_.next_byte();
        }
        return name;
    }

    result_kind parse_result_kind() {
        u32 size = parse_u32();
        result_kind result;
        for (u32 i = 0; i < size; i++) {
            result.push_back(parse_value_kind());
        }
        return result;
    }

    function_kind parse_function_kind() {
        auto param = parse_result_kind();
        auto ret = parse_result_kind();
        return { std::move(param), std::move(ret) };
    }

    limits parse_limits() {
        byte_t b = stream_.next_byte();
        u32 min = parse_u32();
        if (b == 0x01) {
            u32 max = parse_u32();
            return { min, max };
        }
        else {
            return { min, std::nullopt };
        }
    }

    memory_kind parse_memory_kind() {
        return { parse_limits() };
    }

    table_kind parse_table_kind() {
        return { parse_ref_kind(), parse_limits() };
    }

    mut_kind parse_mut() {
        byte_t b = byte();
        switch(b) {
        case static_cast<byte_t>(mut_kind::konst):
            return mut_kind::konst;
        case static_cast<byte_t>(mut_kind::var):
            return mut_kind::var;
        default:
            throw std::runtime_error("Unexpected mut kind");
        }
    }

    global_kind parse_global_kind() {
        return { parse_value_kind(), parse_mut_kind() };
    }

    block_type parse_block_type() {
        byte_t b = byte();
        if (b == 0x40) {
            return empty_blcok;
        }
        else {
            try {
                return parse_value_kind(b);
            }
            catch (const std::runtime_error& e) {
                return stream_.
            }
        }
    }

    instr_ptr parse_instruction() {
        
    }

protected:
    Stream& stream_;
};


}  // namespace ligero::vm
