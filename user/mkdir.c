// Create one or more directories using sys_mkdir.

#include "sys.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        sys_puts("usage: mkdir <path>...\n");

        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (sys_mkdir(argv[i]) != 0) {
            sys_puts("mkdir: cannot create ");
            sys_puts(argv[i]);
            sys_putc('\n');

            return 1;
        }
    }

    return 0;
}
