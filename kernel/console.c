// The console file: stdin, stdout and stderr of user programs, backed by the
// UART driver.

#include "console.h"
#include "error.h"
#include "file.h"
#include "uart.h"
#include "vm.h"

// Bytes copied from user memory per chunk in console_write.
#define CONSOLE_BUF_SIZE 64

// Block until a key arrives and return that one byte. There is no line
// buffering; the shell edits lines itself.
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

// Copy the user buffer through a small kernel buffer and send it to the UART.
// On a bad address, chunks before it have already been written.
static long console_write(struct file *f, unsigned long buf,
                          unsigned long len) {
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
