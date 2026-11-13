#pragma once

#include "str.h"
#include "sysnum.h"

static inline long sys_call(long num, long arg0, long arg1) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a7 __asm__("a7") = num;

    __asm__ volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");

    return a0;
}

static inline long sys_write(const char *buf, unsigned long len) {
    return sys_call(SYSCALL_WRITE, (long)buf, len);
}

static inline void sys_putc(char c) {
    sys_write(&c, 1);
}

static inline void sys_puts(const char *s) {
    sys_write(s, str_len(s));
}

static inline void sys_exit(void) {
    sys_call(SYSCALL_EXIT, 0, 0);
}

static inline long sys_read(char *buf, unsigned long len) {
    return sys_call(SYSCALL_READ, (long)buf, len);
}

static inline char sys_getc(void) {
    char c;

    sys_read(&c, 1);

    return c;
}

static inline unsigned long sys_ticks(void) {
    return sys_call(SYSCALL_TICKS, 0, 0);
}

static inline unsigned long sys_pages(void) {
    return sys_call(SYSCALL_PAGES, 0, 0);
}

static inline long sys_spawn(const char *name, unsigned long len) {
    return sys_call(SYSCALL_SPAWN, (long)name, len);
}

static inline long sys_wait(long pid) {
    return sys_call(SYSCALL_WAIT, pid, 0);
}
