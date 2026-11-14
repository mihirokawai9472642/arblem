#include "syscall.h"
#include "error.h"
#include "page.h"
#include "proc.h"
#include "sbi.h"
#include "sysnum.h"
#include "thread.h"
#include "timer.h"
#include "uart.h"
#include "vm.h"

#define SYSCALL_BUF_SIZE 64
#define SYSCALL_NAME_MAX 16

static long syscall_write(unsigned long buf, unsigned long len) {
    char kbuf[SYSCALL_BUF_SIZE];

    for (unsigned long done = 0; done < len; done += SYSCALL_BUF_SIZE) {
        unsigned long n = len - done;

        if (n > SYSCALL_BUF_SIZE) {
            n = SYSCALL_BUF_SIZE;
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

static long syscall_read(unsigned long buf, unsigned long len) {
    if (len == 0) {
        return 0;
    }

    char c = uart_getc();

    if (vm_copy_to_user(buf, &c, 1) != 0) {
        return -ERR_FAULT;
    }

    return 1;
}

static long syscall_spawn(unsigned long name, unsigned long len) {
    char kname[SYSCALL_NAME_MAX];

    if (len >= SYSCALL_NAME_MAX) {
        return -ERR_NOENT;
    }

    if (vm_copy_from_user(kname, name, len) != 0) {
        return -ERR_FAULT;
    }

    kname[len] = '\0';

    return proc_spawn(kname);
}

static long syscall_wait(unsigned long pid, unsigned long status) {
    int code;
    int err = thread_wait(pid, &code);

    if (err < 0) {
        return err;
    }

    if (status != 0 && vm_copy_to_user(status, &code, sizeof(code)) != 0) {
        return -ERR_FAULT;
    }

    return 0;
}

long syscall_handle(unsigned long num, unsigned long arg0, unsigned long arg1) {
    if (num == SYSCALL_WRITE) {
        return syscall_write(arg0, arg1);
    }

    if (num == SYSCALL_EXIT) {
        thread_exit(arg0);
    }

    if (num == SYSCALL_READ) {
        return syscall_read(arg0, arg1);
    }

    if (num == SYSCALL_TICKS) {
        return timer_ticks();
    }

    if (num == SYSCALL_PAGES) {
        return page_free_count();
    }

    if (num == SYSCALL_SPAWN) {
        return syscall_spawn(arg0, arg1);
    }

    if (num == SYSCALL_WAIT) {
        return syscall_wait(arg0, arg1);
    }

    if (num == SYSCALL_POWEROFF) {
        sbi_shutdown(SBI_SRST_REASON_NONE);
    }

    return -ERR_NOSYS;
}
