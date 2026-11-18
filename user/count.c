// Count from 0 to 4 with a busy loop between lines. The loop never makes a
// system call, so the program only gives up the CPU when the timer preempts it.

#include "sys.h"

// In .bss, so the program also exercises a zero-filled, writable segment.
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
