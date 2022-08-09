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

    store_t store;
    module_instance module;
    allocate_module(store, module, m);

    std::cout << module.funcaddrs.size() << std::endl;

    basic_executor exe;
    exe.store_ = &store;
    exe.module_ = &module;

    auto dummy_frame = std::make_unique<frame>();
    dummy_frame->module = &module;
    exe.current_frame_ = dummy_frame.get();

    exe.stack_push(u32(1));
    exe.stack_push(u32(2));
    op::call start{0};
    start.run(exe);

    std::cout << "Result: ";
    exe.show_stack();

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
