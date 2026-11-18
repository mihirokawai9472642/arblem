# Arblem

Just a hobby operating system :)

Arblem is a small kernel for 64-bit RISC-V, written from scratch in C and
assembly. It boots on the QEMU `virt` machine through OpenSBI, runs user
programs in U-mode in their own address spaces, and gives them a tiny shell.

```
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
- System calls: `read`, `write`, `exit`, `spawn`, `wait`, `ticks`, `pages`, `poweroff`
- Per-process file descriptors, with the console as stdin, stdout and stderr
- A smoke test that boots the system in QEMU and checks the shell's output

## Environment
- Clang 19
- LLD 19
- LLVM 19
- LLDB 19
- QEMU 10 (with OpenSBI 1.6)
- GNU Make 4.4

Install on Debian 13 Trixie:

```sh
sudo apt install clang lld llvm lldb make qemu-system-riscv
```

## Usage

| Command      | Description                             |
|--------------|-----------------------------------------|
| `make`       | Build the kernel and user programs      |
| `make run`   | Boot the kernel on OpenSBI              |
| `make test`  | Run the smoke test                      |
| `make clean` | Remove `build/`                         |

To quit QEMU, type `poweroff` in the shell, or press `Ctrl-A` and then `X`.

### Shell

| Command    | Description                              |
|------------|------------------------------------------|
| `help`     | Show the list of commands                |
| `pages`    | Show the number of free physical pages   |
| `ticks`    | Show the number of timer ticks since boot |
| `poweroff` | Power off the machine                    |
| `hello`    | Print a message from U-mode              |
| `count`    | Count from 0 to 4 slowly                 |
| `fault`    | Write to address 0 and get killed        |

## Structure

| Directory   | Contents                                                  |
|-------------|-----------------------------------------------------------|
| `kernel/`   | S-mode kernel and its private headers                     |
| `user/`     | U-mode programs and their system call wrappers            |
| `lib/`      | Code linked into both the kernel and user programs        |
| `inc/`      | Headers shared by more than one program                   |
| `test/`     | Smoke test run by `make test`                             |
| `docs/`     | Design documentation                                      |

## Documentation

The [docs](docs/README.md) explain how the kernel works, in reading order:

- [Architecture](docs/architecture.md): privilege levels, components and design rules
- [Boot](docs/boot.md): from QEMU reset to the shell prompt
- [Memory](docs/memory.md): page allocator, Sv39 page tables and address spaces
- [Traps](docs/traps.md): trap entry and return, interrupts and exceptions
- [Threads](docs/threads.md): context switch, scheduling, sleep/wakeup, exit/wait
- [Processes](docs/processes.md): loading ELF programs, `spawn` and `wait`
- [System calls](docs/syscalls.md): calling convention and reference
- [Devices](docs/devices.md): UART, PLIC, timer and SBI
- [User programs](docs/user.md): runtime, shell, and adding a program
- [Build and test](docs/build-and-test.md): Makefile, smoke test and debugging
- [Known issues](docs/known-issues.md): bugs and limits of the current design
