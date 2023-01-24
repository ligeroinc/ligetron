#include <bridge.hpp>
#include <wabt/binary-reader-ir.h>
#include <wabt/binary-reader.h>
#include <sstream>
#include <fstream>
#include <runtime.hpp>
#include <execution.hpp>
// #include <zkp/circuit.hpp>
// #include <context.hpp>
// #include <zkp/prover_context.hpp>
// #include <zkp/prover_execution.hpp>
#include <zkp/poly_field.hpp>

#include <chrono>

#include <fixed_vector.hpp>

#include <zkp/nonbatch_execution.hpp>
#include <zkp/nonbatch_context.hpp>

using namespace wabt;
using namespace ligero;
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

template <typename Decoder, typename Poly>
bool validate_sum(Decoder& dec, Poly p) {
    using field = typename Poly::field_type;
    dec.decode(p);
    field acc = static_cast<typename field::value_type>(0);
    for (size_t i = 0; i < p.size(); i++) {
        acc += field{p[i]};
    }
    return acc.data() == 0;
}

size_t l = 1024, d = 2048, n = 4096;
using poly_t = zkp::primitive_poly<params::modulus>;

// using value_type = numeric_value<u32vec, s32vec, u64vec, s64vec>;
using frame_type = zkp_frame<value_t, typename zkp::gc_row<poly_t>::reference>;
using svalue_type = stack_value<value_t, label, frame_type>;

std::string str_a, str_b;

