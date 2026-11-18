// String helpers for both the kernel and user programs. Built once and linked
// into each, so this code must not depend on either side.

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
