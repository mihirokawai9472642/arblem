#define UART0 ((volatile char *)0x10000000)

#define CLINT_MTIMECMP ((volatile unsigned long *)0x02004000)
#define CLINT_MTIME ((volatile unsigned long *)0x0200bff8)
#define TIMER_INTERVAL 10000000

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

static void kernel_printf(const char *f, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, f);

    for (; *f; f++) {
        if (*f == '%' && *(f + 1) == 'x') {
            kernel_puthex(__builtin_va_arg(ap, unsigned long));
            f++;
        } else {
            kernel_putc(*f);
        }
    }

    __builtin_va_end(ap);
}

static void kernel_timer_set(void) {
    *CLINT_MTIMECMP = *CLINT_MTIME + TIMER_INTERVAL;
}

void kernel_trap(void) {
    unsigned long mcause, mepc, mtval;

    __asm__ volatile ("csrr %0, mcause" : "=r"(mcause));
    __asm__ volatile ("csrr %0, mepc" : "=r"(mepc));
    __asm__ volatile ("csrr %0, mtval" : "=r"(mtval));

    if (mcause == 0x8000000000000007) {
        kernel_printf("Tick: %x\n", *CLINT_MTIME);

        kernel_timer_set();
        return;
    }

    kernel_printf("Trap!\n");
    kernel_printf("mcause: %x\n", mcause);
    kernel_printf("mepc: %x\n", mepc);
    kernel_printf("mtval: %x\n", mtval);

    if (mcause == 3) {
        unsigned short insn = *(volatile unsigned short *)mepc;
        mepc += ((insn & 3) == 3) ? 4 : 2;
        __asm__ volatile ("csrw mepc, %0" : : "r"(mepc));
        return;
    }

    for (;;) {
        __asm__ volatile ("wfi");
    }
}

void kernel_main(unsigned long hartid, unsigned long dtb) {
    kernel_printf("Hello World from Arblem :)\n");

    kernel_printf("HartID: %x\n", hartid);
    kernel_printf("DTB: %x\n", dtb);

    kernel_timer_set();
    __asm__ volatile ("csrs mie, %0" : : "r"(1 << 7));
    __asm__ volatile ("csrs mstatus, %0" : : "r"(1 << 3));
}
