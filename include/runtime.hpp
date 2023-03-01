#pragma once

#include <vector>
#include <variant>
#include <optional>
#include <type.hpp>
#include <base.hpp>
#include <instruction.hpp>
#include <bridge.hpp>
#include <prelude.hpp>
#include <value.hpp>

#include <string_view>

namespace ligero::vm {

// using value_t = std::variant<u32, u64>;
// using svalue_t = std::variant<u32, u64, label, frame_ptr>;


// value_t to_value(const svalue_t& sv) {
//     return std::visit(prelude::overloaded {
//             [](u32 v) -> value_t { return v; },
//             [](u64 v) -> value_t { return v; },
//             [](const auto& v) -> value_t { throw wasm_trap("Invalid conversion"); }
//         }, sv);
// }

// svalue_t to_svalue(const value_t& v) {
//     return std::visit(prelude::overloaded {
//             [](u32 v) -> svalue_t { return v; },
//             [](u64 v) -> svalue_t { return v; },
//         }, v);
// }

// struct stack_val_t {
//     enum class kind {
//         i32,
//         i64,
//         label,
//         frame
//     };

//     stack_val_t(u32 i) : val{.i32=i}, tag(kind::i32) { }
//     stack_val_t(u64 i) : val{.i64=i}, tag(kind::i64) { }
//     stack_val_t(label_t label) : val{.label=label}, tag(kind::label) { }
//     stack_val_t(frame_t *f) : val{.frame=f}, tag(kind::frame) { }

//     u32 i32() const { return val.i32; }
//     u64 i64() const { return val.i64; }
//     label_t label() const { return val.label; }
//     frame_t *frame() const { return val.frame; }

//     std::string_view type_str() const {
//         switch(tag) {
//         case kind::i32:
//             return "i32";
//         case kind::i64:
//             return "i64";
//         case kind::label:
//             return "label";
//         case kind::frame:
//             return "frame";
//         default:
//             return "<unknown>";
//         }
//     }

//     union {
//         u32 i32;
//         u64 i64;
//         label_t label;
//         frame_t *frame;
//     } val;
//     kind tag;
// };



// Forward Declaration
/* ------------------------------------------------------------ */
struct function_instance;
// struct table_instance;
struct memory_instance;
struct global_instance;
// struct element_instance;
struct data_instance;
// struct export_instance;


// Module Instance
/* ------------------------------------------------------------ */
struct module_instance {
    module_instance() = default;
    
    std::vector<function_kind> types;
    std::vector<address_t> funcaddrs;
    // std::vector<address_t> tableaddrs;
    std::vector<address_t> memaddrs;
    std::vector<address_t> globaladdrs;
    // std::vector<address_t> elemaddrs;
    std::vector<address_t> dataaddrs;
    // std::vector<export_instance> exports;
};

// Function Instance
/* ------------------------------------------------------------ */
using import_name_t = std::pair<std::string, std::string>;

struct function_instance {
    struct func_code {
        index_t type_index;
        std::vector<value_kind> locals;
        instr_vec body;
    };

    struct host_code {
        index_t type_index;
        import_name_t host_name;
    };

    function_instance(function_kind k) : kind(k) { }
    function_instance(function_kind k, const module_instance *module) : kind(k), module(module) { }

    template <typename T>
    function_instance(name_t name, function_kind k, const module_instance *module, T&& code)
        : name(name), kind(k), module(module), code(std::move(code)) { }

    name_t name;
    function_kind kind;
    const module_instance *module = nullptr;
    std::variant<func_code, host_code> code;
};

// Table Instance
/* ------------------------------------------------------------ */
// struct table_instance {
//     table_kind kind;
//     std::vector<ref_t> elem;
// };

// Memory Instance
/* ------------------------------------------------------------ */
struct memory_instance {
    constexpr static size_t page_size = 65536;  /* 64KB */
    memory_instance(memory_kind k, size_t mem_size) : kind(k), data(mem_size, 0) { }
    
