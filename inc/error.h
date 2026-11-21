#pragma once

// Error codes, shared by the kernel and user programs. Kernel functions and
// system calls return them negated (-ERR_NOENT). The values match Linux.
// See docs/syscalls.md.

#define ERR_NOENT    2  // No such program or file
#define ERR_IO       5  // I/O error
#define ERR_2BIG     7  // Argument list too long
#define ERR_NOEXEC   8  // Not a valid executable
#define ERR_BADF     9  // Bad file descriptor
#define ERR_CHILD    10 // Not a child of the caller
#define ERR_AGAIN    11 // Thread table full
#define ERR_NOMEM    12 // Out of memory
#define ERR_FAULT    14 // Bad user address
#define ERR_EXIST    17 // File exists
#define ERR_NOTDIR   20 // Not a directory
#define ERR_ISDIR    21 // Is a directory
#define ERR_INVAL    22 // Invalid argument
#define ERR_MFILE    24 // Too many open files
#define ERR_NOSPC    28 // No space left on device
#define ERR_PIPE     32 // Broken pipe
#define ERR_NOSYS    38 // Unknown system call
#define ERR_NOTEMPTY 39 // Directory not empty
