// Driver for the NS16550-compatible UART of the QEMU virt machine.
// Output is polled; input sleeps until the receive interrupt.
// See docs/devices.md.

#include "uart.h"
#include "thread.h"
#include "virt.h"

// Registers, at byte offsets from the base address. RBR and THR share an
// offset: reads come from RBR, writes go to THR.
#define UART_RBR ((volatile unsigned char *)(VIRT_UART0 + 0)) // Receive buffer
#define UART_THR                                                               \
    ((volatile unsigned char *)(VIRT_UART0 + 0)) // Transmit holding
#define UART_IER                                                               \
    ((volatile unsigned char *)(VIRT_UART0 + 1)) // Interrupt enable
#define UART_LSR ((volatile unsigned char *)(VIRT_UART0 + 5)) // Line status

#define UART_IER_RDI (1 << 0) // Interrupt when received data is available
#define UART_LSR_DR  (1 << 0) // A received byte is waiting in RBR

// Wait channel for threads in uart_getc.
static char uart_rx_wait;

void uart_init(void) {
    *UART_IER = UART_IER_RDI;
}

// Does not wait for THR to be empty. QEMU accepts every byte at once, but real
// hardware would need to poll LSR first.
void uart_putc(char c) {
    *UART_THR = c;
}

// Return the waiting byte, or -1 if there is none.
static int uart_poll(void) {
    if ((*UART_LSR & UART_LSR_DR) == 0) {
        return -1;
    }

    return *UART_RBR;
}

char uart_getc(void) {
    for (;;) {
        int c = uart_poll();

        if (c >= 0) {
            return c;
        }

        thread_sleep(&uart_rx_wait);
    }
}

// Reading RBR in uart_getc clears the device's interrupt condition, so waking
// the readers is all the interrupt handler has to do.
void uart_interrupt(void) {
    thread_wakeup(&uart_rx_wait);
}
