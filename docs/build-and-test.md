# Build and test

## Toolchain

Arblem builds with the LLVM toolchain and needs no RISC-V cross compiler
package: `clang --target=riscv64-unknown-elf` compiles for RISC-V on any host.
See the [Readme](../Readme.md#environment) for versions and install commands.

| Flag                | Why                                                         |
|---------------------|-------------------------------------------------------------|
| `--target=riscv64-unknown-elf` | Bare-metal RV64GC, no operating system           |
| `-mcmodel=medany`   | Address globals relative to `pc`, so code and data can be linked anywhere (`0x80200000` for the kernel, `0x40000000` for programs) |
| `-ffreestanding`    | No hosted C library; the compiler must not assume one       |
| `-Iinc`             | Shared headers                                              |
| `-MMD -MP`          | Write dependency files so header changes trigger rebuilds   |

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
- `MAKEFLAGS += --no-builtin-rules` turns off make's implicit rules, so only
  the rules written in the Makefile apply.

| Target       | Does                                           |
|--------------|------------------------------------------------|
| `make`       | Build `build/kernel.elf` and the user programs |
| `make run`   | Build, then boot in QEMU                       |
| `make test`  | Build, then run the smoke test                 |
| `make clean` | Delete `build/`                                |

## Smoke test (`test/smoke.sh`)

The smoke test boots the real kernel in QEMU and types into the shell:

1. Creates a named pipe, `build/test.in`, for QEMU's standard input, and
   logs all output to `build/test.log`.
2. Waits for the `> ` prompt.
3. Types `pages`, `hello`, `pages`, `count`, `pages`, `fault`, `pages`,
   `foo`, `pages`, `poweroff`, each ending in `\r` as a terminal would send it.
4. Waits for QEMU to exit.

It then checks the log:

| Check     | Passes when                                              |
|-----------|----------------------------------------------------------|
| `boot`    | The kernel banner appears                                |
| `hello`   | `hello` printed its message                              |
| `count`   | `count` reached `count: 4`                               |
| `fault`   | The shell printed `exit: -1` for `fault`                 |
| `unknown` | `foo` was reported as `Unknown command`                  |
| `pages`   | All five `pages` outputs are identical, so nothing leaked |
| `poweroff`| QEMU exited with status 0                                |

The test fails at once if the log contains `panic: `, or if an expected line
does not appear within 20 seconds.

## Debugging

### Reading a trap report

When a program faults, or the kernel hits a breakpoint, the kernel prints
`scause`, `sepc` and `stval`. To find the instruction at `sepc`, disassemble the
matching file:

```sh
llvm-objdump -d build/kernel.elf        # sepc >= 0x80200000
llvm-objdump -d build/user/fault.elf    # sepc around 0x40000000
```

Common `scause` values are listed in `kernel/riscv.h` (for example 13 = load
page fault, 15 = store page fault). For page faults, `stval` is the address
that was accessed.

### Breakpoints with LLDB

QEMU has a GDB server that LLDB can connect to. Start QEMU paused, waiting for
a debugger on port 1234:

```sh
qemu-system-riscv64 -machine virt -nographic -bios default -kernel build/kernel.elf -s -S
```

Then, in another terminal:

```sh
lldb build/kernel.elf -o 'gdb-remote 1234'
```

The default build has no debug information. Add `-g` to `CFLAGS` to step
through C source lines.

To quit QEMU, type `poweroff` in the shell, or press `Ctrl-A` and then `X`.
