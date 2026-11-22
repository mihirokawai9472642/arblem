# Arblem documentation

These pages describe how Arblem works today: what each part of the kernel does,
the conventions the parts rely on, and the limits of the current design. The
top-level [README](../README.md) covers starting the development container and
running the system.

## Reading order

If you are new to the code, read the pages in this order. Each one builds on
the ones before it.

| Page                                    | Covers                                                        |
|-----------------------------------------|---------------------------------------------------------------|
| [Architecture](architecture.md)         | The big picture: privilege levels, components, design rules   |
| [Boot](boot.md)                         | From QEMU reset to the first `>` prompt                       |
| [Memory](memory.md)                     | Physical page allocator, Sv39 page tables, address spaces     |
| [Traps](traps.md)                       | Trap entry and return, interrupts, exceptions                 |
| [Threads](threads.md)                   | Thread table, context switch, scheduling, sleep and wakeup    |
| [Processes](processes.md)               | ELF programs loaded from disk, `spawn` and `wait`             |
| [System calls](syscalls.md)             | Calling convention, error codes, and every system call        |
| [Devices](devices.md)                   | Device tree, UART, PLIC, timer, VirtIO-blk, and the SBI calls the kernel uses |
| [Filesystem](filesystem.md)             | Block cache, on-disk layout, `mkfs`, files and directories    |
| [User programs](user.md)                | The user runtime, the shell, and adding a new program         |
| [Build and test](build-and-test.md)     | Makefile, smoke test, and debugging with QEMU and LLDB        |
| [Known issues](known-issues.md)         | Bugs and limits of the current design                         |

## Source map

| Area            | Files                                                                     |
|-----------------|---------------------------------------------------------------------------|
| Boot            | `kernel/boot.S`, `kernel/kernel.c`, `kernel/linker.ld`                    |
| Memory          | `kernel/page.c`, `kernel/vm.c`, `kernel/riscv.h`                          |
| Traps           | `kernel/trap.S`, `kernel/trap.c`                                          |
| Threads         | `kernel/thread.S`, `kernel/thread.c`                                      |
| Processes       | `kernel/proc.c`, `kernel/elf.c`                                           |
| System calls    | `kernel/syscall.c`, `inc/sysnum.h`, `inc/error.h`, `user/sys.h`           |
| Files           | `kernel/file.h`, `kernel/file.c`, `kernel/console.c`, `kernel/pipe.c`, `inc/fd.h` |
| Filesystem      | `kernel/bcache.c`, `kernel/kfs.c`, `inc/fs.h`, `tools/mkfs.c`             |
| Devices         | `kernel/fdt.c`, `kernel/uart.c`, `kernel/plic.c`, `kernel/timer.c`, `kernel/virtio_blk.c` |
| Firmware calls  | `kernel/sbi.c`, `kernel/kprintf.c`, `kernel/panic.c`                      |
| User programs   | `user/*.c`, `user/linker.ld`                                              |
| Shared library  | `lib/str.c`, `inc/str.h`                                                  |

## Keeping these pages current

The documentation describes the code at the commit it was last updated in. When
a change makes a page wrong, update the page in the same change.
