// Platform-Level Interrupt Controller. On the QEMU virt machine, hart h uses
// context 2 * h for M-mode and context 2 * h + 1 for S-mode. See
// docs/devices.md.

#include "plic.h"
#include "cpu.h"
#include "riscv.h"

static unsigned long plic_base;

static unsigned long plic_context(void) {
    return 2 * cpu_current()->hartid + 1UL;
}

#define PLIC_PRIORITY(irq)                                                     \
    ((volatile unsigned int *)(plic_base + 4UL * (irq))) // Per IRQ, 0 = never
#define PLIC_ENABLE(ctx)                                                       \
    ((volatile unsigned int *)(plic_base + 0x2000UL +                          \
                               0x80UL * (ctx))) // Per-context enable bits
#define PLIC_THRESHOLD(ctx)                                                    \
    ((volatile unsigned int *)(plic_base + 0x200000UL +                        \
                               0x1000UL * (ctx))) // Per-context threshold
#define PLIC_CLAIM(ctx)                                                        \
    ((volatile unsigned int *)(plic_base + 0x200004UL +                        \
                               0x1000UL *                                      \
                                   (ctx))) // Per-context claim/complete

void plic_init_hart(void) {
    // Deliver every IRQ with a priority above 0 to this hart's S-mode context.
    *PLIC_THRESHOLD(plic_context()) = 0;
    csr_set(sie, SIE_SEIE);
}

void plic_init(unsigned long base) {
    plic_base = base;
    plic_init_hart();
}

void plic_enable(unsigned int irq) {
    unsigned long ctx = plic_context();

    *PLIC_PRIORITY(irq) = 1;
    PLIC_ENABLE(ctx)[irq / 32] |= 1U << (irq % 32);
}

// Reading the claim register returns the highest-priority pending IRQ, or 0
// if there is none, and marks it as being handled.
unsigned int plic_claim(void) {
    return *PLIC_CLAIM(plic_context());
}

// Writing the IRQ back allows the PLIC to deliver it again.
void plic_complete(unsigned int irq) {
    *PLIC_CLAIM(plic_context()) = irq;
}
