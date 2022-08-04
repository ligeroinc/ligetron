#include <stdlib.h>

typedef struct Foo {
    int a, b, c;
} Foo;

int loop(int a, int b) {
    int *s = (int*)malloc(4);
    s[0] = 0;
    s[1] = 1;
    int ans = 0;
    for (int i = 2; i < 10; i++) {
        ans += s[0] + s[1];
        s[0] = s[1];
        s[1] = ans;
    }
    free(s);
    return ans;
}
