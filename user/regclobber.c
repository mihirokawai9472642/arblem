// Helper for regcheck. Keeps overwriting gp and tp while regcheck runs, and
// exits with status 1 if they did not start cleared.

#include "sys.h"

int main(void) {
    unsigned long gp;
    unsigned long tp;

    __asm__ volatile("mv %0, gp" : "=r"(gp));
    __asm__ volatile("mv %0, tp" : "=r"(tp));

    for (volatile unsigned long i = 0; i < 30000000; i++) {
        __asm__ volatile("li gp, 0x3333");
        __asm__ volatile("li tp, 0x4444");
    }

    if (gp != 0 || tp != 0) {
        return 1;
    }

    return 0;
}
