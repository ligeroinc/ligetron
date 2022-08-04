#pragma once

#include <vector>
#include <variant>
#include <optional>
#include <type.hpp>
#include <base.hpp>
#include <instruction.hpp>
#include <bridge.hpp>

#include <ir.h>

namespace ligero::vm {

// Reference
/* ------------------------------------------------------------ */
struct ref_t {
    ref_kind kind;
    address_t address;
};


// Runtime Objects
/* ------------------------------------------------------------ */
union value_t {
    u32 i32;
    u64 i64;
    // ref_t ref;
};

struct externval_t {
    extern_kind kind;
    address_t address;
};

struct label_t {
    u32 arity;
    instr_vec continuation;
};

struct module_instance;
struct frame_t {
    u32 arity;
    std::vector<value_t> locals;
    module_instance *module;
};

// Trap
/* ------------------------------------------------------------ */
struct Trap;


// Forward Declaration
/* ------------------------------------------------------------ */
struct function_instance;
struct table_instance;
struct memory_instance;
struct global_instance;
struct element_instance;
struct data_instance;
struct export_instance;

// Store
/* ------------------------------------------------------------ */
struct store_t {
    store_t() = default;

    template <typename T, typename... Args>
    index_t emplace_back(Args&&... args) {
        if constexpr (std::is_same_v<T, function_instance>) {
            size_t index = functions.size();
            functions.emplace_back(std::forward<Args>(args)...);
            return index;
        }
        else if constexpr (std::is_same_v<T, table_instance>) {
            size_t index = tables.size();
            tables.emplace_back(std::forward<Args>(args)...);
            return index;
        }
        else if constexpr (std::is_same_v<T, memory_instance>) {
            size_t index = memorys.size();
            memorys.emplace_back(std::forward<Args>(args)...);
            return index;
        }
        else if constexpr (std::is_same_v<T, global_instance>) {
            size_t index = globals.size();
            globals.emplace_back(std::forward<Args>(args)...);
            return index;
        }
        else if constexpr (std::is_same_v<T, element_instance>) {
            size_t index = elements.size();
            elements.emplace_back(std::forward<Args>(args)...);
            return index;
        }
        else if constexpr (std::is_same_v<T, data_instance>) {
            size_t index = datas.size();
            datas.emplace_back(std::forward<Args>(args)...);
            return index;
        }
        else {
            // C++20 Compile-time magic
            []<bool flag = false>{ static_assert(flag, "Unexpected instance type"); }();
        }
    }
    
    std::vector<function_instance> functions;
    std::vector<table_instance> tables;
    std::vector<memory_instance> memorys;
    std::vector<global_instance> globals;
    std::vector<element_instance> elements;
    std::vector<data_instance> datas;
};


// Module Instance
/* ------------------------------------------------------------ */
struct module_instance {
    module_instance() = default;
    
    std::vector<function_kind> types;
    std::vector<address_t> funcaddrs;
    std::vector<address_t> tableaddrs;
    std::vector<address_t> memaddrs;
    std::vector<address_t> globaladdrs;
    std::vector<address_t> elemaddrs;
    std::vector<address_t> dataaddrs;
    std::vector<export_instance> exports;
};

// Function Instance
/* ------------------------------------------------------------ */
struct function_instance {
    struct code_t {
        index_t type_index;
        std::vector<value_kind> locals;
        instr_vec body;
    };

    function_instance(function_kind k) : kind(k) { }
    function_instance(function_kind k, const module_instance *module) : kind(k), module(module) { }
    function_instance(name_t name, function_kind k, const module_instance *module, code_t&& code)
        : name(name), kind(k), module(module), code(std::move(code)) { }

    name_t name;
    function_kind kind;
    const module_instance *module = nullptr;
    code_t code;
};

// Table Instance
/* ------------------------------------------------------------ */
struct table_instance {
    table_kind kind;
    std::vector<ref_t> elem;
};

// Memory Instance
/* ------------------------------------------------------------ */
struct memory_instance {
    memory_instance(memory_kind k, size_t mem_size) : kind(k), data(mem_size, 0) { }
    
