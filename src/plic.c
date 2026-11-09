#include "plic.h"
#include "riscv.h"
#include "virt.h"

#define PLIC_PRIORITY(irq) ((volatile unsigned int *)(VIRT_PLIC + 4 * (irq)))
#define PLIC_ENABLE        ((volatile unsigned int *)(VIRT_PLIC + 0x2000))
#define PLIC_THRESHOLD     ((volatile unsigned int *)(VIRT_PLIC + 0x200000))
#define PLIC_CLAIM         ((volatile unsigned int *)(VIRT_PLIC + 0x200004))

void plic_init(void) {
    *PLIC_THRESHOLD = 0;
    csr_set(mie, MIE_MEIE);
}

void plic_enable(unsigned int irq) {
    *PLIC_PRIORITY(irq) = 1;
    PLIC_ENABLE[irq / 32] |= 1U << (irq % 32);
}

unsigned int plic_claim(void) {
    return *PLIC_CLAIM;
}

void plic_complete(unsigned int irq) {
    *PLIC_CLAIM = irq;
}
