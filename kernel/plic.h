#pragma once

// Accept every enabled IRQ and enable external interrupts (sie.SEIE).
void plic_init(void);

// Let irq interrupt this hart in S-mode.
void plic_enable(unsigned int irq);

// Take the pending IRQ (0 if none). Pass it to plic_complete when done.
unsigned int plic_claim(void);
void plic_complete(unsigned int irq);
