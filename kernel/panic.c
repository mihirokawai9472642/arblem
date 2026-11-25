// Fatal kernel errors.

#include "panic.h"
#include "kprintf.h"
#include "riscv.h"
#include "sbi.h"

static int panicking;

// Interrupts go off first, so nothing runs after the message. The smoke test
// looks for the "panic: " prefix. Only the first hart to panic prints and
// shuts down; any other hart halts quietly so output is not garbled.
void panic(const char *msg) {
    csr_clear(sstatus, SSTATUS_SIE);
    kprintf_disable_lock();

    if (__atomic_exchange_n(&panicking, 1, __ATOMIC_ACQ_REL) == 0) {
        kprintf("panic: %s\n", msg);
        sbi_shutdown(SBI_SRST_REASON_FAILURE);
    }

    // If the shutdown fails (or another hart is already shutting down), stop.
    for (;;) {
        __asm__ volatile("wfi");
    }
}
