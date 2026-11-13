#include "panic.h"
#include "kprintf.h"
#include "riscv.h"

void panic(const char *msg) {
    csr_clear(sstatus, SSTATUS_SIE);

    kprintf("panic: %s\n", msg);

    for (;;) {
        __asm__ volatile ("wfi");
    }
}
