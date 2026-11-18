# User programs

User programs live in `user/`. Each one is a freestanding C program: there is
no C library, only the system call wrappers in `user/sys.h` and the string
helpers in `lib/`.

## Runtime

| File              | Role                                                         |
|-------------------|--------------------------------------------------------------|
| `user/start.c`    | `_start`, the ELF entry point: calls `main` and passes its return value to `exit` |
| `user/sys.h`      | Inline wrappers for every system call                        |
| `user/linker.ld`  | Places the program at `0x40000000`, one segment per page-aligned section |
| `lib/str.c`       | `str_equal` and `str_len`, linked into the kernel and every program |

When `main` runs, the stack pointer is `0x80000000` and the stack is one page
(4 KiB). There is no heap, and `main` receives no arguments. File descriptors
0, 1 and 2 are the console, inherited from the shell.

## The programs

| Program | What it does                                                  |
|---------|---------------------------------------------------------------|
| `shell` | Reads a line, runs a built-in command or spawns a program, and waits for it |
| `hello` | Prints `Hello World from U-mode!`                             |
| `count` | Prints `count: 0` to `count: 4` with a busy loop between lines, to show preemption |
| `fault` | Writes to address 0, which page-faults; the kernel ends it with status -1 |

## The shell

The kernel starts the shell at boot with the console as stdin, stdout and
stderr. The shell does its own line editing: it echoes each character, handles
Backspace (`0x7f` or `\b`), and ends the line on Enter (`\r` or `\n`). Lines are
at most 63 characters.

| Input      | Action                                                      |
|------------|-------------------------------------------------------------|
| `help`     | List the commands                                           |
| `pages`    | Print the number of free physical pages in hex              |
| `ticks`    | Print the timer tick count in hex                           |
| `poweroff` | Power off the machine                                       |
| anything else | Spawn the program with that name and wait for it         |

If a program exits with a non-zero status, the shell prints `exit: <status>`.
If it cannot start the program, it prints the reason, such as
`Unknown command`.

## Adding a program

1. Write `user/<name>.c` with an `int main(void)`. Include `sys.h` for system
   calls.
2. Add `<name>` to `USER_PROGS` in the `Makefile`.
3. In `kernel/user.S`, export `user_<name>` and `user_<name>_end`, and
   `.incbin "build/user/<name>.elf"` between them (copy an existing block).
4. In `kernel/proc.c`, declare the two symbols and add an entry to
   `proc_images[]`.
5. Optionally, mention it in the shell's `help` text and in the smoke test.

The name must be shorter than 16 characters, the limit of the `spawn` system
call.
