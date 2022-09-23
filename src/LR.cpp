// #include <array>
// #if defined(__EMSCRIPTEN__)
// #include <emscripten.h>
// #endif

// // #include <runtime.hpp>
// #include <instruction.hpp>

#define LIGEROVM_EXTERN(ENV, NAME) __attribute__((import_module(#ENV), import_name(#NAME))) extern

extern "C" {

LIGEROVM_EXTERN(env, get_witness)
int get_witness(int);
// LIGEROVM_EXTERN(env, get_argc)
// int get_argc(void);
LIGEROVM_EXTERN(env, get_witness_size)
int get_witness_size();

// LIGEROVM_EXTERN(env, a)
// int A();

// LIGEROVM_EXTERN(env, b)
// int B();

// LIGEROVM_EXTERN(env, c)
// int C();

// constexpr size_t size = 32;
// using fixed_array = int[size];

// #if defined(__EMSCRIPTEN__)
// EMSCRIPTEN_KEEPALIVE
// #endif
// int linear_inference(const int *input, int threshold) {
//     int N = get_witness_size(0);

//     int beta[N];
//     int konst[N];

//     for (int i = 0; i < N; i++) {
//         beta[i] = get_witness(0, i);
//         konst[i] = get_witness(1, i);
//     }

//     int acc = 0;
//     for (auto i = 0; i < N; i++) {
//         int tmp = input[i];
//         tmp *= beta[i];
//         tmp += konst[i];
//         acc += tmp;
//     }

//     return acc > threshold;
// }

inline int min(int a, int b) {
    return a <= b ? a : b;
}

inline int oblivious_if(bool cond, int t, int f) {
    int mask = static_cast<int>((1ULL << 33) - cond);
    return (mask & t) | (~mask & f);
}

int minDistance(const char* word1, const char* word2, const int m, const int n) {
    int pre;
    int cur[n + 1];

    for (int j = 0; j <= n; j++) {
        cur[j] = j;
    }
    
    for (int i = 1; i <= m; i++) {
        pre = cur[0];
        cur[0] = i;
        for (int j = 1; j <= n; j++) {
            int temp = cur[j];
            bool cond = word1[i - 1] == word2[j - 1];
            cur[j] = oblivious_if(cond,
                                  pre,
                                  min(pre, min(cur[j - 1], cur[j])) + 1);
            // if (word1[i - 1] == word2[j - 1]) {
            //     cur[j] = pre;
            // } else {
            //     cur[j] = min(pre, min(cur[j - 1], cur[j])) + 1;
            // }
            pre = temp;
        }
    }
    return cur[n];
}

// int main(int argc, char *argv[]) {
//     std::cout << minDistance(argv[3], argv[4], std::stoi(argv[1]), std::stoi(argv[2])) << std::endl;
// }

// int* bubbleSort(int *arr) {
//     const int N = get_witness_size();
//     // int arr[N];

//     for (int i = 0; i < N; i++) {
//         arr[i] = get_witness(i);
//     }
    
//     for (int i = 0; i < N; i++) {
//         for (int j = 0; j < N - i - 1; j++) {
//             if (arr[j] > arr[j + 1]) {
//                 int tmp = arr[j];
//                 arr[j] = arr[j+1];
//                 arr[j+1] = tmp;
//             }
//         }
//     }
//     return arr;
// }


}
