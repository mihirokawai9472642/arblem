// Print command-line arguments separated by single spaces, followed by a
// newline.

#include "sys.h"

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            sys_putc(' ');
        }

        sys_puts(argv[i]);
    }

    sys_putc('\n');

    return 0;
}
