#pragma once

// Error codes, shared by the kernel and user programs. Kernel functions and
// system calls return them negated (-ERR_NOENT). The values match Linux.
// See docs/syscalls.md.

#define ERR_NOENT  2  // No such program
#define ERR_NOEXEC 8  // Not a valid executable
#define ERR_BADF   9  // Bad file descriptor
#define ERR_CHILD  10 // Not a child of the caller
#define ERR_AGAIN  11 // Thread table full
#define ERR_NOMEM  12 // Out of memory
#define ERR_FAULT  14 // Bad user address
#define ERR_NOSYS  38 // Unknown system call
