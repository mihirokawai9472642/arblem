# User programs

User programs live in `user/`. Each one is a freestanding C program: there is
no C library, only the system call wrappers in `user/sys.h` and the string
helpers in `lib/`.

## Runtime

| File              | Role                                                         |
|-------------------|--------------------------------------------------------------|
| `user/start.c`    | `_start(argc, argv)`, the ELF entry point: calls `main(argc, argv)` and passes its return value to `exit` |
| `user/sys.h`      | Inline wrappers for every system call                        |
| `user/linker.ld`  | Places the program at `0x40000000`, one segment per page-aligned section |
| `lib/str.c`       | `str_equal` and `str_len`, linked into the kernel and every program |

When `main` runs, `argc` and `argv` hold the argument list on the stack below
`0x80000000`, aligned to 16 bytes. The stack starts with one mapped page (4 KiB)
and grows automatically on demand up to 64 KiB, with an unmapped 4 KiB guard
page below it. Programs can grow or shrink a demand-paged heap with `sys_sbrk`.
File descriptors 0, 1 and 2 are the console, inherited from the shell.

## The programs

| Program | What it does                                                  |
|---------|---------------------------------------------------------------|
| `shell` | Reads a line, runs a built-in command or spawns a program, and waits for it |
| `hello` | Prints `Hello World from U-mode!`                             |
| `count` | Prints `count: 0` to `count: 4` with a busy loop between lines, to show preemption |
| `fault` | Writes to address 0, which page-faults; the kernel ends it with status -1 |
| `orphan` | Spawns `hello` and exits without waiting for it, leaving `hello` an orphan; the smoke test uses it to check that orphans are freed |
| `regcheck` | Sets `gp` and `tp`, starts `regclobber`, and prints `regs: ok` if its own values survived and `regclobber` started with them cleared; used by the smoke test |
| `regclobber` | Helper for `regcheck`: keeps overwriting `gp` and `tp`, and exits with status 1 if they did not start cleared |
| `pipecheck` | Checks `close`, `dup`, and `pipe` (errors, rollback, wrap-around, EOF, child stdout) and prints `pipe: ok`; used by the smoke test |
| `upper` | Copies stdin to stdout, converting ASCII lowercase letters to uppercase |
| `echo` | Prints its command-line arguments (`argv[1..]`) separated by spaces |
| `memcheck` | Checks `sbrk`, lazy heap allocation, syscall demand paging, stack growth, and the stack guard page, printing `mem: ok`; used by the smoke test |
| `blkcheck` | Checks VirtIO-blk sector read/write round-trip, sector isolation, out-of-bounds `-ERR_IO`, bad-pointer `-ERR_FAULT`, leaves a persistence signature in sector 0, and prints `blk: ok`; used by the smoke test |
| `ls` | Lists directory entries (default `/`) by reading `struct fs_dirent` records |
| `cat` | Copies each file argument (or stdin if none) to stdout |
| `rm` | Removes one or more files or empty directories using `sys_unlink` |
| `mkdir` | Creates one or more directories using `sys_mkdir` |
| `fscheck` | Checks file R/W, `O_TRUNC`, 5000-byte multi-block I/O, directories, `unlink`, and error codes, leaving `/persist` on disk and printing `fs: ok`; used by the smoke test |
| `smpcheck` | Spawns four parallel workers exercising demand-paged `sbrk`, concurrent FS operations, shared pipe I/O, per-process `tp` preservation, and boot-hart timer ticks, printing `smp: ok`; used by the smoke test |

## The shell

The kernel starts the shell at boot with the console as stdin, stdout and
stderr. The shell does its own line editing: it echoes each character, handles
Backspace (`0x7f` or `\b`), and ends the line on Enter (`\r` or `\n`). Lines are
at most 63 characters, split on spaces into up to 16 arguments (`SHELL_ARG_MAX`)
for both single commands and pipelines.

| Input      | Action                                                      |
|------------|-------------------------------------------------------------|
| `help`     | List the commands                                           |
| `pages`    | Print the number of free physical pages in hex              |
| `ticks`    | Print the timer tick count in hex                           |
| `poweroff` | Power off the machine                                       |
| `cmd1 \| cmd2` | Connect `cmd1`'s stdout to `cmd2`'s stdin with a pipe   |
| anything else | Spawn the program with its arguments and wait for it     |

If a program exits with a non-zero status, the shell prints `exit: <status>`.
If it cannot start the program, it prints the reason, such as
`Unknown command`.

## Adding a program

1. Write `user/<name>.c` with `int main(void)` or `int main(int argc, char **argv)`.
   Include `sys.h` for system calls.
2. Add `<name>` to `USER_PROGS` in the `Makefile`. `build/mkfs` packs
   `build/user/<name>.elf` into `/` on the disk image automatically; no kernel
   file needs editing.
3. Optionally, mention it in `user/shell.c`'s `help` text and in
   `test/smoke.sh`.

The program name must fit inside a directory entry (`FS_NAME_MAX - 1` = 27
characters) and be shorter than `SYSCALL_NAME_MAX` (64 characters).
