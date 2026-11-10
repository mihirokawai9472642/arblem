#include "uart.h"
#include "thread.h"
#include "virt.h"

#define UART_RBR ((volatile unsigned char *)(VIRT_UART0 + 0))
#define UART_THR ((volatile unsigned char *)(VIRT_UART0 + 0))
#define UART_IER ((volatile unsigned char *)(VIRT_UART0 + 1))
#define UART_LSR ((volatile unsigned char *)(VIRT_UART0 + 5))

#define UART_IER_RDI (1 << 0)
#define UART_LSR_DR  (1 << 0)

static char uart_rx_wait;

void uart_init(void) {
    *UART_IER = UART_IER_RDI;
}

void uart_putc(char c) {
    *UART_THR = c;
}

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

void uart_interrupt(void) {
    thread_wakeup(&uart_rx_wait);
}