    memory_kind kind;
    std::vector<u8> data;
};

// Global Instance
/* ------------------------------------------------------------ */
struct global_instance {
    global_instance(global_kind k, value_t val) : kind(k), val(val) { }
    
    global_kind kind;
    value_t val;
};

// Element Instance
/* ------------------------------------------------------------ */
struct element_instance {
    ref_kind type;
    std::vector<ref_t> elem;
};

// Data Instance
/* ------------------------------------------------------------ */
struct data_instance {
    std::vector<u8> data;
};

// Export Instance
/* ------------------------------------------------------------ */
struct export_instance {
    name_t name;
    externval_t val;
};


/* ------------------------------------------------------------ */
index_t allocate_function(store_t& store, const module_instance *inst, const wabt::Func& func) {
    std::vector<value_kind> local_type;
    for (const auto& type : func.local_types) {
        local_type.push_back(translate_type(type));
    }

    instr_vec body;
    for (const auto& expr : func.exprs) {
        body.push_back(translate(expr));
    }

    std::vector<value_kind> param, result;
    for (const auto& type : func.decl.sig.param_types) {
        param.push_back(translate_type(type));
    }

    for (const auto& type : func.decl.sig.result_types) {
        result.push_back(translate_type(type));
    }

    function_instance::code_t code{
        func.decl.type_var.index(),
        std::move(local_type),
        std::move(body)
    };
    return store.emplace_back<function_instance>(func.name,
                                                 function_kind{ std::move(param), std::move(result) },
                                                 inst,
                                                 std::move(code));
}

index_t allocate_table(store_t& store, const wabt::Table& table) {
    undefined("Undefined allocate_table");
    return 0;
}

index_t allocate_memory(store_t& store, const wabt::Memory& memory) {
    constexpr size_t page_size = 64 * 1024 * 1024;  // 64 KB

    limits limit{ memory.page_limits.initial };
    if (memory.page_limits.has_max)
        limit.max.emplace(memory.page_limits.max);

    return store.emplace_back<memory_instance>(memory_kind{ limit }, page_size * limit.min);
}

index_t allocate_global(store_t& store, const wabt::Global& g) {
    value_t val;
    if (auto *p = dynamic_cast<const wabt::ConstExpr*>(&(*(g.init_expr.begin())))) {
        switch(p->const_.type()) {
        case wabt::Type::I32:
            val.i32 = p->const_.u32();
            break;
        case wabt::Type::I64:
            val.i64 = p->const_.u64();
            break;
        default:
            undefined("Unsopported initialization in global");
        }        
    }
    else {
        undefined("Unsopported initialization in global");
    }
    global_kind kind{ translate_type(g.type), g.mutable_ };
    return store.emplace_back<global_instance>(std::move(kind), std::move(val));
}

void allocate_module(store_t& store, module_instance& mins, const wabt::Module& module) {
    // Types
    for (const auto *tp : module.types) {
        std::vector<value_kind> param, result;
        if (auto *p = dynamic_cast<const wabt::FuncType*>(tp)) {
            const wabt::FuncSignature& sig = p->sig;
            for (const auto& t : sig.param_types)
                param.push_back(translate_type(t));
            for (const auto& t : sig.result_types)
                result.push_back(translate_type(t));
        }
        else {
            undefined("Expect Function type");
        }
        mins.types.emplace_back(std::move(param), std::move(result));
    }

    // Functions
    for (const auto *p : module.funcs) {
        mins.funcaddrs.push_back(allocate_function(store, &mins, *p));
    }

    
    // TODO: Tables
    

    // Memorys
    for (const auto *p : module.memories) {
        mins.memaddrs.push_back(allocate_memory(store, *p));
    }


    // TODO: Globals


    // TODO: Elements


    // TODO: Datas


    // TODO: Exports
    
    return;
}

}  // namespace ligero::vm
