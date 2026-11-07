void kernel_main(void) {
    volatile char *uart = (volatile char *)0x10000000;
    const char *str = "Hello World from Arblem :)\n";

    while (*str) {
        *uart = *str++;
    }
}
