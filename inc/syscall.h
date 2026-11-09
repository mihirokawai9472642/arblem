#pragma once

#define SYSCALL_PUTC 1
#define SYSCALL_EXIT 2

unsigned long syscall_handle(unsigned long num, unsigned long arg);
