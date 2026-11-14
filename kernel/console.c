#include "console.h"
#include "error.h"
#include "file.h"
#include "uart.h"
#include "vm.h"

#define CONSOLE_BUF_SIZE 64

static long console_read(struct file *f, unsigned long buf, unsigned long len) {
    (void)f;

    if (len == 0) {
        return 0;
    }

    char c = uart_getc();

    if (vm_copy_to_user(buf, &c, 1) != 0) {
        return -ERR_FAULT;
    }

    return 1;
}

static long console_write(struct file *f, unsigned long buf, unsigned long len) {
    char kbuf[CONSOLE_BUF_SIZE];

    (void)f;

    for (unsigned long done = 0; done < len; done += CONSOLE_BUF_SIZE) {
        unsigned long n = len - done;

        if (n > CONSOLE_BUF_SIZE) {
            n = CONSOLE_BUF_SIZE;
        }

        if (vm_copy_from_user(kbuf, buf + done, n) != 0) {
            return -ERR_FAULT;
        }

        for (unsigned long i = 0; i < n; i++) {
            uart_putc(kbuf[i]);
        }
    }

    return len;
}

static struct file console = {console_read, console_write};

struct file *console_file(void) {
    return &console;
}