    memory_kind kind;
    std::vector<u8> data;
};

// Global Instance
/* ------------------------------------------------------------ */
struct global_instance {
    global_instance(global_kind k, u32 val) : kind(k), val(val) { }
    
    global_kind kind;
    u32 val;
};

// Element Instance
/* ------------------------------------------------------------ */
// struct element_instance {
//     ref_kind type;
//     std::vector<ref_t> elem;
// };

// Data Instance
/* ------------------------------------------------------------ */
struct data_instance {
    data_instance(const std::vector<u8>& d, instr_vec&& expr, index_t mem)
        : offset_expr(std::move(expr)), memory(mem), data(d) { }

    bool is_active = true;
    instr_vec offset_expr;
    index_t memory = 0;
    std::vector<u8> data;
};

// Export Instance
/* ------------------------------------------------------------ */
// struct export_instance {
//     name_t name;
//     externval_t val;
// };

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
        // else if constexpr (std::is_same_v<T, table_instance>) {
        //     size_t index = tables.size();
        //     tables.emplace_back(std::forward<Args>(args)...);
        //     return index;
        // }
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
        // else if constexpr (std::is_same_v<T, element_instance>) {
        //     size_t index = elements.size();
        //     elements.emplace_back(std::forward<Args>(args)...);
        //     return index;
        // }
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
    // std::vector<table_instance> tables;
    std::vector<memory_instance> memorys;
    std::vector<global_instance> globals;
    // std::vector<element_instance> elements;
    std::vector<data_instance> datas;
};


/* ------------------------------------------------------------ */
index_t allocate_function(store_t& store,
                          const module_instance *inst,
                          const wabt::Func& func,
                          const std::unordered_map<std::string, import_name_t>& import_map) {
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

    std::variant<function_instance::func_code, function_instance::host_code> code;
    /* A function without body is an external function */
    if (body.empty()) {
        // code = function_instance::host_code {
        //     func.decl.type_var.index(),
        //     import_map.at(func.name)
        // };
    }
    else {
        code = function_instance::func_code {
            func.decl.type_var.index(),
            std::move(local_type),
            std::move(body)
        };
    }

    return store.emplace_back<function_instance>(func.name,
                                                 function_kind{ std::move(param), std::move(result) },
                                                 inst,
                                                 std::move(code));
}

// index_t allocate_table(store_t& store, const wabt::Table& table) {
//     undefined("Undefined allocate_table");
//     return 0;
// }

index_t allocate_memory(store_t& store, const wabt::Memory& memory) {
    limits limit{ memory.page_limits.initial };
    if (memory.page_limits.has_max)
        limit.max.emplace(memory.page_limits.max);

    size_t memory_size = std::max<u64>(limit.min, 256) + 128; // 16MB heap + 8MB stack
    
    return store.emplace_back<memory_instance>(memory_kind{ limit },
                                               memory_instance::page_size * memory_size);
}

// index_t allocate_global(store_t& store, const wabt::Global& g) {
//     // value_t val;
//     // if (auto *p = dynamic_cast<const wabt::ConstExpr*>(&(*(g.init_expr.begin())))) {
//     //     switch(p->const_.type()) {
//     //     case wabt::Type::I32:
//     //         val = p->const_.u32();
//     //         break;
//     //     case wabt::Type::I64:
//     //         val = p->const_.u64();
//     //         break;
//     //     default:
//     //         undefined("Unsopported initialization in global");
//     //     }        
//     // }
//     // else {
//     //     undefined("Unsopported initialization in global");
//     // }
//     instr_vec init;
//     std::transform(g.init_expr.begin(), g.init_expr.end(),
//                    std::back_inserter(init),
//                    [](const auto& expr) {
//                        return translate(expr);
//                    });
//     global_kind kind{ translate_type(g.type), g.mutable_ };
//     return store.emplace_back<global_instance>(std::move(kind), std::move(init));
// }

