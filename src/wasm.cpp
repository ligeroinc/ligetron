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

void show_hash(const typename zkp::sha256::digest& d) {
    std::cout << std::hex;
    for (auto i = 0; i < zkp::sha256::digest_size; i++)
        std::cout << (int)d.data[i];
    std::cout << std::endl << std::dec;
}

constexpr uint64_t modulus = 4611686018326724609ULL;
constexpr size_t l = 128, d = 256, n = 512;
using poly_t = zkp::primitive_poly<modulus>;

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

    zkp::reed_solomon64 encoder(modulus, l, d, n);

    zkp::stage1_prover_context<poly_t, zkp::sha256> ctx(encoder);
    {
        store_t store;
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
    }

    auto hash = ctx.root_hash();
    std::cout << "----------------------------------------" << std::endl << std::endl;
    show_hash(hash);
    std::cout << "----------------------------------------" << std::endl;

    zkp::stage2_prover_context<poly_t> ctx2(encoder, hash);
    {
        store_t store;
        ctx2.store(&store);
        zkp::zkp_executor exe(ctx2);
        auto module = instantiate(store, m, exe);
        ctx2.module(&module);

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
            ctx2.set_args(data);
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
    }

    std::cout << "----------------------------------------" << std::endl
              << "validation of linear: " << ctx2.validate_linear() << std::endl
              << "validation of quadratic: " << ctx2.validate_quadratic() << std::endl
              << "----------------------------------------" << std::endl;

    constexpr size_t sample_size = 80;
    zkp::hash_random_engine<zkp::sha256> engine(zkp::hash<zkp::sha256>(ctx2.get_argument()));
    std::vector<size_t> indexes(n), sample_index;
    std::iota(indexes.begin(), indexes.end(), 0);
    std::sample(indexes.cbegin(), indexes.cend(),
                std::back_inserter(sample_index),
                sample_size,
                engine);
    std::cout << "Sampled indexes: ";
    for (auto i = 0; i < sample_size; i++) {
        std::cout << sample_index[i] << " ";
    }
    std::cout << std::endl;
    
    zkp::stage3_prover_context<poly_t> ctx3(encoder, sample_index);
    {
        store_t store;
        ctx3.store(&store);
        zkp::zkp_executor exe(ctx3);
        auto module = instantiate(store, m, exe);
        ctx3.module(&module);

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
            ctx3.set_args(data);
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
    }

    std::cout << "Saved samples: " << ctx3.get_sample().size() << std::endl;

    zkp::verifier_context<poly_t> vctx(encoder, hash, ctx3.get_sample());
    {
        store_t store;
        vctx.store(&store);
        zkp::zkp_executor exe(vctx);
        auto module = instantiate(store, m, exe);
        vctx.module(&module);

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
            vctx.set_args(data);
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
    }

    const auto& prover_arg = ctx2.get_argument();
    const auto& verifier_arg = vctx.get_argument();

    bool verify_result = true;
    for (size_t i = 0; i < sample_size; i++) {
        verify_result = verify_result &&
            (prover_arg.code()[sample_index[i]] == verifier_arg.code()[i]) &&
            (prover_arg.linear()[sample_index[i]] == verifier_arg.linear()[i]) &&
            (prover_arg.quadratic()[sample_index[i]] == verifier_arg.quadratic()[i]);
    }
    
    std::cout << "Check result: " << verify_result << std::endl;
    
    return 0;
}
