# Build and test

## Toolchain

Arblem builds inside the Debian 13 (Trixie) development container defined by
`Dockerfile` and `compose.yaml`, using the LLVM toolchain and QEMU without a
separate RISC-V cross-compiler package (`clang --target=riscv64-unknown-elf`
targets RISC-V directly). See the [README](../README.md#getting-started) for tool
versions and container startup.

| Flag                           | Why                                                                 |
|--------------------------------|---------------------------------------------------------------------|
| `--target=riscv64-unknown-elf` | Bare-metal RISC-V, no operating system                              |
| `-march=rv64gc -mabi=lp64d`    | RV64GC instruction set with the 64-bit hard-float LP64D ABI         |
| `-mcmodel=medany`              | Address globals relative to `pc`, so code and data can be linked anywhere (`0x80200000` for the kernel, `0x40000000` for programs) |
| `-std=c17 -ffreestanding`      | C17 without a hosted C library (`CFLAGS` only; the compiler must not assume libc) |
| `-g`                           | Emit DWARF debug information for LLDB and source-level disassembly  |
| `-Iinc`                        | Shared headers                                                      |
| `-MMD -MP`                     | Write dependency files so header changes trigger rebuilds           |
| `-MJ $@.json`                  | Write a compilation database fragment next to each object file      |

## Makefile

All output goes to `build/`, mirroring the source tree: `kernel/trap.S`
becomes `build/kernel/trap.S.o`.

```
lib/*.c ──────────────┬──────────────────────────────┐
                      v                              v
user/start.c + user/<name>.c ──> build/user/<name>.elf
                                       │ .incbin
                                       v
kernel/*.c, kernel/*.S ──────────> build/kernel.elf
```

- `build/kernel/user.S.o` depends on every user ELF file, because
  `kernel/user.S` includes them with `.incbin`. Changing a user program
  rebuilds the kernel image.
- Each user ELF file is `start.c.o`, the program's own object, and the `lib/`
  objects, linked with `user/linker.ld`.
- `build/compile_commands.json` joins the `.o.json` fragments emitted for
  `KERNEL_OBJS`, `LIB_OBJS` and `USER_OBJS`, so `clangd` and `clang-tidy`
  read the exact compiler flags from the `Makefile`.
- `MAKEFLAGS += --no-builtin-rules` turns off make's implicit rules, so only
  the rules written in the Makefile apply.

Run each target in the container as `docker compose exec dev make <target>`,
or spawn the matching Zed task (`build` for the default target):

| Target   | Does                                                                                |
|----------|-------------------------------------------------------------------------------------|
| (none)   | Build `build/kernel.elf`, the user programs, and `build/compile_commands.json`      |
| `run`    | Build, then boot in QEMU                                                            |
| `test`   | Build, then run the smoke test                                                      |
| `debug`  | Build, then boot halted at the first instruction waiting for LLDB on port 1234      |
| `lldb`   | Attach LLDB to the waiting QEMU instance (from a second terminal)                   |
| `format` | Format C sources and headers (`kernel/`, `lib/`, `inc/`, `user/`) with `clang-format` |
| `tidy`   | Run `clang-tidy` on all C sources using `build/compile_commands.json`               |
| `clean`  | Delete `build/`                                                                     |

## Formatting and static analysis

C formatting and static checks are configured in `.clang-format` and
`.clang-tidy`:

- `make format` formats C sources and headers in `kernel/`, `lib/`, `inc/` and
  `user/`. Assembly files (`.S`) are not covered.
- `make tidy` runs `clang-tidy` (`clang-analyzer-*` and `bugprone-*`, with
  `bugprone-easily-swappable-parameters` and `bugprone-reserved-identifier`
  disabled) across `kernel/*.c`, `lib/*.c` and `user/*.c`. `.zed/settings.json`
  runs `clangd` inside the container, so the same diagnostics appear in Zed.

## Smoke test (`test/smoke.sh`)

The smoke test boots the real kernel in QEMU and types into the shell:

1. Creates a named pipe, `build/test.in`, for QEMU's standard input, and
   logs all output to `build/test.log`.
2. Waits for the `> ` prompt.
3. Types `pages`, `hello`, `pages`, `orphan` seven times, `regcheck`,
   `count`, `pages`, `fault`, `pages`, `foo`, `pages`, `poweroff`, each ending
   in `\r` as a terminal would send it. `count` takes about a second, which gives the
   orphans' children time to finish and be freed before the next `pages`.
4. Waits for QEMU to exit.

It then checks the log:

| Check     | Passes when                                              |
|-----------|----------------------------------------------------------|
| `boot`    | The kernel banner appears                                |
| `hello`   | `hello` printed its message                              |
| `count`   | `count` reached `count: 4`                               |
| `fault`   | The shell printed `exit: -1` for `fault`                 |
| `unknown` | `foo` was reported as `Unknown command`                  |
| `regs`    | `regcheck` printed `regs: ok`: `gp` and `tp` are saved per program, and a new program starts with them cleared |
| `orphan`  | `hello` ran 8 times: once alone, and once from each `orphan`, so orphans are freed and the thread table never fills up |
| `pages`   | All five `pages` outputs are identical, so nothing leaked |
| `poweroff`| QEMU exited with status 0                                |

The test fails at once if the log contains `panic: `, or if an expected line
does not appear within 20 seconds.

## Debugging

### Reading a trap report

When a program faults, or the kernel hits a breakpoint, the kernel prints
`scause`, `sepc` and `stval`. To find the instruction at `sepc`, disassemble the
matching file inside the container:

```sh
docker compose exec dev llvm-objdump -d build/kernel.elf        # sepc >= 0x80200000
docker compose exec dev llvm-objdump -d build/user/fault.elf    # sepc around 0x40000000
```

Common `scause` values are listed in `kernel/riscv.h` (for example 13 = load
page fault, 15 = store page fault). For page faults, `stval` is the address
that was accessed.

### Breakpoints with LLDB

QEMU has a GDB server that LLDB can connect to. Start QEMU paused, waiting for
a debugger on port 1234 (Zed task: `debug`):

```sh
docker compose exec dev make debug
```

Then, in another terminal, attach LLDB (Zed task: `lldb`):

```sh
docker compose exec dev make lldb
```

Debug information (`-g`) is enabled by default in `CFLAGS` and `ASFLAGS`, so
LLDB can step through C source lines right away.

To quit QEMU, type `poweroff` in the shell, or press `Ctrl-A` and then `X`.