template <typename Context>
void run_program(Module& m, Context& ctx, size_t func, bool fill = true) {
    store_t store;
    ctx.store(&store);
    zkp::nonbatch_executor exe(ctx);
    auto module = instantiate(store, m, exe);
    ctx.module(&module);

    // std::cout << "Functions: " << module.funcaddrs.size() << std::endl;
    // for (const auto& f : store.functions) {
    //     std::cout << "Func: " << f.name << std::endl;
    // }

    // {
    //     std::vector<u8> arg(32);
    //     u32 *p = reinterpret_cast<u32*>(arg.data());
    //     for (auto i = 0; i < 8; i++) {
    //         p[i] = 8-i;
    //     }
    //     std::vector<u8> data(arg);
    //     ctx.set_args(data);
    // }
    constexpr size_t offset = 8388608;
    size_t len1 = str_a.size(), len2 = str_b.size();
    size_t offset1 = offset + len1;

    auto& mem = store.memorys[0].data;

    for (size_t i = 0; i < len1; i++) {
        mem[offset + i] = str_a[i];
    }

    for (size_t i = 0; i < len2; i++) {
        mem[offset1 + i] = str_b[i];
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

// EM_JS(void, emscripten_save_file, (const char *str, int size), {
//         var filename = "proof.data";
//         let content = UTF8ToString(str, size);
//         // let content = Module.FS.readFile(filename);
//         // err(`Offering download of "${filename}", with ${content.length} bytes...`);

//         var a = document.createElement('a');
//         a.download = filename;
//         a.href = URL.createObjectURL(new Blob([content], {type: mime}));
//         a.style.display = 'none';

//         document.body.appendChild(a);
//         a.click();
//         setTimeout(() => {
//                 document.body.removeChild(a);
//                 URL.revokeObjectURL(a.href);
//             }, 2000);
//     });

int main(int argc, char *argv[]) {
    // std::string dummy;

    const char *file = argv[1];
    size_t func = 1;

    l = std::stol(argv[2]);
    d = std::max(std::bit_ceil(l), 256UL);
    n = 2 * d;

    std::cout << "l: " << l << ", k: " << d << ", n: " << n << std::endl;
    
    str_a = argv[3];
    str_b = argv[4];
    // const char *file = argv[1];
    // size_t func = std::stoi(argv[2]);
    
    std::ifstream fs(file, std::ios::binary);
    std::stringstream ss;
    ss << fs.rdbuf();
    const std::string content = ss.str();
    // std::cout << "read file " << file << " with size " << content.size() << std::endl;
    
    Module m;
    Result r = ReadBinaryIr(file, content.data(), content.size(), ReadBinaryOptions{}, nullptr, &m);

    assert(r != Result::Error);

    // std::cout << "Imports: " << m.imports.size() << std::endl;
    // for (const auto *imp : m.imports) {
    //     if (auto *p = dynamic_cast<const FuncImport*>(imp)) {
    //         std::cout << imp->module_name << "." << imp->field_name << " => " << p->func.name << std::endl;
    //     }
    // }

    std::cout << "Proving " << file <<  " of " << l << " instances" << std::endl;
    // std::cout << "l: " << l << " d: " << d << " K: " << n << std::endl;

    std::random_device rd;
    zkp::random_seeds encoder_seed { rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd() };
    zkp::reed_solomon64 encoder(params::modulus, l, d, n);

    
    // Stage 1
    // -------------------------------------------------------------------------------- //

    std::cout << "Start Stage 1" << std::endl;

    // zkp::stage1_prover_context<value_type, svalue_type, poly_t, zkp::sha256> ctx(encoder);    
    zkp::nonbatch_stage1_context<value_t, svalue_type, poly_t, zkp::sha256> ctx(encoder, encoder_seed);
    {
        auto t = make_timer("stage1", "run");
        run_program(m, ctx, func);
        ctx.assert_one(ctx.stack_pop_var());
        ctx.finalize();
    }

    // size_t stage1_time = std::chrono::duration_cast<std::chrono::milliseconds>(stage1_end - stage1_begin).count();
    // std::cout << "Prover Stage1 time: "
    //           << stage1_time << "ms"
    //           << " (NTT: " << encoder.timer_ / 1000 << "ms)"
    //           << std::endl;
    // encoder.timer_ = 0;

    zkp::merkle_tree<zkp::sha256> tree = ctx.builder();
    auto stage1_root = tree.root();
    // auto hash = ctx.root_hash();
    // std::cout << "----------------------------------------" << std::endl << std::endl;
    std::cout << "Root of Merkle Tree: ";
    show_hash(stage1_root);
    std::cout << "----------------------------------------" << std::endl;

    // std::cin >> dummy;

    
    // Stage 2
    // -------------------------------------------------------------------------------- //
    std::cout << "Start Stage 2" << std::endl;
    
    zkp::nonbatch_stage2_context<value_t, svalue_type, poly_t> ctx2(encoder, encoder_seed, stage1_root);
    {
        auto t = make_timer("stage2", "run");
        run_program(m, ctx2, func);
        ctx2.assert_one(ctx2.stack_pop_var());
        ctx2.finalize();
    }

    // size_t stage2_time = std::chrono::duration_cast<std::chrono::milliseconds>(stage2_end - stage2_begin).count();
    // std::cout << "Stage2 time: "
    //           << stage2_time << "ms"
    //           << " (NTT: " << encoder.timer_ / 1000 << "ms)"
    //           << std::endl;
    // encoder.timer_ = 0;

    // auto stage2_result = ctx2.stack_pop_var();
    // decltype(stage2_result) one{ 1U, ctx2.encode_const(1U) };
    // auto statement = ctx2.eval(stage2_result - one);

    auto& prover_arg = ctx2.get_argument();

    std::vector<poly_t> linear_polys = prover_arg.linear();
    std::vector<poly_t> quad_polys = prover_arg.quadratic();
    
    std::cout << std::boolalpha;
    std::cout << "Num quadratic constraints: " << ctx2.constraints_count() << std::endl;
    std::cout << "Validation of linear constraints:    ";
    for (size_t i = 0; i < params::num_linear_test; i++) {
        std::cout << validate_sum(encoder, linear_polys[i]) << " ";
    }
    std::cout << std::endl;
    std::cout << "Validation of quadratic constraints: ";
    for (size_t i = 0; i < params::num_quadratic_test; i++) {
        std::cout << validate(encoder, quad_polys[i]) << " ";
    }
    std::cout << std::endl
              << "----------------------------------------" << std::endl;

    constexpr size_t sample_size = 189;
    std::cout << "sample size: " << sample_size << std::endl;
    
    auto stage2_seed = zkp::hash<zkp::sha256>(ctx2.get_argument());
    zkp::hash_random_engine<zkp::sha256> engine(stage2_seed);
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

    auto decommit = tree.decommit(sample_index);

//     // std::cin >> dummy;

    // Stage 3
    // -------------------------------------------------------------------------------- //
    std::cout << "Start Stage 3" << std::endl;

    std::ofstream proof("proof.data", std::ios::out | std::ios::binary | std::ios::trunc);
    boost::archive::binary_oarchive oa(proof);

    auto& codes = prover_arg.code();
    for (auto& code : codes)
        encoder.partial_decode(code);

    {
        std::vector<poly_t> linear_polys = prover_arg.linear();
        std::vector<poly_t> quad_polys = prover_arg.quadratic();
        std::cout << "Validation of linear constraints:    ";
        for (size_t i = 0; i < params::num_linear_test; i++) {
            std::cout << validate_sum(encoder, linear_polys[i]) << " ";
        }
        std::cout << std::endl;
        std::cout << "Validation of quadratic constraints: ";
        for (size_t i = 0; i < params::num_quadratic_test; i++) {
            std::cout << validate(encoder, quad_polys[i]) << " ";
        }
        std::cout << std::endl
                  << "----------------------------------------" << std::endl;
    }

    
    oa << encoder_seed
       << stage1_root
       << stage2_seed
       << codes
       << prover_arg.quadratic()
       << prover_arg.linear()
       << decommit;

    size_t saved_rows = 0;
    auto save = [&oa, &saved_rows](const auto& poly) { oa << poly; saved_rows++; };

    zkp::nonbatch_stage3_context<value_t, svalue_type, poly_t, decltype(save)> ctx3(encoder, encoder_seed, sample_index, save);
    {
        auto t = make_timer("stage3", "run");
        run_program(m, ctx3, func);
        ctx3.assert_one(ctx3.stack_pop_var());
        ctx3.finalize();
    }

    // size_t stage3_time = std::chrono::duration_cast<std::chrono::milliseconds>(stage3_end - stage3_begin).count();
    // std::cout << "Stage3 time: "
    //           << stage3_time
    //           << "ms"
    //           << " (NTT: " << encoder.timer_ / 1000 << "ms)"
    //           << std::endl;
    // encoder.timer_ = 0;

    // std::cout << "----------------------------------------" << std::endl
    std::cout << "Saved rows: " << saved_rows << std::endl
              << "----------------------------------------" << std::endl;
    // std::cout << "Quadratic constraints: " << ctx3.quad_count << std::endl;
              // << "Total count: " << ctx3.linear_count + ctx3.quad_count << std::endl;
// << "Linear constraints: " << ctx3.linear_count << std::endl

    // std::cin >> dummy;

    std::cout << std::endl << "Done!" << std::endl << std::endl
              // << "Total prover time: "
              // << std::setprecision(2)
              // << (stage1_time + stage2_time + stage3_time) / 1000.0
              // << "s"
              << std::endl << std::endl
              << "| | | Push button below to download the proof"
              << std::endl
              << "V V V"
              << std::endl;

    proof.close();

// #include <filesystem>
//     namespace fss = std::filesystem;
//     std::string path = "/";
//     for (const auto & entry : fss::directory_iterator(path))
//         std::cout << entry.path() << std::endl;

    // const char * str = "abcde";
    // foo(str, 5);

    // EM_ASM({
    //         FS.readFile('proof.data');
    //     });
    
    // std::string str = proof.str();
    // EM_ASM({
    //         emscripten_save_file($0, $1);
    //     }, str.c_str(), str.size());

    show_timer();
    return 0;
}
