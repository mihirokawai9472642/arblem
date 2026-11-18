# Architecture

Arblem is a small teaching kernel for 64-bit RISC-V. It runs on the QEMU `virt`
machine, on a single hart, and boots through OpenSBI.

## Privilege levels

RISC-V has three privilege levels, and Arblem uses all of them:

```
+-----------------------------------------------------------+
| U-mode   user programs: shell, hello, count, fault        |
|          own address space, reach the kernel with ecall   |
+-----------------------------------------------------------+
| S-mode   Arblem kernel                                    |
|          traps, threads, page tables, drivers, syscalls   |
+-----------------------------------------------------------+
| M-mode   OpenSBI (QEMU's built-in firmware)               |
|          timer CSR delegation, debug console, shutdown    |
+-----------------------------------------------------------+
```

- **M-mode** runs OpenSBI. The kernel calls it through the SBI interface
  (`ecall` from S-mode) to print kernel messages and to power off the machine.
  See [Devices](devices.md#sbi).
- **S-mode** runs the kernel. It owns `stvec`, `satp` and the S-mode interrupt
  sources (timer and external).
- **U-mode** runs user programs. A program can only touch pages mapped with
  `PTE_U` in its own page table, and reaches the kernel through system calls.

## Components

```
                       user programs (U-mode)
                 shell   hello   count   fault
                    \      |       |      /
                     ecall (a7 = number)
                              |
   +--------------------------v---------------------------+
   | trap.S / trap.c   trap entry, dispatch, return       |
   +------+----------------+-------------------+----------+
          |                |                   |
   syscall.c          timer.c / plic.c     user faults
   (read, write,      (interrupts)         -> thread_exit(-1)
    spawn, wait...)        |
     |      |              v
     |   file.h       thread.c / thread.S  scheduling, sleep/wakeup
     |   console.c         |
     |   uart.c            v
     v                 vm.c  page.c        address spaces, pages
   proc.c  elf.c  user.S                   (embedded user ELF files)
```

| Component    | Role                                                                  |
|--------------|-----------------------------------------------------------------------|
| `boot.S`     | First instructions: set up a stack, clear `.bss`, install `stvec`     |
| `kernel.c`   | Initialize subsystems in order and start the shell                    |
| `page.c`     | Hand out and take back 4 KiB physical pages                           |
| `vm.c`       | Build Sv39 page tables and copy data to and from user memory          |
| `trap.S/.c`  | Save registers on a trap, dispatch it, and return with `sret`         |
| `thread.S/.c`| Thread table, context switch, round-robin scheduler, sleep/wakeup     |
| `proc.c`     | Load an ELF image into a new address space and start a thread in it  |
| `syscall.c`  | Decode system call numbers and arguments                              |
| `console.c`  | The console `struct file`, backed by the UART                         |
| `uart.c`     | 16550 UART driver with receive interrupts                             |
| `plic.c`     | Platform-Level Interrupt Controller: enable, claim, complete          |
| `timer.c`    | Periodic timer interrupt through the `stimecmp` CSR                   |
| `sbi.c`      | Calls into OpenSBI                                                    |

## Threads and processes

There is one kind of schedulable object, the **thread** (`struct thread` in
`kernel/thread.c`). A thread that runs a user program also owns that program's
page table and file table, so in practice a thread *is* a process. A thread's ID
is its index in the thread table, and that ID is the PID that `spawn` returns.

Every thread has its own one-page kernel stack. The kernel switches threads only
by calling `thread_yield`, which saves callee-saved registers on the current
kernel stack and loads them from the next one (see [Threads](threads.md)).

## Design rules

These rules hold across the whole kernel. A lot of the code is simple *because*
of them, so changing one of them means revisiting many files.

1. **One hart.** The kernel never starts a second hart, and has no locks.
2. **The kernel runs with interrupts off.** When a trap enters S-mode the hart
   clears `sstatus.SIE`, and the kernel never sets it again while handling the
   trap. Interrupts are taken only while a thread is in U-mode, while the
   idle thread waits in `wfi`, or in the few instructions a new thread runs
   between `thread_entry` and `trap_enter_user`. So code that handles a trap
   never races an interrupt handler, and sleep/wakeup cannot lose a wakeup.
3. **Kernel memory is identity-mapped.** Every page table maps physical RAM and
   the devices at the same virtual addresses, without `PTE_U`. A kernel
   pointer is also a physical address, which is what SBI calls and page
   table entries need.
4. **User memory lives in one 1 GiB slot.** Every user mapping is between
   `0x40000000` and `0x80000000`, which is exactly one entry of the top-level
   page table. The kernel's entries are shared by all page tables, and the
   user entry belongs to one process (see [Memory](memory.md)).
5. **Write xor execute.** No page, kernel or user, is both writable and
   executable.
6. **Errors are negative numbers.** Kernel functions that can fail return
   `-ERR_*` from `inc/error.h`, and system calls pass them to user programs
   unchanged.
