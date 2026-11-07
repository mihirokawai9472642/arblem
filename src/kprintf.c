#include "kprintf.h"
#include "uart.h"

static void puthex(unsigned long x) {
    uart_putc('0');
    uart_putc('x');

    for (int shift = 60; shift >= 0; shift -= 4) {
        uart_putc("0123456789abcdef"[(x >> shift) & 0xf]);
    }
}

void kprintf(const char *f, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, f);

    for (; *f; f++) {
        if (*f == '%' && *(f + 1) == 'x') {
            puthex(__builtin_va_arg(ap, unsigned long));
            f++;
        } else {
            uart_putc(*f);
        }
    }

    __builtin_va_end(ap);
}
