// Print the contents of each file argument (or stdin when given no arguments)
// to stdout.

#include "sys.h"

static int cat_fd(int fd) {
    char buf[64] = {0};

    for (;;) {
        long n = sys_read(fd, buf, sizeof(buf));

        if (n < 0) {
            return -1;
        }

        if (n == 0) {
            return 0;
        }

        if (sys_write(FD_STDOUT, buf, (unsigned long)n) != n) {
            return -1;
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return cat_fd(FD_STDIN) == 0 ? 0 : 1;
    }

    for (int i = 1; i < argc; i++) {
        long fd = sys_open(argv[i], O_RDONLY);

        if (fd < 0) {
            sys_puts("cat: cannot open ");
            sys_puts(argv[i]);
            sys_putc('\n');

            return 1;
        }

        int err = cat_fd((int)fd);

        sys_close((int)fd);

        if (err < 0) {
            sys_puts("cat: read failed\n");

            return 1;
        }
    }

    return 0;
}
