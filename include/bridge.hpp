#pragma once

#include <type.hpp>
#include <instruction.hpp>
#include <ir.h>
#include <exception>

namespace ligero::vm {

struct undefined_error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

inline void undefined(const wabt::Expr& expr) {
    throw undefined_error(std::string("Undefined ") + wabt::GetExprTypeName(expr));
}

inline void undefined(const std::string& str = "") {
    throw undefined_error(std::string("Undefined ") + str);
}

template <typename T, typename... Args>
instr_ptr make_instr(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

value_kind translate_type(const wabt::Type& type) {
    const wabt::Type::Enum t = type;
    switch(t) {
    case wabt::Type::Enum::I32:
        return value_kind::i32;
    case wabt::Type::Enum::I64:
        return value_kind::i64;
    default:
        undefined(type.GetName() + " is unsupported");
        return value_kind::i32;
    }
}

instr_ptr translate(const wabt::Expr& expr);

instr_ptr translate_constexpr(const wabt::ConstExpr& expr) {
    const wabt::Const& k = expr.const_;
    switch(k.type()) {
    case wabt::Type::I32:
        return make_instr<op::inn_const>(k.u32(), int_kind::i32);
    case wabt::Type::I64:
        return make_instr<op::inn_const>(k.u64(), int_kind::i64);
    default:
        undefined(expr);
    }
    return nullptr;
}

instr_ptr translate_unaryexpr(const wabt::UnaryExpr& expr) {
    const wabt::Opcode::Enum op = expr.opcode;
    switch(op) {
    case wabt::Opcode::I32Clz:
        return make_instr<op::inn_clz>(int_kind::i32);
    case wabt::Opcode::I64Clz:
        return make_instr<op::inn_clz>(int_kind::i64);
    case wabt::Opcode::I32Ctz:
        return make_instr<op::inn_ctz>(int_kind::i32);
    case wabt::Opcode::I64Ctz:
        return make_instr<op::inn_ctz>(int_kind::i64);
    default:
        undefined(expr);
    }
    return nullptr;
}

instr_ptr translate_binaryexpr(const wabt::BinaryExpr& expr) {
    const wabt::Opcode::Enum op = expr.opcode;
    switch(op) {
    case wabt::Opcode::I32Add:
        return make_instr<op::inn_add>(int_kind::i32);
    case wabt::Opcode::I32Sub:
        return make_instr<op::inn_sub>(int_kind::i32);
    case wabt::Opcode::I32Mul:
        return make_instr<op::inn_mul>(int_kind::i32);
    case wabt::Opcode::I32DivS:
        return make_instr<op::inn_div_sx>(int_kind::i32, sign_kind::sign);
    case wabt::Opcode::I32DivU:
        return make_instr<op::inn_div_sx>(int_kind::i32, sign_kind::unsign);
    case wabt::Opcode::I32RemS:
        return make_instr<op::inn_rem_sx>(int_kind::i32, sign_kind::sign);
    case wabt::Opcode::I32RemU:
        return make_instr<op::inn_rem_sx>(int_kind::i32, sign_kind::unsign);
    case wabt::Opcode::I32And:
        return make_instr<op::inn_and>(int_kind::i32);
    case wabt::Opcode::I32Or:
        return make_instr<op::inn_or>(int_kind::i32);
    case wabt::Opcode::I32Xor:
        return make_instr<op::inn_xor>(int_kind::i32);
    case wabt::Opcode::I32Shl:
        return make_instr<op::inn_shl>(int_kind::i32);
    case wabt::Opcode::I32ShrS:
        return make_instr<op::inn_shr_sx>(int_kind::i32, sign_kind::sign);
    case wabt::Opcode::I32ShrU:
        return make_instr<op::inn_shr_sx>(int_kind::i32, sign_kind::unsign);
    case wabt::Opcode::I32Rotl:
        return make_instr<op::inn_rotl>(int_kind::i32);
    case wabt::Opcode::I32Rotr:
        return make_instr<op::inn_rotr>(int_kind::i32);

        // TODO: add i64

    default:
        undefined(expr);
    }
    return nullptr;
}

instr_ptr translate_ternaryexpr(const wabt::TernaryExpr& expr) {
    // const wabt::Opcode::Enum op = expr.opcode;
    // switch (op) {

    // default:
    //     undefined(expr);
    // }
    undefined(expr);
    return nullptr;
}

instr_ptr translate_compareexpr(const wabt::CompareExpr& expr) {
    const wabt::Opcode::Enum op = expr.opcode;
    switch(op) {
    case wabt::Opcode::I32Eq:
        return make_instr<op::inn_eqz>(int_kind::i32);
    case wabt::Opcode::I32Ne:
        return make_instr<op::inn_ne>(int_kind::i32);
    case wabt::Opcode::I32LtS:
        return make_instr<op::inn_lt_sx>(int_kind::i32, sign_kind::sign);
    case wabt::Opcode::I32LtU:
        return make_instr<op::inn_lt_sx>(int_kind::i32, sign_kind::unsign);
    case wabt::Opcode::I32GtS:
        return make_instr<op::inn_gt_sx>(int_kind::i32, sign_kind::sign);
    case wabt::Opcode::I32GtU:
        return make_instr<op::inn_gt_sx>(int_kind::i32, sign_kind::unsign);
    case wabt::Opcode::I32LeS:
        return make_instr<op::inn_le_sx>(int_kind::i32, sign_kind::sign);
    case wabt::Opcode::I32LeU:
        return make_instr<op::inn_le_sx>(int_kind::i32, sign_kind::unsign);
    case wabt::Opcode::I32GeS:
        return make_instr<op::inn_ge_sx>(int_kind::i32, sign_kind::sign);
    case wabt::Opcode::I32GeU:
        return make_instr<op::inn_ge_sx>(int_kind::i32, sign_kind::unsign);
    default:
        undefined(expr);
    }
    return nullptr;
}

instr_ptr translate_convertexpr(const wabt::ConvertExpr& expr) {
    const wabt::Opcode::Enum op = expr.opcode;
    switch (op) {
    case wabt::Opcode::I32Eqz:
        return make_instr<op::inn_eqz>(int_kind::i32);
    case wabt::Opcode::I64Eqz:
        return make_instr<op::inn_eqz>(int_kind::i64);
    case wabt::Opcode::I32Extend8S:
        return make_instr<op::inn_extend8_s>(int_kind::i32);
    case wabt::Opcode::I32Extend16S:
        return make_instr<op::inn_extend16_s>(int_kind::i32);
    case wabt::Opcode::I64ExtendI32S:
        return make_instr<op::i64_extend_i32_sx>(sign_kind::sign);
    case wabt::Opcode::I64ExtendI32U:
        return make_instr<op::i64_extend_i32_sx>(sign_kind::unsign);
    case wabt::Opcode::I32WrapI64:
        return make_instr<op::i32_wrap_i64>();
    default:
        undefined(expr.opcode.GetName());
    }
    return nullptr;
}

template <typename In, typename Out>
instr_ptr translate_scope(const In& expr) {
    Out block;
    block.label = expr.block.label;
    if (expr.block.decl.has_func_type) {
        block.type = expr.block.decl.type_var.index();
    }
    for (const auto& ins : expr.block.exprs) {
        block.body.push_back(translate(ins));
    }
    return make_instr<Out>(std::move(block));
}

instr_ptr translate_load(const wabt::LoadExpr& expr) {
    wabt::Opcode::Enum op = expr.opcode;
    switch (op) {
    case wabt::Opcode::I32Load:
        return make_instr<op::inn_load>(int_kind::i32, expr.align, expr.offset);
    case wabt::Opcode::I32Load8S:
        return make_instr<op::inn_load8_sx>(int_kind::i32, sign_kind::sign, expr.align, expr.offset);
    case wabt::Opcode::I32Load8U:
        return make_instr<op::inn_load8_sx>(int_kind::i32, sign_kind::unsign, expr.align, expr.offset);
    case wabt::Opcode::I32Load16S:
        return make_instr<op::inn_load16_sx>(int_kind::i32, sign_kind::sign, expr.align, expr.offset);
    case wabt::Opcode::I32Load16U:
        return make_instr<op::inn_load16_sx>(int_kind::i32, sign_kind::unsign, expr.align, expr.offset);
    default:
        undefined(expr);
    }
    return nullptr;
}

instr_ptr translate_store(const wabt::StoreExpr& expr) {
    wabt::Opcode::Enum op = expr.opcode;
    switch(op) {
    case wabt::Opcode::I32Store:
        return make_instr<op::inn_store>(int_kind::i32, expr.align, expr.offset);
    case wabt::Opcode::I32Store8:
        return make_instr<op::inn_store8>(int_kind::i32, expr.align, expr.offset);
    case wabt::Opcode::I32Store16:
        return make_instr<op::inn_store16>(int_kind::i32, expr.align, expr.offset);
    default:
        undefined(expr);
    }
    return nullptr;
}

instr_ptr translate(const wabt::Expr& expr) {
    if (auto *p = dynamic_cast<const wabt::ConstExpr*>(&expr)) {
        return translate_constexpr(*p);
    }

    if (auto *p = dynamic_cast<const wabt::UnaryExpr*>(&expr)) {
        return translate_unaryexpr(*p);
    }

    if (auto *p = dynamic_cast<const wabt::BinaryExpr*>(&expr)) {
        return translate_binaryexpr(*p);
    }

    if (auto *p = dynamic_cast<const wabt::CompareExpr*>(&expr)) {
        return translate_compareexpr(*p);
    }

    if (auto *p = dynamic_cast<const wabt::TernaryExpr*>(&expr)) {
        return translate_ternaryexpr(*p);
    }

    if (auto *p = dynamic_cast<const wabt::ConvertExpr*>(&expr)) {
        return translate_convertexpr(*p);
    }

    // TODO: Reference

    if (auto *p = dynamic_cast<const wabt::DropExpr*>(&expr)) {
        return make_instr<op::drop>();
    }

    if (auto *p = dynamic_cast<const wabt::SelectExpr*>(&expr)) {
        std::vector<value_kind> types;
        for (wabt::Type::Enum t : p->result_type) {
            switch(t) {
            case wabt::Type::Enum::I32:
                types.push_back(value_kind::i32);
                break;
            case wabt::Type::Enum::I64:
                types.push_back(value_kind::i64);
                break;
            default:
                undefined(expr);
            }
        }
        return make_instr<op::select>(std::move(types));
    }

    /* ------------------------------------------------------------ */

    if (auto *p = dynamic_cast<const wabt::LocalGetExpr*>(&expr)) {
        return make_instr<op::local_get>(p->var.index());
    }

    if (auto *p = dynamic_cast<const wabt::LocalSetExpr*>(&expr)) {
        return make_instr<op::local_set>(p->var.index());
    }

    if (auto *p = dynamic_cast<const wabt::LocalTeeExpr*>(&expr)) {
        return make_instr<op::local_tee>(p->var.index());
    }

    if (auto *p = dynamic_cast<const wabt::GlobalSetExpr*>(&expr)) {
        return make_instr<op::global_get>(p->var.index());
    }

    if (auto *p = dynamic_cast<const wabt::GlobalSetExpr*>(&expr)) {
        return make_instr<op::global_set>(p->var.index());
    }

    /* ------------------------------------------------------------ */

    // TODO: Table

    /* ------------------------------------------------------------ */
    if (auto *p = dynamic_cast<const wabt::LoadExpr*>(&expr)) {
        return translate_load(*p);
    }

    if (auto *p = dynamic_cast<const wabt::StoreExpr*>(&expr)) {
        return translate_store(*p);
    }

    /* ------------------------------------------------------------ */

    if (auto *p = dynamic_cast<const wabt::BlockExpr*>(&expr)) {
        return translate_scope<wabt::BlockExpr, op::block>(*p);
    }

    if (auto *p = dynamic_cast<const wabt::LoopExpr*>(&expr)) {
        return translate_scope<wabt::LoopExpr, op::loop>(*p);
    }

    if (auto *p = dynamic_cast<const wabt::BrExpr*>(&expr)) {
        return make_instr<op::br>(p->var.index());
    }

    if (auto *p = dynamic_cast<const wabt::BrIfExpr*>(&expr)) {
        return make_instr<op::br_if>(p->var.index());
    }

    if (auto *p = dynamic_cast<const wabt::ReturnExpr*>(&expr)) {
        return make_instr<op::ret>();
    }

    if (auto *p = dynamic_cast<const wabt::CallExpr*>(&expr)) {
        return make_instr<op::call>(p->var.index());
    }

    undefined(expr);
    return nullptr;
}

}
