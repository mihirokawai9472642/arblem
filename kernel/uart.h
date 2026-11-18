#pragma once

// Enable the receive interrupt. The PLIC must also enable VIRT_UART0_IRQ.
void uart_init(void);

// Send one byte.
void uart_putc(char c);

// Return the next received byte, sleeping until one arrives.
char uart_getc(void);

// Called by trap_handler for VIRT_UART0_IRQ.
void uart_interrupt(void);
