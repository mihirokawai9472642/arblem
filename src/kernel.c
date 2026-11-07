#define UART0 ((volatile char *)0x10000000)

static void kernel_putc(char c) {
    *UART0 = c;
}

static void kernel_puts(const char *s) {
    while (*s) {
        kernel_putc(*s++);
    }
}

static void kernel_puthex(unsigned long x) {
    kernel_puts("0x");

    for (int shift = 60; shift >= 0; shift -= 4) {
        kernel_putc("0123456789abcdef"[(x >> shift) & 0xf]);
    }
}

void kernel_main(unsigned long hartid, unsigned long dtb) {
    kernel_puts("Hello World from Arblem :)\n");

    kernel_puts("HartID: ");
    kernel_puthex(hartid);
    kernel_puts("\n");

    kernel_puts("DTB: ");
    kernel_puthex(dtb);
    kernel_puts("\n");
}
