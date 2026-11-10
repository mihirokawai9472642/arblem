#include "syscall.h"
#include "thread.h"
#include "uart.h"
#include "vm.h"

#define SYSCALL_BUF_SIZE 64

static unsigned long syscall_write(unsigned long buf, unsigned long len) {
    char kbuf[SYSCALL_BUF_SIZE];

    for (unsigned long done = 0; done < len; done += SYSCALL_BUF_SIZE) {
        unsigned long n = len - done;

        if (n > SYSCALL_BUF_SIZE) {
            n = SYSCALL_BUF_SIZE;
        }

        if (vm_copy_from_user(kbuf, buf + done, n) != 0) {
            return -1;
        }

        for (unsigned long i = 0; i < n; i++) {
            uart_putc(kbuf[i]);
        }
    }

    return len;
}

unsigned long syscall_handle(unsigned long num, unsigned long arg0, unsigned long arg1) {
    if (num == SYSCALL_WRITE) {
        return syscall_write(arg0, arg1);
    }

    if (num == SYSCALL_EXIT) {
        thread_exit();
    }

    return -1;
}
