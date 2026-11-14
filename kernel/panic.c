#include "panic.h"
#include "kprintf.h"
#include "riscv.h"
#include "sbi.h"
#include "sbinum.h"

void panic(const char *msg) {
    csr_clear(sstatus, SSTATUS_SIE);

    kprintf("panic: %s\n", msg);

    sbi_shutdown(SBI_SRST_REASON_FAILURE);

    for (;;) {
        __asm__ volatile ("wfi");
    }
}
