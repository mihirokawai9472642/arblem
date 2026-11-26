# Arblem

Just a hobby operating system :)

Arblem is a small kernel for 64-bit RISC-V, written from scratch in C17 and
assembly. It boots on the QEMU `virt` machine through OpenSBI, runs user
programs in U-mode in separate Sv39 address spaces, and provides a tiny shell.
The cross compiler, QEMU, and the other tools run in a container, so the only
host requirements are Docker Engine (with the Compose plugin) and, for editing,
Zed with the RISC-V Assembly extension (its tasks and its `clangd` and
`riscv-asm-lsp` language servers use the container).

```text
Hello World from Arblem :)
HartID: 0x0000000000000000
...
> hello
Hello World from U-mode!
> fault
Trap!
scause: 0x000000000000000f
...
exit: -1
> poweroff
```

## Features

- S-mode kernel on OpenSBI, using SBI for kernel messages, secondary hart startup (HSM), and shutdown
- Multi-hart SMP (`-smp 2`), with per-hart `struct cpu`, atomic spinlocks, sleeplocks, and lost-wakeup-free sleep/wakeup
- Device tree (DTB) parsing (`kernel/fdt.c`) for CPUs, RAM, timer frequency, and MMIO device discovery
- Sv39 paging, with a separate address space per program, W^X mappings, and demand-paged heap and stack (up to 64 KiB with a guard page)
- Preemptive round-robin threads across harts, driven by per-hart 100 Hz timers
- Interrupt-driven UART input and Modern VirtIO-blk (MMIO v2) disk I/O with DMA through the PLIC
- 4 KiB LRU block cache (`kernel/bcache.c`) and simple block filesystem (`kernel/kfs.c`, `tools/mkfs.c`)
- User programs loaded from ELF files on the VirtIO-blk filesystem, with `argc`/`argv` argument passing
- System calls: `read`, `write`, `open`, `close`, `dup`, `pipe`, `unlink`, `mkdir`, `exit`, `spawn`, `wait`, `ticks`, `pages`, `sbrk`, `blk_read`, `blk_write`, `poweroff`
- Per-process reference-counted file descriptors for the console, pipes, and filesystem files/directories
- A smoke test that boots two harts in QEMU and checks parallel SMP execution, shell output, and disk persistence

## Getting started

The development container (`Dockerfile` and `compose.yaml`, based on Debian 13
Trixie) provides Clang 19, LLD 19, LLVM 19, LLDB 19, `clangd` 19,
`clang-format` 19, `clang-tidy` 19, QEMU 10 (with OpenSBI 1.6), GNU Make 4.4,
and `riscv-asm-lsp` 0.3.0 (a language server for the assembly sources, pinned
in the `Dockerfile`).

Run these from the project root:

1. Build and start the container (Zed task: `up`):

   ```sh
   docker compose up -d --build
   ```

2. Build the kernel and boot it in QEMU (Zed task: `run`). Quit QEMU by typing
   `poweroff` in the shell, or pressing `Ctrl-A` and then `X`:

   ```sh
   docker compose exec dev make run
   ```

3. Run the smoke test (Zed task: `test`):

   ```sh
   docker compose exec dev make test
   ```

## Commands

Each target runs in the container as `docker compose exec dev make <target>`,
and most targets have a matching Zed task (`build` for the default target).

| Target         | What it does                                                                         |
| -------------- | ------------------------------------------------------------------------------------ |
| (none)         | Builds `build/kernel.elf`, the user programs, `build/mkfs`, `build/disk.img`, and `build/compile_commands.json` |
| `run`          | Boots the kernel on OpenSBI in QEMU, with the serial console in the terminal         |
| `test`         | Runs the smoke test in QEMU                                                          |
| `debug`        | Boots halted at the first instruction, waiting for LLDB on port 1234                 |
| `lldb`         | Starts LLDB attached to the waiting QEMU instance (from a second terminal)           |
| `format`       | Formats C sources and headers (`kernel/`, `lib/`, `inc/`, `user/`, `tools/`) with `clang-format` |
| `format-check` | Checks that C sources and headers match `.clang-format` without modifying files      |
| `tidy`         | Checks C sources (`kernel/`, `lib/`, `user/`) with `clang-tidy`                      |
| `clean`        | Deletes `build/`                                                                     |

