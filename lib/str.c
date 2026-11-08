#include "str.h"

int str_equal(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }

    return *a == *b;
}
