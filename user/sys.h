#pragma once

#include "fd.h"
#include "str.h"
#include "sysnum.h"

static inline long sys_call(long num, long arg0, long arg1, long arg2) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a7 __asm__("a7") = num;

    __asm__ volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");

    return a0;
}

static inline long sys_write(int fd, const char *buf, unsigned long len) {
    return sys_call(SYSCALL_WRITE, fd, (long)buf, len);
}

static inline void sys_putc(char c) {
    sys_write(FD_STDOUT, &c, 1);
}

static inline void sys_puts(const char *s) {
    sys_write(FD_STDOUT, s, str_len(s));
}

static inline void sys_exit(int code) {
    sys_call(SYSCALL_EXIT, code, 0, 0);
}

static inline long sys_read(int fd, char *buf, unsigned long len) {
    return sys_call(SYSCALL_READ, fd, (long)buf, len);
}

static inline char sys_getc(void) {
    char c;

    sys_read(FD_STDIN, &c, 1);

    return c;
}

static inline unsigned long sys_ticks(void) {
    return sys_call(SYSCALL_TICKS, 0, 0, 0);
}

static inline unsigned long sys_pages(void) {
    return sys_call(SYSCALL_PAGES, 0, 0, 0);
}

static inline long sys_spawn(const char *name, unsigned long len) {
    return sys_call(SYSCALL_SPAWN, (long)name, len, 0);
}

static inline long sys_wait(long pid, int *status) {
    return sys_call(SYSCALL_WAIT, pid, (long)status, 0);
}

static inline long sys_poweroff(void) {
    return sys_call(SYSCALL_POWEROFF, 0, 0, 0);
}
