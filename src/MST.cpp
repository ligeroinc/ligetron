#include<stdio.h>
#include<stdlib.h>
#include<string.h>
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

int minwtSpanningTree(const char* word1, const char* word2, const int m, const int n) {
    int pre;
    int wt[m/6];
    int lnode[m/6];
    int rnode[m/6];
    int spt[n];
    int spt_left[n];
    int spt_right[n];
    char *end;
    int v=0;
    for(int j=0;j<m;j+=9)
    {
	    lnode[j/9] = 100*(word1[j]-'0')+10*(word1[j+1]-'0')+(word1[j+2]-'0');
	    rnode[j/9] = 100*(word1[j+3]-'0')+10*(word1[j+4]-'0')+(word1[j+5]-'0');
	    wt[j/9] = 100*(word1[j+6]-'0')+10*(word1[j+7]-'0')+(word1[j+8]-'0');
    }
    int e = m/9;
    //bubble sort
    for(int i = 0; i < e;i++)
    {
	    for(int j = 0; j < e - 1;j++)
	    {
		    bool cond = wt[j] > wt[j+1];

		    int temp = wt[j];
		    wt[j] = oblivious_if(cond,wt[j+1],temp);
		    wt[j+1] = oblivious_if(cond,temp,wt[j+1]);
		    
		    temp = lnode[j];
		    lnode[j] = oblivious_if(cond,lnode[j+1],temp);
		    lnode[j+1] = oblivious_if(cond,temp,lnode[j+1]);
		    
		    temp = rnode[j];
		    rnode[j] = oblivious_if(cond,rnode[j+1],temp);
		    rnode[j+1] = oblivious_if(cond,temp,rnode[j+1]);
	    }
    }

    spt[0] = wt[0];
    spt_left[0] = lnode[0];
    spt_right[0] = rnode[0];
    int pos=1;
    for(int i = 1; i < e ; i++)
    {
	    bool lcond = false;
	    bool rcond = false;
	    for(int j = 0; j < i; j++)
	    {
		    lcond = lcond || (lnode[i] == lnode[j]) || (lnode[i] == rnode[j]);
		    rcond = rcond || (rnode[i] == lnode[j]) || (rnode[i] == rnode[j]);
	    }
	    for(int k = 0; k < n; k++)
	    {
		    bool cond = (lcond && rcond) || (k != pos);
		    spt[k] = oblivious_if(cond,spt[k],wt[i]);
		    spt_left[k] = oblivious_if(cond,spt_left[k],lnode[i]);
		    spt_right[k] = oblivious_if(cond,spt_right[k],rnode[i]);
	    }
	    pos = oblivious_if(lcond && rcond, pos, pos+1);

    }
    int totalwt=0;
    for(int i = 0; i < n; i++)
    {
	    totalwt = totalwt + spt[i];
    }

    return totalwt;
}

bool statement(const char *word1, const char* word2, const int m, const int n) {
    return minwtSpanningTree(word1, word2, m, n) < 15;
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
