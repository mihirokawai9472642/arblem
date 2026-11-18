# System calls

## Calling convention

A user program makes a system call with `ecall`:

| Register | Holds                          |
|----------|--------------------------------|
| `a7`     | System call number (`inc/sysnum.h`) |
| `a0`-`a2`| Up to three arguments          |
| `a0`     | Return value, after the call   |

All other registers keep their values. `user/sys.h` wraps each call in an
inline function; `sys_call` there is the only place that issues `ecall`.

On the kernel side, `trap_handler` passes the saved `a7`, `a0`, `a1`, `a2` to
`syscall_handle`, writes the result into the saved `a0`, and resumes the program
at the instruction after `ecall` (`sepc + 4`).

## Errors

A negative return value is an error code from `inc/error.h`, negated. The
numbers match Linux, which makes them easy to look up.

| Name         | Value | Meaning                                         |
|--------------|-------|-------------------------------------------------|
| `ERR_NOENT`  | 2     | No program with that name                       |
| `ERR_NOEXEC` | 8     | The program is not a valid executable           |
| `ERR_BADF`   | 9     | Bad file descriptor                             |
| `ERR_CHILD`  | 10    | Not a child of the caller                       |
| `ERR_AGAIN`  | 11    | Thread table is full                            |
| `ERR_NOMEM`  | 12    | Out of physical pages                           |
| `ERR_FAULT`  | 14    | A pointer argument is not valid user memory     |
| `ERR_NOSYS`  | 38    | Unknown system call number                      |

## Reference

Numbers are from `inc/sysnum.h`, wrappers from `user/sys.h`.

### 1 `write(fd, buf, len)`

Writes `len` bytes from `buf` to file descriptor `fd`. Returns the number of
bytes written.

- `-ERR_BADF` if `fd` is out of range, unused, or not writable.
- `-ERR_FAULT` if part of `buf` is not readable user memory. Bytes before the
  bad page may already have been written.

Wrappers: `sys_write`, `sys_putc`, `sys_puts`.

### 2 `exit(code)`

Ends the calling program with exit status `code`. Does not return. The program
stays a zombie until its parent calls `wait`.

Wrapper: `sys_exit`.

### 3 `read(fd, buf, len)`

Reads up to `len` bytes from `fd` into `buf`. Returns the number of bytes read.

On the console, `read` blocks until a key is pressed and then returns exactly
one byte, or 0 if `len` is 0. There is no line editing in the kernel; the shell
does that itself.

- `-ERR_BADF` if `fd` is out of range, unused, or not readable.
- `-ERR_FAULT` if `buf` is not writable user memory.

Wrappers: `sys_read`, `sys_getc`.

### 4 `ticks()`

Returns the number of timer interrupts since boot. The timer runs at 100 Hz.

Wrapper: `sys_ticks`.

### 5 `pages()`

Returns the number of free physical pages.

Wrapper: `sys_pages`.

### 6 `spawn(name, len)`

Starts the embedded program called `name` (`len` bytes, not NUL-terminated) as
a child of the caller, with a copy of the caller's file table. Returns the
child's PID.

- `-ERR_NOENT` if no program has that name, or `len` is 16 or more.
- `-ERR_FAULT` if `name` is not readable user memory.
- `-ERR_NOEXEC` if the embedded ELF file fails the loader's checks.
- `-ERR_NOMEM` if there are not enough free pages.
- `-ERR_AGAIN` if the thread table is full.

Wrapper: `sys_spawn`.

### 7 `wait(pid, status)`

Blocks until child `pid` exits, frees it, and stores its exit status at
`status` unless `status` is 0. Returns 0.

- `-ERR_CHILD` if `pid` is not a live child of the caller.
- `-ERR_FAULT` if `status` is not writable user memory. The child has already
  been freed by then, so its status is lost.

Wrapper: `sys_wait`.

### 8 `poweroff()`

Asks OpenSBI to shut the machine down. Does not return if the shutdown works.
If OpenSBI refuses, the call falls through and returns `-ERR_NOSYS`.

Wrapper: `sys_poweroff`.

## Adding a system call

1. Add a number to `inc/sysnum.h`.
2. Handle it in `syscall_handle` in `kernel/syscall.c`. Copy any pointer
   arguments with `vm_copy_from_user` and `vm_copy_to_user`; never dereference
   them directly.
3. Add a wrapper to `user/sys.h`.
4. Document it on this page.
