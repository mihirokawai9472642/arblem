# Architecture

Arblem is a small teaching kernel for 64-bit RISC-V. It runs on the QEMU `virt`
machine, with multi-hart SMP (2 harts by default), and boots through OpenSBI.

## Privilege levels

RISC-V has three privilege levels, and Arblem uses all of them:

```
+-----------------------------------------------------------+
| U-mode   user programs: the shell and what it starts      |
|          own address space, reach the kernel with ecall   |
+-----------------------------------------------------------+
| S-mode   Arblem kernel                                    |
|          traps, threads, page tables, drivers, syscalls   |
+-----------------------------------------------------------+
| M-mode   OpenSBI (QEMU's built-in firmware)               |
|          timer CSR delegation, console, hart start, halt   |
+-----------------------------------------------------------+
```

- **M-mode** runs OpenSBI. The kernel calls it through the SBI interface
  (`ecall` from S-mode) to print kernel messages, start secondary harts, and
  power off the machine. See [Devices](devices.md#sbi).
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
    open, unlink,          |
    mkdir, spawn...)       v
     |      |              v
     |   file.h/.c    thread.c / thread.S  scheduling, sleep/wakeup
     |   console.c         |
     |   pipe.c            |
     |   uart.c            |
     |   kfs.c / bcache.c  |
     |   virtio_blk.c      v
     v                 vm.c  page.c        address spaces, pages
   proc.c  elf.c  (ELF loaded from disk)
```

| Component    | Role                                                                  |
|--------------|-----------------------------------------------------------------------|
| `boot.S`     | `_start` and `_start_secondary`: set up stacks, clear `.bss`, install `stvec` |
| `kernel.c`   | Initialize subsystems, start secondary harts (`kernel_secondary`), and run `kernel_init` to mount the FS and spawn the shell |
| `cpu.c`      | Per-hart `struct cpu` array, pointed to by `tp` while in S-mode       |
| `spinlock.c` | Atomic spinlocks with nested per-hart interrupt disabling             |
| `sleeplock.c`| Sleeping mutual-exclusion lock for I/O paths that may sleep           |
| `fdt.c`      | Parse the device tree for CPUs, RAM, timer frequency, and MMIO devices |
| `page.c`     | Hand out and take back 4 KiB physical pages                           |
| `vm.c`       | Build Sv39 page tables and copy data to and from user memory          |
| `trap.S/.c`  | Save registers on a trap, dispatch it, and return with `sret`         |
| `thread.S/.c`| Thread table, context switch, round-robin scheduler, sleep/wakeup     |
| `proc.c`     | Load an ELF file from the filesystem into a new address space and start a thread in it |
| `syscall.c`  | Decode system call numbers and arguments                              |
| `file.c`     | Reference counting and closing for `struct file`                      |
| `console.c`  | The console `struct file`, backed by the UART                         |
| `pipe.c`     | One-page ring-buffer pipes (`struct file` read/write ends)            |
| `bcache.c`   | Fixed-size 4 KiB LRU block cache over the VirtIO block device         |
| `kfs.c`      | Simple block filesystem: inodes, directories, paths, `struct fs_file` |
| `uart.c`     | 16550 UART driver with receive interrupts                             |
| `virtio_blk.c` | Modern VirtIO MMIO v2 block device driver                           |
| `plic.c`     | Platform-Level Interrupt Controller: per-hart context `2 * hartid + 1`, enable, claim, complete |
| `timer.c`    | Per-hart periodic timer interrupt (`stimecmp` CSR); boot hart counts ticks |
| `sbi.c`      | Calls into OpenSBI (Base, SRST, DBCN, HSM)                            |

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

1. **Multi-hart SMP with spinlocks and sleeplocks.** Each hart keeps its
   `struct cpu *` in `tp` while in S-mode. Shared in-memory state and fast
   critical sections are guarded by `struct spinlock` (which disables local
   interrupts with nesting depth tracked in `cpu->lock_depth`); longer paths
   that sleep across disk I/O (`kernel/kfs.c`) use `struct sleeplock`. Lock
   ordering is always subsystem lock -> `sched_lock`, and no thread may sleep
   or enter the scheduler while holding a spinlock other than `sched_lock`
   (`thread_sched` checks `lock_depth == 1`).
2. **The kernel runs with interrupts off.** When a trap enters S-mode the hart
   clears `sstatus.SIE`, and the kernel never sets it again while handling the
   trap. Interrupts arrive only while a thread is in U-mode or while
   `thread_idle` waits in `wfi`, and spinlocks enforce nested `push_off`/`pop_off`
   interrupt disabling. Across harts, lost wakeups are prevented by
   `thread_sleep(chan, lk)`, which acquires `sched_lock` before releasing `lk`.
3. **Kernel memory is identity-mapped.** Every page table maps physical RAM and
   the devices at the same virtual addresses, without `PTE_U`. A kernel
   pointer is also a physical address, which is what SBI calls, VirtIO DMA
   descriptors, and page table entries need.
4. **User memory lives in one 1 GiB slot.** Every user mapping is between
   `0x40000000` and `0x80000000`, which is exactly one entry of the top-level
   page table. The kernel's entries are shared by all page tables, and the
   user entry belongs to one process (see [Memory](memory.md)).
5. **Write xor execute.** No page, kernel or user, is both writable and
   executable.
6. **Errors are negative numbers.** Kernel functions that can fail return
   `-ERR_*` from `inc/error.h`, and system calls pass them to user programs
   unchanged.
