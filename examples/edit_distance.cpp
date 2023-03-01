extern "C" {

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
            pre = temp;
        }
    }
    return cur[n];
}

bool statement(const char *word1, const char* word2, const int m, const int n) {
    return minDistance(word1, word2, m, n) < 5;
}

}  // extern "C"
