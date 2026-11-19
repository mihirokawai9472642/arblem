#pragma once

// System call numbers, passed in a7. Shared by kernel/syscall.c and
// user/sys.h. See docs/syscalls.md for arguments and return values.

#define SYSCALL_WRITE    1  // write(fd, buf, len)
#define SYSCALL_EXIT     2  // exit(code)
#define SYSCALL_READ     3  // read(fd, buf, len)
#define SYSCALL_TICKS    4  // ticks()
#define SYSCALL_PAGES    5  // pages()
#define SYSCALL_SPAWN    6  // spawn(name, len)
#define SYSCALL_WAIT     7  // wait(pid, status)
#define SYSCALL_POWEROFF 8  // poweroff()
#define SYSCALL_CLOSE    9  // close(fd)
#define SYSCALL_DUP      10 // dup(fd)
