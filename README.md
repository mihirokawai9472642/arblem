# Arblem

Just a hobby operating system :)

Arblem is a small kernel for 64-bit RISC-V, written from scratch in C17 and
assembly. It boots on the QEMU `virt` machine through OpenSBI, runs user
programs in U-mode in separate Sv39 address spaces, and provides a tiny shell.
The cross compiler, QEMU, and the other tools run in a container, so the only
host requirements are Docker Engine (with the Compose plugin) and, for editing,
Zed (its tasks and `clangd` use the container).

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

- S-mode kernel on OpenSBI, using SBI for kernel messages and shutdown
- Sv39 paging, with a separate address space per program and W^X mappings
- Preemptive round-robin threads, driven by a 100 Hz timer
- Interrupt-driven UART input through the PLIC
- User programs loaded from ELF files embedded in the kernel image
- System calls: `read`, `write`, `close`, `dup`, `pipe`, `exit`, `spawn`, `wait`, `ticks`, `pages`, `poweroff`
- Per-process reference-counted file descriptors, with the console and pipes
- A smoke test that boots the system in QEMU and checks the shell's output

## Getting started

The development container (`Dockerfile` and `compose.yaml`, based on Debian 13
Trixie) provides Clang 19, LLD 19, LLVM 19, LLDB 19, `clangd` 19,
`clang-format` 19, `clang-tidy` 19, QEMU 10 (with OpenSBI 1.6), and GNU Make
4.4.

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
and the matching Zed task (`build` for the default target) does the same.

| Target   | What it does                                                                         |
| -------- | ------------------------------------------------------------------------------------ |
| (none)   | Builds `build/kernel.elf`, the user programs, and `build/compile_commands.json`      |
| `run`    | Boots the kernel on OpenSBI in QEMU, with the serial console in the terminal         |
| `test`   | Runs the smoke test in QEMU                                                          |
| `debug`  | Boots halted at the first instruction, waiting for LLDB on port 1234                 |
| `lldb`   | Starts LLDB attached to the waiting QEMU instance (from a second terminal)           |
| `format` | Formats C sources and headers (`kernel/`, `lib/`, `inc/`, `user/`) with `clang-format` |
| `tidy`   | Checks C sources (`kernel/`, `lib/`, `user/`) with `clang-tidy`                      |
| `clean`  | Deletes `build/`                                                                     |

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
| `cmd1 \| cmd2` | Connect `cmd1`'s stdout to `cmd2`'s stdin with a pipe |

## Layout

| Path                           | Contents                                                 |
| ------------------------------ | -------------------------------------------------------- |
| `kernel/`                      | S-mode kernel, assembly, linker script, private headers  |
| `user/`                        | U-mode programs, runtime, syscall wrappers, linker script |
| `lib/`                         | Code linked into both the kernel and user programs       |
| `inc/`                         | Headers shared across `kernel/`, `user/`, and `lib/`     |
| `test/`                        | Smoke test (`test/smoke.sh`) run by `make test`          |
| `docs/`                        | Design documentation                                     |
| `build/`                       | Everything generated (not in git)                        |
| `Makefile`                     | Target, build settings, QEMU options, and targets        |
| `Dockerfile`, `compose.yaml`   | Development container                                    |
| `.clang-format`, `.clang-tidy` | Formatting and static analysis settings                  |
| `.zed/`                        | Zed tasks and container `clangd` configuration           |
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
- [Devices](docs/devices.md): UART, PLIC, timer, and SBI
- [User programs](docs/user.md): runtime, shell, and adding a program
- [Build and test](docs/build-and-test.md): Makefile, smoke test, and debugging
- [Known issues](docs/known-issues.md): bugs and limits of the current design

## License

Arblem is licensed under the [MIT License](LICENSE.md).
