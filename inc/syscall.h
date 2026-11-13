#pragma once

#define SYSCALL_WRITE 1
#define SYSCALL_EXIT  2
#define SYSCALL_READ  3
#define SYSCALL_TICKS 4
#define SYSCALL_PAGES 5
#define SYSCALL_SPAWN 6
#define SYSCALL_WAIT  7

unsigned long syscall_handle(unsigned long num, unsigned long arg0, unsigned long arg1);
