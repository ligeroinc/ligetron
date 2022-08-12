// #include <array>
// #if defined(__EMSCRIPTEN__)
// #include <emscripten.h>
// #endif

// // #include <runtime.hpp>
// #include <instruction.hpp>

#define LIGEROVM_EXTERN(ENV, NAME) __attribute__((import_module(#ENV), import_name(#NAME))) extern

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


LIGEROVM_EXTERN(env, get_witness)
int get_witness(int i);
LIGEROVM_EXTERN(env, get_witness_size)
int get_witness_size(void);

int* bubbleSort(int *arr) {
    const int N = get_witness_size();
    // int arr[N];

    for (int i = 0; i < N; i++) {
        arr[i] = get_witness(i);
    }
    
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                int tmp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = tmp;
            }
        }
    }
    return arr;
}


}

// int foobar() {
//     long *ptr = new long[5];
//     int p = static_cast<int>(ptr[2]);
//     delete[] ptr;
//     return p;
// }
