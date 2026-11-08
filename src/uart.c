#include "uart.h"
#include "virt.h"

#define UART_RBR ((volatile unsigned char *)(VIRT_UART0 + 0))
#define UART_THR ((volatile unsigned char *)(VIRT_UART0 + 0))
#define UART_LSR ((volatile unsigned char *)(VIRT_UART0 + 5))

#define UART_LSR_DR (1 << 0)

void uart_putc(char c) {
    *UART_THR = c;
}

int uart_getc(void) {
    if ((*UART_LSR & UART_LSR_DR) == 0) {
        return -1;
    }

    return *UART_RBR;
}
