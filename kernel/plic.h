#pragma once

void plic_init(void);
void plic_enable(unsigned int irq);
unsigned int plic_claim(void);
void plic_complete(unsigned int irq);
