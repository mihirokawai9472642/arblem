#pragma once

#define SYSCALL_WRITE 1
#define SYSCALL_EXIT  2

unsigned long syscall_handle(unsigned long num, unsigned long arg0, unsigned long arg1);