Pass `WERROR=1` to `make` or `make tidy` to treat warnings as errors (used in CI).

To debug, run the `debug` task (or `docker compose exec dev make debug`), then
the `lldb` task, which opens in a new terminal. The Zed task `shell` opens a
shell in the container. Use the host (WSL) terminal for `git`: commit signing
and SSH are not available in the container.

### Shell

| Command    | Description                                  |
| ---------- | -------------------------------------------- |
| `help`     | Show the list of commands                    |
| `pages`    | Show the number of free physical pages       |
| `ticks`    | Show the number of timer ticks since boot    |
| `poweroff` | Power off the machine                        |
| `hello`    | Print a message from U-mode                  |
| `count`    | Count from 0 to 4 slowly                     |
| `fault`    | Write to address 0 and get killed            |
| `orphan`   | Start `hello` and exit without waiting for it |
| `regcheck`     | Check that `gp` and `tp` are kept per program        |
| `pipecheck`    | Check `close`, `dup`, and `pipe`                     |
| `upper`        | Convert stdin to uppercase on stdout                 |
| `echo`         | Print command-line arguments                         |
| `memcheck`     | Check `sbrk`, demand paging, stack growth, and guard |
| `blkcheck`     | Check VirtIO-blk sector read/write and error handling |
| `ls`           | List directory entries                               |
| `cat`          | Print file contents or copy stdin to stdout          |
| `rm`           | Remove files or empty directories                    |
| `mkdir`        | Create directories                                   |
| `fscheck`      | Check filesystem R/W, directories, and persistence   |
| `smpcheck`     | Stress-test parallel execution across harts          |
| `cmd1 \| cmd2` | Connect `cmd1`'s stdout to `cmd2`'s stdin with a pipe |

## Layout

| Path                           | Contents                                                 |
| ------------------------------ | -------------------------------------------------------- |
| `kernel/`                      | S-mode kernel, assembly, linker script, private headers  |
| `user/`                        | U-mode programs, runtime, syscall wrappers, linker script |
| `lib/`                         | Code linked into both the kernel and user programs       |
| `inc/`                         | Headers shared across `kernel/`, `user/`, and `lib/`     |
| `tools/`                       | Host build utilities (`tools/mkfs.c`)                    |
| `test/`                        | Smoke test (`test/smoke.sh`) run by `make test`          |
| `docs/`                        | Design documentation                                     |
| `build/`                       | Everything generated (not in git)                        |
| `Makefile`                     | Target, build settings, QEMU options, and targets        |
| `Dockerfile`, `compose.yaml`   | Development container                                    |
| `.clang-format`, `.clang-tidy` | Formatting and static analysis settings                  |
| `.zed/`                        | Zed tasks and container language server configuration    |
| `.github/workflows/`           | Continuous integration workflow (`ci.yml`)               |
| `AGENTS.md`                    | Instructions for coding agents                           |

## Documentation

The [docs](docs/README.md) explain how the kernel works, in reading order:

- [Architecture](docs/architecture.md): privilege levels, components, and design rules
- [Boot](docs/boot.md): from QEMU reset to the shell prompt
- [Memory](docs/memory.md): page allocator, Sv39 page tables, and address spaces
- [Traps](docs/traps.md): trap entry and return, interrupts, and exceptions
- [Threads](docs/threads.md): context switch, scheduling, sleep/wakeup, exit/wait
- [Processes](docs/processes.md): loading ELF programs, `spawn`, and `wait`
- [System calls](docs/syscalls.md): calling convention and reference
- [Devices](docs/devices.md): device tree, UART, PLIC, timer, VirtIO-blk, and SBI
- [Filesystem](docs/filesystem.md): block cache, on-disk layout, `mkfs`, files, and directories
- [User programs](docs/user.md): runtime, shell, and adding a program
- [Build and test](docs/build-and-test.md): Makefile, smoke test, and debugging
- [Known issues](docs/known-issues.md): bugs and limits of the current design

## License

Arblem is licensed under the [MIT License](LICENSE.md).
