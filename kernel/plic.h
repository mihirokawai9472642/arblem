#pragma once

// Set the PLIC MMIO base address and enable external interrupts on this hart.
void plic_init(unsigned long base);

// Accept every enabled IRQ and enable sie.SEIE on the current hart.
void plic_init_hart(void);

// Let irq interrupt the current hart in S-mode.
void plic_enable(unsigned int irq);

// Take the pending IRQ for the current hart (0 if none). Pass it to
// plic_complete when done.
unsigned int plic_claim(void);
void plic_complete(unsigned int irq);
