#include <bridge.hpp>
#include <src/binary-reader-ir.h>
#include <src/binary-reader.h>
#include <sstream>
#include <fstream>
#include <runtime.hpp>
#include <execution.hpp>

using namespace wabt;
using namespace ligero::vm;

int main(int argc, char *argv[]) {
    const char *file = argv[1];
    std::ifstream fs(file, std::ios::binary);
    std::stringstream ss;
    ss << fs.rdbuf();
    const std::string content = ss.str();
    std::cout << "read file " << file << " with size " << content.size() << std::endl;
    
    Module m;
    Result r = ReadBinaryIr(file, content.data(), content.size(), ReadBinaryOptions{}, nullptr, &m);

    assert(r != Result::Error);

    std::cout << "Imports: " << m.imports.size() << std::endl;
    for (const auto *imp : m.imports) {
        if (auto *p = dynamic_cast<const FuncImport*>(imp)) {
            std::cout << imp->module_name << "." << imp->field_name << " => " << p->func.name << std::endl;
        }
    }

    store_t store;
    module_instance module;
    allocate_module(store, module, m);

    std::cout << "Functions: " << module.funcaddrs.size() << std::endl;
    for (const auto& f : store.functions) {
        std::cout << "Func: " << f.name << std::endl;
    }

    basic_executor exe;
    exe.store(&store);
    exe.module(&module);

    {
        std::vector<u8> arg(32);
        u32 *p = reinterpret_cast<u32*>(arg.data());
        for (auto i = 0; i < 8; i++) {
            p[i] = 8-i;
        }
        std::vector<u8> data(arg);
        exe.args(data);
    }
    

    auto dummy_frame = std::make_unique<frame>();
    dummy_frame->module = &module;
    exe.push_frame(std::move(dummy_frame));

    auto *v = reinterpret_cast<u32*>(store.memorys[0].data.data());
    std::cout << "Mem: ";
    for (auto i = 0; i < 10; i++) {
        std::cout << *(v + i) << " ";
    }
    std::cout << std::endl;

    exe.stack_push(u32(0));
    op::call start{ std::stoul(argv[2]) };
    start.run(exe);

    std::cout << "Result: ";
    exe.show_stack();

    std::cout << "Mem: ";
    for (auto i = 0; i < 10; i++) {
        std::cout << *(v + i) << " ";
    }
    std::cout << std::endl;

    // if (r != Result::Error) {
    //     std::cout << "success" << std::endl;
    //     // for (const auto& exp : m.exports) {
    //     //     std::cout << exp->name << ", " << exp->var.index() << std::endl;
    //     // }
    //     for (Func* func: m.funcs) {
    //         std::cout << "In Function: " << func->name << std::endl;
    //         for (const auto& expr : func->exprs) {
    //             std::cout << GetExprTypeName(expr.type()) << std::endl;
    //             if (const auto *ref = dynamic_cast<const BlockExpr*>(&expr); ref != nullptr) {
    //                 for (const auto& exp : ref->block.exprs) {
    //                     std::cout << GetExprTypeName(exp.type()) << std::endl;
    //                 }
    //             }
    //             instr_ptr p = translate(expr);
    //         }
    //     }
    // }
    return 0;
}
