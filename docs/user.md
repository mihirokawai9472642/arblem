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
2. Add `<name>` to `USER_PROGS` in the `Makefile`.
3. In `kernel/user.S`, export `user_<name>` and `user_<name>_end`, and
   `.incbin "build/user/<name>.elf"` between them (copy an existing block).
4. In `kernel/proc.c`, declare the two symbols and add an entry to
   `proc_images[]`.
5. Optionally, mention it in the shell's `help` text and in the smoke test.

The name must be shorter than 64 characters, the limit of the `spawn` system
call (`SYSCALL_NAME_MAX`).
