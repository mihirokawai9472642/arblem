#include "uart.h"
#include "virt.h"

#define UART_THR ((volatile char *)(VIRT_UART0 + 0))

void uart_putc(char c) {
    *UART_THR = c;
}
