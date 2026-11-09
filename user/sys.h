#pragma once

#include "syscall.h"

static inline long sys_call(long num, long arg) {
    register long a0 __asm__("a0") = arg;
    register long a7 __asm__("a7") = num;

    __asm__ volatile ("ecall" : "+r"(a0) : "r"(a7) : "memory");

    return a0;
}

static inline void sys_putc(char c) {
    sys_call(SYSCALL_PUTC, c);
}

static inline void sys_puts(const char *s) {
    while (*s) {
        sys_putc(*s++);
    }
}

static inline void sys_exit(void) {
    sys_call(SYSCALL_EXIT, 0);
}
