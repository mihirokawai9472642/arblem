#pragma once

// Run system call num (a7) with arguments from a0-a2. Returns the value for
// the program's a0: a result, or a negative -ERR_* code.
long syscall_handle(unsigned long num, unsigned long arg0, unsigned long arg1,
                    unsigned long arg2);
