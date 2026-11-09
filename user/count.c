#include "sys.h"

static volatile int count;

int main(void) {
    for (count = 0; count < 5; count++) {
        sys_puts("count: ");
        sys_putc('0' + count);
        sys_putc('\n');

        for (volatile unsigned long i = 0; i < 10000000; i++) {
        }
    }

    return 0;
}