index_t allocate_data(store_t& store, const wabt::DataSegment& seg) {
    instr_vec exprs;
    for (const auto& expr : seg.offset) {
        exprs.push_back(translate(expr));
    }
    return store.emplace_back<data_instance>(seg.data, std::move(exprs), seg.memory_var.index());
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
    {
        std::unordered_map<std::string, import_name_t> map;
        for (const auto *imp : module.imports) {
            if (auto *p = dynamic_cast<const wabt::FuncImport*>(imp)) {
                map.insert({ p->func.name, { imp->module_name, imp->field_name }});
            }
        }
        
        for (const auto *p : module.funcs) {
            mins.funcaddrs.push_back(allocate_function(store, &mins, *p, map));
        }
    }
    
    // Memorys
    for (const auto *p : module.memories) {
        mins.memaddrs.push_back(allocate_memory(store, *p));
    }

    for (const auto *p : module.data_segments) {
        mins.dataaddrs.push_back(allocate_data(store, *p));
    }

    // TODO: Exports
    
    return;
}

template <typename Executor>
module_instance instantiate(store_t& store, const wabt::Module& module, Executor& exe) {
    module_instance minst;
    allocate_module(store, minst, module);

    /* Initialize globals */
    {
        auto frame_init = std::make_unique<typename Executor::frame_type>();

        // TODO: make a frame only contain imported globals and functions
        frame_init->module = &minst;
        exe.context().push_frame(std::move(frame_init));

        for (auto& imp : module.imports) {
            if (auto *p = dynamic_cast<wabt::GlobalImport*>(imp)) {
                // TODO: fix hard-coding
                if (p->module_name == "env" && p->field_name == "__stack_pointer") {
                    u32 stack_pointer = store.memorys[0].data.size() - 1;
                    std::cout << "stack pointer: " << stack_pointer << std::endl;
                    
                    auto& g = p->global;
                    auto expr = std::make_unique<wabt::ConstExpr>(wabt::Const::I32(stack_pointer));
                    if (g.init_expr.empty()) {
                        g.init_expr.push_back(std::move(expr));
                    }
                }
            }
        }

        for (const auto *p : module.globals) {
            // std::cout << "Initializing global " << p->name << std::endl;

            for (const auto& expr : p->init_expr) {
                translate(expr)->run(exe);
            }

            auto top = exe.context().stack_pop().template as<typename Executor::u32_type>();

            // TODO: make it more general
            u32 global_val = 0;
            if constexpr (requires(typename Executor::u32_type u, size_t i) { u[i]; }) {
                global_val = top[0];
            }
            else {
                global_val = top;
            }
            
            // auto v = std::visit(prelude::overloaded {
            //         [](u32 v) -> value_t { return v; },
            //         [](u64 v) -> value_t { return v; },
            //         [](const auto& v) -> value_t { throw wasm_trap("Invalid global init value"); }
            //     }, );

            global_kind k{ translate_type(p->type), p->mutable_ };
            index_t gi = store.emplace_back<global_instance>(k, global_val);
            minst.globaladdrs.push_back(gi);
           
        }
    }

    /* Instantiate Datas */
    {
        for (address_t addr : minst.dataaddrs) {
            data_instance& dinst = store.datas[addr];
            u32 n = dinst.data.size();
            for (auto& expr : dinst.offset_expr) {
                expr->run(exe);
            }
            exe.context().stack_push(typename Executor::value_type{ 0U });
            exe.context().stack_push(n);
            exe.run(op::memory_init{ static_cast<u32>(addr) });
            exe.run(op::data_drop{ static_cast<u32>(addr) });
        }
    }

    return minst;
}

template <typename Executor, typename... Args>
void invoke(module_instance& m, Executor& exe, index_t funcaddr, Args&&... args) {
    auto dummy = std::make_unique<typename Executor::frame_type>();
    dummy->module = &m;
    exe.context().push_frame(std::move(dummy));

    ((exe.context().push_witness(args)), ...);

    // std::cout << "Parameters: ";
    // exe.context().show_stack();
    op::call{funcaddr}.run(exe);
    std::cout << "Result: ";
    exe.context().show_stack();
    std::cout << std::endl;
}

}  // namespace ligero::vm
