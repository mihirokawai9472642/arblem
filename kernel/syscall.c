// System call dispatch. trap_handler calls syscall_handle for every ecall
// from U-mode. User pointers are only accessed through vm_copy_from_user and
// vm_copy_to_user. See docs/syscalls.md.

#include "syscall.h"
#include "error.h"
#include "file.h"
#include "page.h"
#include "pipe.h"
#include "proc.h"
#include "sbi.h"
#include "sysnum.h"
#include "thread.h"
#include "timer.h"
#include "virtio_blk.h"
#include "vm.h"

// Size of the kernel buffer for a program name, including the NUL.
#define SYSCALL_NAME_MAX 64

static long syscall_write(unsigned long fd, unsigned long buf,
                          unsigned long len) {
    struct file *f = thread_file(fd);

    if (f == 0 || f->write == 0) {
        return -ERR_BADF;
    }

    return f->write(f, buf, len);
}

static long syscall_read(unsigned long fd, unsigned long buf,
                         unsigned long len) {
    struct file *f = thread_file(fd);

    if (f == 0 || f->read == 0) {
        return -ERR_BADF;
    }

    return f->read(f, buf, len);
}

static long syscall_close(unsigned long fd) {
    return thread_close_fd(fd);
}

static long syscall_dup(unsigned long fd) {
    struct file *f = thread_file(fd);

    if (f == 0) {
        return -ERR_BADF;
    }

    int newfd = thread_alloc_fd(f);

    if (newfd < 0) {
        return newfd;
    }

    file_ref(f);

    return newfd;
}

static long syscall_pipe(unsigned long fds_addr) {
    struct file *rf = 0;
    struct file *wf = 0;
    int err = pipe_create(&rf, &wf);

    if (err < 0) {
        return err;
    }

    int rfd = thread_alloc_fd(rf);

    if (rfd < 0) {
        file_close(rf);
        file_close(wf);

        return rfd;
    }

    int wfd = thread_alloc_fd(wf);

    if (wfd < 0) {
        thread_close_fd(rfd);
        file_close(wf);

        return wfd;
    }

    int fds[2] = {rfd, wfd};

    if (vm_copy_to_user(fds_addr, fds, sizeof(fds)) != 0) {
        thread_close_fd(rfd);
        thread_close_fd(wfd);

        return -ERR_FAULT;
    }

    return 0;
}

// name is not NUL-terminated in user memory; len gives its length.
static long syscall_spawn(unsigned long name, unsigned long len,
                          unsigned long argv_addr) {
    char kname[SYSCALL_NAME_MAX];

    if (len >= SYSCALL_NAME_MAX) {
        return -ERR_NOENT;
    }

    if (vm_copy_from_user(kname, name, len) != 0) {
        return -ERR_FAULT;
    }

    kname[len] = '\0';

    if (argv_addr == 0) {
        return proc_spawn(kname, 0, 0);
    }

    char kargs[PROC_ARG_BYTES];
    const char *kargv[PROC_ARG_MAX];
    int argc = 0;
    unsigned long used = 0;

    for (;;) {
        unsigned long uptr = 0;
        unsigned long elem = argv_addr + (unsigned long)argc * sizeof(uptr);

        if (vm_copy_from_user(&uptr, elem, sizeof(uptr)) != 0) {
            return -ERR_FAULT;
        }

        if (uptr == 0) {
            break;
        }

        if (argc == PROC_ARG_MAX) {
            return -ERR_2BIG;
        }

        int n =
            vm_copy_str_from_user(kargs + used, uptr, PROC_ARG_BYTES - used);

        if (n < 0) {
            return n;
        }

        kargv[argc] = kargs + used;
        used += (unsigned long)n + 1;
        argc++;
    }

    if (argc == 0) {
        return proc_spawn(kname, 0, 0);
    }

    return proc_spawn(kname, argc, kargv);
}

// status may be 0 when the caller does not want the exit status.
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

static long syscall_blk_read(unsigned long sector, unsigned long buf) {
    char kbuf[VIRTIO_BLK_SECTOR_SIZE];
    int err = virtio_blk_read(sector, kbuf);

    if (err < 0) {
        return err;
    }

    if (vm_copy_to_user(buf, kbuf, sizeof(kbuf)) != 0) {
        return -ERR_FAULT;
    }

    return 0;
}

static long syscall_blk_write(unsigned long sector, unsigned long buf) {
    char kbuf[VIRTIO_BLK_SECTOR_SIZE];

    if (vm_copy_from_user(kbuf, buf, sizeof(kbuf)) != 0) {
        return -ERR_FAULT;
    }

    return virtio_blk_write(sector, kbuf);
}

long syscall_handle(unsigned long num, unsigned long arg0, unsigned long arg1,
                    unsigned long arg2) {
    if (num == SYSCALL_WRITE) {
        return syscall_write(arg0, arg1, arg2);
    }

    // Does not return.
    if (num == SYSCALL_EXIT) {
        thread_exit((int)arg0);
    }

    if (num == SYSCALL_READ) {
        return syscall_read(arg0, arg1, arg2);
    }

    if (num == SYSCALL_TICKS) {
        return (long)timer_ticks();
    }

    if (num == SYSCALL_PAGES) {
        return (long)page_free_count();
    }

    if (num == SYSCALL_SPAWN) {
        return syscall_spawn(arg0, arg1, arg2);
    }

    if (num == SYSCALL_WAIT) {
        return syscall_wait(arg0, arg1);
    }

    // Returns only if the shutdown fails, and then falls through to
    // -ERR_NOSYS.
    if (num == SYSCALL_POWEROFF) {
        sbi_shutdown(SBI_SRST_REASON_NONE);
    }

    if (num == SYSCALL_CLOSE) {
        return syscall_close(arg0);
    }

    if (num == SYSCALL_DUP) {
        return syscall_dup(arg0);
    }

    if (num == SYSCALL_PIPE) {
        return syscall_pipe(arg0);
    }

    if (num == SYSCALL_SBRK) {
        return thread_sbrk((long)arg0);
    }

    if (num == SYSCALL_BLK_READ) {
        return syscall_blk_read(arg0, arg1);
    }

    if (num == SYSCALL_BLK_WRITE) {
        return syscall_blk_write(arg0, arg1);
    }

    return -ERR_NOSYS;
}
