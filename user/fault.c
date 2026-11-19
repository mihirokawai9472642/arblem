// Store to address 0, which is not mapped for U-mode. The store page fault
// makes the kernel end the program with exit status -1.

int main(void) {
    // Suppress the null dereference check: triggering a page fault at
    // address 0 is the entire purpose of this test program.
    *(volatile char *)0 = 0; // NOLINT(clang-analyzer-core.NullDereference)

    return 0;
}
