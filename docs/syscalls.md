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
| `ERR_NOENT`  | 2     | No such program or file                         |
| `ERR_IO`     | 5     | Block I/O error (no disk, out-of-range sector, or device error) |
| `ERR_2BIG`   | 7     | Argument list too long                          |
| `ERR_NOEXEC` | 8     | The program is not a valid executable           |
| `ERR_BADF`   | 9     | Bad file descriptor                             |
| `ERR_CHILD`  | 10    | Not a child of the caller                       |
| `ERR_AGAIN`  | 11    | Thread table is full                            |
| `ERR_NOMEM`  | 12    | Out of physical pages                           |
| `ERR_FAULT`  | 14    | A pointer argument is not valid user memory     |
| `ERR_EXIST`  | 17    | File or directory already exists                |
| `ERR_NOTDIR` | 20    | Path component is not a directory               |
| `ERR_ISDIR`  | 21    | Path refers to a directory                      |
| `ERR_INVAL`  | 22    | Invalid argument, flags, or filename            |
| `ERR_MFILE`  | 24    | Thread's or filesystem's open file table is full |
| `ERR_NOSPC`  | 28    | No free disk blocks, inodes, or file space left |
| `ERR_PIPE`   | 32    | Pipe write with no reader left                  |
| `ERR_NOSYS`  | 38    | Unknown system call number                      |
| `ERR_NOTEMPTY` | 39  | Directory is not empty                          |

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

### 6 `spawn(name, len, argv)`

Starts the ELF program at path `name` in the filesystem (`len` bytes, not
NUL-terminated) as a child of the caller, with a copy of the caller's file
table. `argv` is either 0 (passes `name` alone as `argv[0]`) or a
NULL-terminated array of up to `PROC_ARG_MAX` (16) NUL-terminated user strings
totaling at most `PROC_ARG_BYTES` (256) bytes including NULs. Returns the
child's PID.

- `-ERR_NOENT` if no program has that name, or `len` is 64 or more.
- `-ERR_FAULT` if `name`, `argv`, or any argument string is not readable user
  memory.
- `-ERR_2BIG` if `argv` has more than 16 entries, a string/array exceeds the
  256-byte buffer, or the arguments do not fit on the initial stack page.
- `-ERR_NOEXEC` if `name` is not a regular file or fails the ELF loader's
  checks.
- `-ERR_NOMEM` if there are not enough free pages.
- `-ERR_AGAIN` if the thread table is full.

Wrappers: `sys_spawn`, `sys_spawnv`.

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

### 9 `close(fd)`

Closes file descriptor `fd` and drops its reference to the underlying
`struct file`. Returns 0.

- `-ERR_BADF` if `fd` is out of range or unused.

Wrapper: `sys_close`.

### 10 `dup(fd)`

Installs a new reference to the file at `fd` into the lowest free file
descriptor and returns the new descriptor.

- `-ERR_BADF` if `fd` is out of range or unused.
- `-ERR_MFILE` if all `THREAD_FILE_MAX` descriptors are in use.

Wrapper: `sys_dup`.

### 11 `pipe(fds)`

Creates a unidirectional in-memory pipe (512-byte ring buffer in one physical
page) and writes the read descriptor to `fds[0]` and the write descriptor to
`fds[1]`. Returns 0.

- Reading blocks while the buffer is empty and a write end is open, then returns
  available bytes (or 0 on EOF once all write ends close).
- Writing blocks while the buffer is full and a read end is open, and returns
  `-ERR_PIPE` once all read ends close.
- `-ERR_NOMEM` if no physical page is free.
- `-ERR_MFILE` if fewer than two file descriptors are free.
- `-ERR_FAULT` if `fds` is not writable user memory.

Wrapper: `sys_pipe`.

### 12 `sbrk(delta)`

Moves the calling program's heap break by `delta` bytes and returns the previous
break (a user address).

- Growing (`delta > 0`) advances the break lazily without allocating physical
  pages; pages are demand-paged on first access (or during syscall copy checks).
- Shrinking (`delta < 0`) unmaps and frees every page wholly above the new break.
- `sbrk(0)` returns the current break.
- `-ERR_NOMEM` if growing past `PROC_HEAP_LIMIT` (`0x7ffef000`) or shrinking
  below `heap_base`.

Wrapper: `sys_sbrk`.

### 13 `blk_read(sector, buf)`

Reads one 512-byte sector at index `sector` from the VirtIO block device into a
kernel bounce buffer and copies it to `buf`. Returns 0.

- `-ERR_IO` if no block device is present, `sector` is out of range, or the
  device reports an error.
- `-ERR_FAULT` if the 512-byte range at `buf` is not writable user memory.

Wrapper: `sys_blk_read`.

### 14 `blk_write(sector, buf)`

Copies 512 bytes from `buf` into a kernel bounce buffer and writes them to sector
index `sector` on the VirtIO block device. Returns 0.

- `-ERR_FAULT` if the 512-byte range at `buf` is not readable user memory (checked
  before touching the device).
- `-ERR_IO` if no block device is present, `sector` is out of range, or the
  device reports an error.

Wrapper: `sys_blk_write`.

### 15 `open(path, flags)`

Opens a file or directory at NUL-terminated user string `path` (up to 128 bytes
including NUL) and returns a new file descriptor. `flags` combines one access
mode (`O_RDONLY`, `O_WRONLY`, `O_RDWR`) with optional `O_CREAT` and `O_TRUNC`
from `inc/fs.h`.

- Directories may be opened with `O_RDONLY` (returning 32-byte `struct
  fs_dirent` records on `read`).
- `-ERR_FAULT` / `-ERR_2BIG` if `path` is invalid or >= 128 bytes.
- `-ERR_INVAL` if `flags` are unknown, `O_TRUNC` is passed without write access,
  or a path component exceeds 27 characters.
- `-ERR_NOENT` if `path` does not exist and `O_CREAT` is clear (or a parent
  directory is missing).
- `-ERR_NOTDIR` if an intermediate path component is not a directory.
- `-ERR_ISDIR` if `path` is a directory and write access or `O_TRUNC` is
  requested.
- `-ERR_MFILE` if the thread's FD table or the filesystem's open file table is
  full.
- `-ERR_NOSPC` if creating a file runs out of inodes or disk blocks.

Wrapper: `sys_open`.

### 16 `unlink(path)`

Removes the file or empty directory at `path` and frees its inode and blocks.
Returns 0.

- `-ERR_FAULT` / `-ERR_2BIG` on a bad or overlong path string.
- `-ERR_INVAL` on `"/"`, `"."`, `".."`, or overlong component names.
- `-ERR_NOENT` or `-ERR_NOTDIR` if `path` or a parent component is missing.
- `-ERR_NOTEMPTY` if `path` is a non-empty directory.

Wrapper: `sys_unlink`.

### 17 `mkdir(path)`

Creates a new empty directory at `path` (initialized with `.` and `..`).
Returns 0.

- `-ERR_FAULT` / `-ERR_2BIG` on a bad or overlong path string.
- `-ERR_EXIST` if `path` already exists.
- `-ERR_NOENT`, `-ERR_NOTDIR`, `-ERR_INVAL`, or `-ERR_NOSPC` on missing parents,
  invalid names, or a full disk/inode table.

Wrapper: `sys_mkdir`.

## Adding a system call

1. Add a number to `inc/sysnum.h`.
2. Handle it in `syscall_handle` in `kernel/syscall.c`. Copy any pointer
   arguments with `vm_copy_from_user` and `vm_copy_to_user`; never dereference
   them directly.
3. Add a wrapper to `user/sys.h`.
4. Document it on this page.
