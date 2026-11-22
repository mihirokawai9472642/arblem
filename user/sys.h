#pragma once

// System call wrappers for user programs. See docs/syscalls.md.

#include "fd.h"
#include "fs.h"
#include "str.h"
#include "sysnum.h"

// Make system call num with up to three arguments. Returns the kernel's a0:
// a result, or a negative -ERR_* code from error.h.
static inline long sys_call(long num, long arg0, long arg1, long arg2) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a7 __asm__("a7") = num;

    __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");

    return a0;
}

// Write len bytes of buf to fd. Returns the number of bytes written.
static inline long sys_write(int fd, const char *buf, unsigned long len) {
    return sys_call(SYSCALL_WRITE, fd, (long)buf, (long)len);
}

// Write one character to stdout.
static inline void sys_putc(char c) {
    sys_write(FD_STDOUT, &c, 1);
}

// Write a NUL-terminated string to stdout.
static inline void sys_puts(const char *s) {
    sys_write(FD_STDOUT, s, str_len(s));
}

// End the program with exit status code. Does not return.
static inline void sys_exit(int code) {
    sys_call(SYSCALL_EXIT, code, 0, 0);
}

// Read up to len bytes from fd into buf. The console returns one byte per call.
static inline long sys_read(int fd, char *buf, unsigned long len) {
    return sys_call(SYSCALL_READ, fd, (long)buf, (long)len);
}

// Read one character from stdin, waiting for it.
static inline char sys_getc(void) {
    char c = 0;

    sys_read(FD_STDIN, &c, 1);

    return c;
}

// Timer ticks since boot, 100 per second.
static inline unsigned long sys_ticks(void) {
    return sys_call(SYSCALL_TICKS, 0, 0, 0);
}

// Free physical pages.
static inline unsigned long sys_pages(void) {
    return sys_call(SYSCALL_PAGES, 0, 0, 0);
}

// Start the program called name (len bytes, no NUL needed) with a
// NULL-terminated argv array (or 0 to pass name alone). Returns its PID.
static inline long sys_spawnv(const char *name, unsigned long len,
                              const char *const *argv) {
    return sys_call(SYSCALL_SPAWN, (long)name, (long)len, (long)argv);
}

// Start the program called name with no extra arguments. Returns its PID.
static inline long sys_spawn(const char *name, unsigned long len) {
    return sys_spawnv(name, len, 0);
}

// Wait for child pid to exit and store its exit status in *status, unless
// status is 0. Returns 0.
static inline long sys_wait(long pid, int *status) {
    return sys_call(SYSCALL_WAIT, pid, (long)status, 0);
}

// Power off the machine. Returns only on failure.
static inline long sys_poweroff(void) {
    return sys_call(SYSCALL_POWEROFF, 0, 0, 0);
}

// Close file descriptor fd. Returns 0, or -ERR_BADF.
static inline long sys_close(int fd) {
    return sys_call(SYSCALL_CLOSE, fd, 0, 0);
}

// Duplicate fd into the lowest free file descriptor. Returns the new fd,
// or -ERR_BADF / -ERR_MFILE.
static inline long sys_dup(int fd) {
    return sys_call(SYSCALL_DUP, fd, 0, 0);
}

// Create a pipe and store its read and write fds in fds[0] and fds[1].
// Returns 0, or a negative -ERR_* code.
static inline long sys_pipe(int fds[2]) {
    return sys_call(SYSCALL_PIPE, (long)fds, 0, 0);
}

// Grow or shrink the user heap by delta bytes. Returns the previous program
// break (a user address), or -ERR_NOMEM on failure.
static inline long sys_sbrk(long delta) {
    return sys_call(SYSCALL_SBRK, delta, 0, 0);
}

// Read one 512-byte disk sector into buf. Returns 0, -ERR_IO, or -ERR_FAULT.
static inline long sys_blk_read(unsigned long sector, void *buf) {
    return sys_call(SYSCALL_BLK_READ, (long)sector, (long)buf, 0);
}

// Write one 512-byte disk sector from buf. Returns 0, -ERR_IO, or -ERR_FAULT.
static inline long sys_blk_write(unsigned long sector, const void *buf) {
    return sys_call(SYSCALL_BLK_WRITE, (long)sector, (long)buf, 0);
}

// Open path with flags from fs.h (O_RDONLY, O_WRONLY, O_RDWR, O_CREAT,
// O_TRUNC). Returns a new file descriptor, or a negative -ERR_* code.
static inline long sys_open(const char *path, unsigned int flags) {
    return sys_call(SYSCALL_OPEN, (long)path, (long)flags, 0);
}

// Remove the file or empty directory at path. Returns 0, or a negative -ERR_*.
static inline long sys_unlink(const char *path) {
    return sys_call(SYSCALL_UNLINK, (long)path, 0, 0);
}

// Create a directory at path. Returns 0, or a negative -ERR_* code.
static inline long sys_mkdir(const char *path) {
    return sys_call(SYSCALL_MKDIR, (long)path, 0, 0);
}
