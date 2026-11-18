// Store to address 0, which is not mapped for U-mode. The store page fault
// makes the kernel end the program with exit status -1.

int main(void) {
    *(volatile char *)0 = 0;

    return 0;
}
