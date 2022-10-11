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

#include <chrono>

using namespace wabt;
using namespace ligero::vm;

void show_hash(const typename zkp::sha256::digest& d) {
    std::cout << std::hex;
    for (size_t i = 0; i < zkp::sha256::digest_size; i++)
        std::cout << (int)d.data[i];
    std::cout << std::endl << std::dec;
}

template <typename Decoder, typename Poly>
bool validate(Decoder& dec, Poly p) {
    dec.decode(p);
    return std::all_of(p.begin(), p.end(), [](auto v) { return v == 0; });
}

// constexpr uint64_t modulus = 4611686018326724609ULL;
constexpr uint64_t modulus = 1073479681UL;
constexpr size_t l = 128, d = 256, n = 512;
using poly_t = zkp::primitive_poly<modulus>;

template <typename Context>
void run_program(Module& m, Context& ctx, size_t func, bool fill = true) {
    store_t store;
    ctx.store(&store);
    zkp::zkp_executor exe(ctx);
    auto module = instantiate(store, m, exe);
    ctx.module(&module);

    std::cout << "Functions: " << module.funcaddrs.size() << std::endl;
    for (const auto& f : store.functions) {
        std::cout << "Func: " << f.name << std::endl;
    }

    // {
    //     std::vector<u8> arg(32);
    //     u32 *p = reinterpret_cast<u32*>(arg.data());
    //     for (auto i = 0; i < 8; i++) {
    //         p[i] = 8-i;
    //     }
    //     std::vector<u8> data(arg);
    //     ctx.set_args(data);
    // }
    constexpr size_t offset = 16384;
    constexpr size_t len1 = 10, len2 = 10;
    constexpr size_t offset1 = offset + len1;
    if (fill) {
        {
            auto& mem = store.memorys[0].data;
            mem[offset] = 's';
            mem[offset+1] = 'u';
            mem[offset+2] = 'n';
            mem[offset+3] = 'd';
            mem[offset+4] = 'a';
            mem[offset+5] = 'y';

            mem[offset1] = 's';
            mem[offset1+1] = 'a';
            mem[offset1+2] = 't';
            mem[offset1+3] = 'u';
            mem[offset1+4] = 'r';
            mem[offset1+5] = 'd';
            mem[offset1+6] = 'a';
            mem[offset1+7] = 'y';
        }
    }

    // auto *v = reinterpret_cast<u32*>(store.memorys[0].data.data());
    // std::cout << "Mem: ";
    // for (auto i = 0; i < 10; i++) {
    //     std::cout << *(v + i) << " ";
    // }
    // std::cout << std::endl;

    invoke(module, exe, func, offset, offset1, len1, len2);

    // std::cout << "Mem: ";
    // for (auto i = 0; i < 10; i++) {
    //     std::cout << *(v + i) << " ";
    // }
    // std::cout << std::endl;
}

int main(int argc, char *argv[]) {
    // std::string dummy;
    
    const char *file = argv[1];
    size_t func = std::stoi(argv[2]);
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

    std::cout << "l: " << l << " d: " << d << " K: " << n << std::endl;

    std::random_device rd;
    const unsigned int encoder_seed = rd();
    zkp::reed_solomon64 encoder(modulus, l, d, n, encoder_seed);

    
    // Stage 1
    // -------------------------------------------------------------------------------- //

    
    zkp::stage1_prover_context<poly_t, zkp::sha256> ctx(encoder);
    auto stage1_begin = std::chrono::high_resolution_clock::now();
    {
        run_program(m, ctx, func);
    }
    auto stage1_end = std::chrono::high_resolution_clock::now();

    std::cout << "Stage1 time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(stage1_end - stage1_begin).count()
              << "ms" << std::endl;

    zkp::merkle_tree<zkp::sha256> tree = ctx.builder();
    auto hash = tree.root();
    // auto hash = ctx.root_hash();
    std::cout << "----------------------------------------" << std::endl << std::endl;
    show_hash(hash);
    std::cout << "----------------------------------------" << std::endl;

    // std::cin >> dummy;

    
    // Stage 2
    // -------------------------------------------------------------------------------- //

    
    encoder.seed(encoder_seed);
    zkp::stage2_prover_context<poly_t> ctx2(encoder, hash);
    auto stage2_begin = std::chrono::high_resolution_clock::now();
    {
        run_program(m, ctx2, func);
    }
    auto stage2_end = std::chrono::high_resolution_clock::now();

    std::cout << "Stage2 time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(stage2_end - stage2_begin).count()
              << "ms" << std::endl;

    const auto& prover_arg = ctx2.get_argument();
    std::cout << "----------------------------------------" << std::endl
              << "validation of linear: Not needed"
              << "validation of quadratic: " << validate(encoder, prover_arg.quadratic()) << std::endl
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
    for (size_t i = 0; i < sample_size; i++) {
        std::cout << sample_index[i] << " ";
    }
    std::cout << std::endl;

    auto decommit = tree.decommit(sample_index);

    // std::cin >> dummy;


    // Stage 3
    // -------------------------------------------------------------------------------- //
    

    encoder.seed(encoder_seed);
    zkp::stage3_prover_context<poly_t> ctx3(encoder, sample_index);
    auto stage3_begin = std::chrono::high_resolution_clock::now();
    {
        run_program(m, ctx3, func);
    }
    auto stage3_end = std::chrono::high_resolution_clock::now();

    std::cout << "Stage3 time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(stage3_end - stage3_begin).count()
              << "ms" << std::endl;

    std::cout << "----------------------------------------" << std::endl
              << "Saved samples: " << ctx3.get_sample().size() << std::endl
              << "----------------------------------------" << std::endl;
    std::cout << "Linear constraints: " << ctx3.linear_count << std::endl
              << "Quadratic constraints: " << ctx3.quad_count << std::endl
              << "Total count: " << ctx3.linear_count + ctx3.quad_count << std::endl;

    // std::cin >> dummy;


    // Verifier
    // -------------------------------------------------------------------------------- //
    

    encoder.seed(encoder_seed);
    zkp::verifier_context<poly_t> vctx(encoder, hash, sample_index, ctx3.get_sample());
    auto verify_begin = std::chrono::high_resolution_clock::now();
    {
        run_program(m, vctx, func, false);
    }
    auto verify_end = std::chrono::high_resolution_clock::now();

    const auto& verifier_arg = vctx.get_argument();
    auto rhash = zkp::merkle_tree<zkp::sha256>::recommit(vctx.builder(), decommit);

    std::cout << "----------------------------------------" << std::endl;
    show_hash(rhash);
    std::cout << "----------------------------------------" << std::endl;

    bool verify_result = true;
    for (size_t i = 0; i < sample_size; i++) {
        verify_result = verify_result &&
            (prover_arg.code()[sample_index[i]] == verifier_arg.code()[i]) &&
            (prover_arg.linear()[sample_index[i]] == verifier_arg.linear()[i]) &&
            (prover_arg.quadratic()[sample_index[i]] == verifier_arg.quadratic()[i]);
    }
    verify_result = verify_result && (hash == rhash);

    std::cout << "Verify time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(verify_end - verify_begin).count()
              << "ms" << std::endl
              << "Verify result: " << verify_result << std::endl;
    
    return 0;
}
