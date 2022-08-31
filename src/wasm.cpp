#include <bridge.hpp>
#include <src/binary-reader-ir.h>
#include <src/binary-reader.h>
#include <sstream>
#include <fstream>
#include <runtime.hpp>
#include <execution.hpp>
// #include <zkp/circuit.hpp>
#include <context.hpp>
#include <zkp/prover_context.hpp>
#include <zkp/prover_execution.hpp>
#include <zkp/poly_field.hpp>

using namespace wabt;
using namespace ligero::vm;

constexpr uint64_t modulus = 23;
using poly_t = zkp::primitive_poly<23>;

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

    zkp::reed_solomon64 lrs(modulus, 1024, 2048, 4096);
    zkp::reed_solomon64 qrs(modulus, 1024, 4096, 4096);

    store_t store;
    zkp::stage1_prover_context<zkp::sha256, poly_t> ctx(lrs, qrs);
    ctx.store(&store);
    zkp::zkp_executor exe(ctx);
    auto module = instantiate(store, m, exe);
    ctx.module(&module);

    std::cout << "Functions: " << module.funcaddrs.size() << std::endl;
    for (const auto& f : store.functions) {
        std::cout << "Func: " << f.name << std::endl;
    }

    {
        std::vector<u8> arg(32);
        u32 *p = reinterpret_cast<u32*>(arg.data());
        for (auto i = 0; i < 8; i++) {
            p[i] = 8-i;
        }
        std::vector<u8> data(arg);
        ctx.set_args(data);
    }
    

    auto dummy_frame = std::make_unique<frame>();
    dummy_frame->module = &module;
    exe.context().push_frame(std::move(dummy_frame));

    auto *v = reinterpret_cast<u32*>(store.memorys[0].data.data());
    std::cout << "Mem: ";
    for (auto i = 0; i < 10; i++) {
        std::cout << *(v + i) << " ";
    }
    std::cout << std::endl;

    exe.context().stack_push(u32(0));
    op::call start{ std::stoul(argv[2]) };
    start.run(exe);

    std::cout << "Result: ";
    exe.context().show_stack();

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
