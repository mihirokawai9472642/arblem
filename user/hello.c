#include "sys.h"

void user_main(void) {
    const char *s = "Hello World from U-mode!\n";

    while (*s) {
        sys_putc(*s++);
    }

    sys_exit();
}
