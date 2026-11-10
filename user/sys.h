#pragma once

#include "syscall.h"

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
    unsigned long len = 0;

    while (s[len]) {
        len++;
    }

    sys_write(s, len);
}

static inline void sys_exit(void) {
    sys_call(SYSCALL_EXIT, 0, 0);
}
