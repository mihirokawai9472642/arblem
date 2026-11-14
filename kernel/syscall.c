#include "syscall.h"
#include "error.h"
#include "file.h"
#include "page.h"
#include "proc.h"
#include "sbi.h"
#include "sysnum.h"
#include "thread.h"
#include "timer.h"
#include "vm.h"

#define SYSCALL_NAME_MAX 16

static long syscall_write(unsigned long fd, unsigned long buf, unsigned long len) {
    struct file *f = thread_file(fd);

    if (f == 0 || f->write == 0) {
        return -ERR_BADF;
    }

    return f->write(f, buf, len);
}

static long syscall_read(unsigned long fd, unsigned long buf, unsigned long len) {
    struct file *f = thread_file(fd);

    if (f == 0 || f->read == 0) {
        return -ERR_BADF;
    }

    return f->read(f, buf, len);
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

long syscall_handle(unsigned long num, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    if (num == SYSCALL_WRITE) {
        return syscall_write(arg0, arg1, arg2);
    }

    if (num == SYSCALL_EXIT) {
        thread_exit(arg0);
    }

    if (num == SYSCALL_READ) {
        return syscall_read(arg0, arg1, arg2);
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
