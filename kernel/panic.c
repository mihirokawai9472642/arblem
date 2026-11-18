// Fatal kernel errors.

#include "panic.h"
#include "kprintf.h"
#include "riscv.h"
#include "sbi.h"

// Interrupts go off first, so nothing runs after the message. The smoke test
// looks for the "panic: " prefix.
void panic(const char *msg) {
    csr_clear(sstatus, SSTATUS_SIE);

    kprintf("panic: %s\n", msg);

    sbi_shutdown(SBI_SRST_REASON_FAILURE);

    // If the shutdown fails, stop here.
    for (;;) {
        __asm__ volatile ("wfi");
    }
}
