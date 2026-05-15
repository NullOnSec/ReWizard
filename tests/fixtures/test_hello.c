#include <stdio.h>

int factorial(int n) {
    int result = 1;
    for (int i = 1; i <= n; ++i) {
        result *= i;
    }
    return result;
}

int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

void greet(const char* name) {
    if (name) {
        printf("Hello, %s!\n", name);
    } else {
        printf("Hello, World!\n");
    }
}

int main() {
    greet("ReWizard");

    int f = factorial(5);
    int m = max(f, 100);

    printf("factorial(5) = %d\n", f);
    printf("max(%d, 100) = %d\n", f, m);

    return m > 0 ? 0 : 1;
}
