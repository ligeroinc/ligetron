// #include <array>
// #if defined(__EMSCRIPTEN__)
// #include <emscripten.h>
// #endif

// // #include <runtime.hpp>
// #include <instruction.hpp>

extern "C" {

// constexpr size_t size = 32;
// using fixed_array = int[size];

// #if defined(__EMSCRIPTEN__)
// EMSCRIPTEN_KEEPALIVE
// #endif
// int program_main(int argc, int *argv[]) {
//     const int size = argv[0][0];
//     const int *beta = argv[1];
//     const int *konst = argv[2];
//     const int *input = argv[2];

//     int acc = 0;
//     for (auto i = 0; i < size; i++) {
//         int tmp = input[i];
//         tmp *= beta[i];
//         tmp += konst[i];
//         acc += tmp;
//     }

//     return acc;
// }

// __attribute__((import_module("env"), import_name("get_witness")))
// int get_witness(int i);

int program_main(int x, int y) {
    return x + y + 1;
}


}

// int foobar() {
//     long *ptr = new long[5];
//     int p = static_cast<int>(ptr[2]);
//     delete[] ptr;
//     return p;
// }
