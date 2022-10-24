#include <bridge.hpp>
#include <wabt/binary-reader-ir.h>
#include <wabt/binary-reader.h>
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

#include <fixed_vector.hpp>

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
constexpr uint64_t modulus = 1125625028935681ULL;
constexpr size_t l = 128, d = 256, n = 512;
using poly_t = zkp::primitive_poly<modulus>;

using u32vec = fixed_vector<u32, l>;
using s32vec = fixed_vector<s32, l>;
using u64vec = fixed_vector<u64, l>;
using s64vec = fixed_vector<s64, l>;
using value_type = numeric_value<u32vec, s32vec, u64vec, s64vec>;
using frame_type = frame<value_type>;
using svalue_type = stack_value<value_type, label, frame_type>;

size_t str_len = 10;

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
    size_t len1 = str_len, len2 = str_len;
    size_t offset1 = offset + len1;
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

    const char *file = "edit.wasm";
    size_t func = 0;
    str_len = std::stoi(argv[1]);
    // const char *file = argv[1];
    // size_t func = std::stoi(argv[2]);
    
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

    

    std::ifstream proof("proof.data", std::ios::in | std::ios::binary);
    boost::archive::binary_iarchive ia(proof);
    
    unsigned int encoder_seed;
    ia >> encoder_seed;

    zkp::sha256::digest merkle_root;
    ia >> merkle_root;

    zkp::sha256::digest sample_seed;
    ia >> sample_seed;

    poly_t prover_code, prover_quad;
    ia >> prover_code;
    ia >> prover_quad;

    zkp::merkle_tree<zkp::sha256>::decommitment decommit;
    ia >> decommit;

    std::vector<poly_t> samples;
    ia >> samples;

    

    zkp::reed_solomon64 encoder(modulus, l, d, n, encoder_seed);

    constexpr size_t sample_size = 80;
    zkp::hash_random_engine<zkp::sha256> engine(sample_seed);
    std::vector<size_t> indexes(n), sample_index;
    std::iota(indexes.begin(), indexes.end(), 0);
    std::sample(indexes.cbegin(), indexes.cend(),
                std::back_inserter(sample_index),
                sample_size,
                engine);
    // std::cout << "Sampled indexes: ";
    // for (size_t i = 0; i < sample_size; i++) {
    //     std::cout << sample_index[i] << " ";
    // }
    // std::cout << std::endl;


    encoder.seed(encoder_seed);
    zkp::verifier_context<value_type, svalue_type, poly_t> vctx(encoder, merkle_root, sample_index, samples);
    auto verify_begin = std::chrono::high_resolution_clock::now();
    {
        run_program(m, vctx, func, false);
    }
    auto verify_end = std::chrono::high_resolution_clock::now();

    const auto& verifier_arg = vctx.get_argument();
    auto rhash = zkp::merkle_tree<zkp::sha256>::recommit(vctx.builder(), decommit);

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Prover Merkle Tree root:   ";
    show_hash(merkle_root);
    std::cout << "verifier Merkle Tree root: ";
    show_hash(rhash);
    std::cout << "----------------------------------------" << std::endl;
    
    bool merkle_result = merkle_root == rhash;
    bool code_result = true, quad_result = true;
    for (size_t i = 0; i < sample_size; i++) {
        code_result = code_result && (prover_code[sample_index[i]] == verifier_arg.code()[i]);
            // (prover_arg.linear()[sample_index[i]] == verifier_arg.linear()[i]) &&
        quad_result = quad_result && (prover_quad[sample_index[i]] == verifier_arg.quadratic()[i]);
    }

    bool verify_result = code_result && quad_result && merkle_result;


    std::cout << "Hash Verify result:      " << merkle_result << std::endl
              << "Code Verify result:      " << code_result << std::endl
              << "Quadratic Verify result: " << quad_result << std::endl
              << "Final Verify result:     " << verify_result << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    std::cout << "Verify time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(verify_end - verify_begin).count()
              << "ms" << std::endl;

    
    return 0;
}
