// Platform-Level Interrupt Controller. Arblem uses only context 1, which on
// the QEMU virt machine is hart 0 in S-mode. See docs/devices.md.

#include "plic.h"
#include "riscv.h"
#include "virt.h"

#define PLIC_PRIORITY(irq)                                                     \
    ((volatile unsigned int *)(VIRT_PLIC + 4UL * (irq))) // Per IRQ, 0 = never
#define PLIC_ENABLE                                                            \
    ((volatile unsigned int *)(VIRT_PLIC + 0x2080)) // Context 1 enable bits
#define PLIC_THRESHOLD                                                         \
    ((volatile unsigned int *)(VIRT_PLIC + 0x201000)) // Context 1 threshold
#define PLIC_CLAIM                                                             \
    ((volatile unsigned int *)(VIRT_PLIC +                                     \
                               0x201004)) // Context 1 claim/complete

void plic_init(void) {
    // Deliver every IRQ with a priority above 0.
    *PLIC_THRESHOLD = 0;
    csr_set(sie, SIE_SEIE);
}

void plic_enable(unsigned int irq) {
    *PLIC_PRIORITY(irq) = 1;
    PLIC_ENABLE[irq / 32] |= 1U << (irq % 32);
}

// Reading the claim register returns the highest-priority pending IRQ, or 0
// if there is none, and marks it as being handled.
unsigned int plic_claim(void) {
    return *PLIC_CLAIM;
}

// Writing the IRQ back allows the PLIC to deliver it again.
void plic_complete(unsigned int irq) {
    *PLIC_CLAIM = irq;
}
