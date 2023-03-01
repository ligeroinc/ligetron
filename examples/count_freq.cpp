extern "C" {

inline int oblivious_if(bool cond, int t, int f) {
    int mask = static_cast<int>((1ULL << 33) - cond);
    return (mask & t) | (~mask & f);
}

int countFreq(const char *pat, const char *txt, int M, int N) {
    int res = 0;
 
    /* A loop to slide pat[] one by one */
    for (int i = 0; i <= N - M; i++) {
        /* For current index i, check for pattern match */
        int count = 0;
        for (int j = 0; j < M; j++) {
            count += oblivious_if(txt[i + j] != pat[j], 0, 1);
        }
 
        // if pat[0...M-1] = txt[i, i+1, ...i+M-1]
        res += oblivious_if(count == M, 1, 0);
    }
    return res;
}

bool statement(const char *word1, const char* word2, const int m, const int n) {
    return countFreq(word1, word2, m, n) < 3;
}

}  // extern "C"
