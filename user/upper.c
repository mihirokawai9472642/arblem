// Filter stdin to stdout, turning ASCII lowercase letters into uppercase.

#include "sys.h"

int main(void) {
    char buf[64] = {0};

    for (;;) {
        long n = sys_read(FD_STDIN, buf, sizeof(buf));

        if (n < 0) {
            return 1;
        }

        if (n == 0) {
            break;
        }

        for (long i = 0; i < n; i++) {
            if (buf[i] >= 'a' && buf[i] <= 'z') {
                buf[i] = (char)(buf[i] - ('a' - 'A'));
            }
        }

        if (sys_write(FD_STDOUT, buf, (unsigned long)n) < 0) {
            return 1;
        }
    }

    return 0;
}
