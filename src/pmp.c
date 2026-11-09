#include "pmp.h"
#include "riscv.h"

void pmp_init(unsigned long user_start, unsigned long user_end) {
    csr_write(pmpaddr0, user_start >> 2);
    csr_write(pmpaddr1, user_end >> 2);
    csr_write(pmpcfg0, (PMP_TOR | PMP_R | PMP_W | PMP_X) << 8);
}
