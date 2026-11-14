#include "kprintf.h"
#include "sbi.h"
#include "uart.h"

#define KPRINTF_BUF_SIZE 64

struct kprintf_buf {
    char data[KPRINTF_BUF_SIZE];
    unsigned long len;
};

static void kprintf_flush(struct kprintf_buf *b) {
    if (sbi_console_write(b->data, b->len) != 0) {
        for (unsigned long i = 0; i < b->len; i++) {
            uart_putc(b->data[i]);
        }
    }

    b->len = 0;
}

static void putc(struct kprintf_buf *b, char c) {
    b->data[b->len++] = c;

    if (b->len == KPRINTF_BUF_SIZE) {
        kprintf_flush(b);
    }
}

static void putstr(struct kprintf_buf *b, const char *s) {
    for (; *s; s++) {
        putc(b, *s);
    }
}

static void puthex(struct kprintf_buf *b, unsigned long x) {
    putc(b, '0');
    putc(b, 'x');

    for (int shift = 60; shift >= 0; shift -= 4) {
        putc(b, "0123456789abcdef"[(x >> shift) & 0xf]);
    }
}

void kprintf(const char *f, ...) {
    struct kprintf_buf b;
    __builtin_va_list ap;

    b.len = 0;
    __builtin_va_start(ap, f);

    for (; *f; f++) {
        if (*f == '%' && *(f + 1) == 'x') {
            puthex(&b, __builtin_va_arg(ap, unsigned long));
            f++;
        } else if (*f == '%' && *(f + 1) == 's') {
            putstr(&b, __builtin_va_arg(ap, const char *));
            f++;
        } else {
            putc(&b, *f);
        }
    }

    __builtin_va_end(ap);

    kprintf_flush(&b);
}
