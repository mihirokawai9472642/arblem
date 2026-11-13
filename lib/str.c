#include "str.h"

int str_equal(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }

    return *a == *b;
}

unsigned long str_len(const char *s) {
    unsigned long len = 0;

    while (s[len]) {
        len++;
    }

    return len;
}
