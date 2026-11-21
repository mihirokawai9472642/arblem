#pragma once

// Set the UART MMIO base address and IRQ, and enable the receive interrupt.
// The PLIC must also enable the same IRQ.
void uart_init(unsigned long base, unsigned int irq);

// Return the PLIC IRQ number registered by uart_init.
unsigned int uart_irq(void);

// Send one byte.
void uart_putc(char c);

// Return the next received byte, sleeping until one arrives.
char uart_getc(void);

// Called by trap_handler for the UART IRQ.
void uart_interrupt(void);
