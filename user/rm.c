// Remove one or more files or empty directories using sys_unlink.

#include "sys.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        sys_puts("usage: rm <path>...\n");

        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (sys_unlink(argv[i]) != 0) {
            sys_puts("rm: cannot remove ");
            sys_puts(argv[i]);
            sys_putc('\n');

            return 1;
        }
    }

    return 0;
}
