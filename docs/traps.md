# Traps

A *trap* is any transfer of control into the kernel: an interrupt (timer,
UART, VirtIO-blk), a system call (`ecall` from U-mode), or an exception (page fault,
illegal instruction, breakpoint). All of them enter the kernel at
`trap_entry` in `kernel/trap.S`, because `boot.S` writes that address into
`stvec` in direct mode.

## What the hardware does

When a trap is taken into S-mode, the hart:

1. Saves the interrupted `pc` in `sepc`.
2. Writes the cause to `scause` (bit 63 set for interrupts) and extra
   information, such as the faulting address, to `stval`.
3. Saves the previous privilege level in `sstatus.SPP` (0 = U, 1 = S) and the
   previous `sstatus.SIE` in `sstatus.SPIE`, then clears `SIE`.
4. Jumps to `stvec`.

The hart does **not** switch stacks. `sp` still holds whatever the interrupted
code had in it, which might be a user address. `trap_entry` has to find a
kernel stack on its own.

`sret` undoes this: it jumps to `sepc`, sets the privilege level from `SPP`,
and restores `SIE` from `SPIE`.

## The `tp` and `sscratch` convention

In S-mode, `tp` points to the current hart's `struct cpu` (`&cpus[hartid]`),
set by `cpu_init`. `sscratch` is a CSR that the hardware never touches, and
Arblem uses it together with `tp` to find both the hart state and the thread's
kernel stack:

| Where the hart is | `tp`               | `sscratch`         | `cpu->kernel_sp`                          |
|-------------------|--------------------|--------------------|-------------------------------------------|
| S-mode            | `&cpus[hartid]`    | 0                  | (unused)                                  |
| U-mode            | User program `tp`  | `&cpus[hartid]`    | Top of the current thread's kernel stack  |

`trap_entry` starts with `csrrw tp, sscratch, tp`, which swaps `tp` and
`sscratch`:

- **From U-mode**, `tp` becomes `&cpus[hartid]` (non-zero), and `sscratch` now
  holds the user `tp`. The entry code stashes user `sp` in `cpu->user_sp`, loads
  the thread's kernel stack from `cpu->kernel_sp`, pushes the frame, saves user
  `sp` and user `tp`, and clears `sscratch` to 0.
- **From S-mode**, `tp` becomes 0. The code swaps `sscratch` back into `tp`
  (clearing `sscratch` to 0) and pushes the frame onto the current kernel stack.

On the way out:

- **Returning to U-mode (`sstatus.SPP == 0`)**: the thread may have migrated to
  another hart during `trap_handler`, so `trap_entry` writes
  `sp + TRAP_FRAME_SIZE` into `CPU_KERNEL_SP(tp)` on the *current* hart, puts
  `tp` (`&cpus[hartid]`) into `sscratch`, and restores the user `tp` from the
  frame before popping the remaining registers and running `sret`.
- **Returning to S-mode (`sstatus.SPP == 1`)**: `trap_entry` skips updating
  `sscratch` and skips loading `tp` from the frame, so `tp` stays bound to the
  current physical hart even across thread migration.

Kernel code entering U-mode for the first time (`trap_enter_user`) sets
`CPU_KERNEL_SP(tp)` and `sscratch` in the same way.

## The trap frame

`trap_entry` pushes a 160-byte frame (`TRAP_FRAME_SIZE` in `kernel/trap.h`) on
the kernel stack. `struct trap_frame` in `kernel/trap.c` describes the same
layout:

| Offset | Registers                  |
|--------|----------------------------|
| 0      | `ra`                       |
| 8-56   | `t0`-`t6`                  |
| 64-120 | `a0`-`a7`                  |
| 128    | `sp` of the interrupted code |
| 136    | `gp`                       |
| 144    | `tp`                       |

The frame holds the caller-saved registers, plus `gp` and `tp`.
`trap_handler` is a normal C function, so it preserves `s0`-`s11` itself, and
`thread_switch` saves them when it switches threads.

`gp` (global pointer) and `tp` (thread pointer) are neither caller-saved nor
callee-saved: the ABI fixes them for the whole program, and no function
touches them. For user traps, `gp` and `tp` still hold the user program's
values; saving them in the frame on the thread's kernel stack lets each program
keep its own values across preemption and hart migration. In S-mode, `tp`
always points to the current physical hart's `struct cpu`, so S-mode trap
returns never overwrite `tp` from the frame.

The frame is 19 registers, rounded up to keep the stack 16-byte aligned.

The system call path changes the saved `a0` in the frame, so the program sees
the return value in `a0` after `sret`.

## Dispatch (`trap_handler` in `kernel/trap.c`)

`trap_handler` first reads `scause`, `sepc`, `sstatus` and `stval` into local
variables. These CSRs belong to the hart, not to the thread: if this trap
switches to another thread, which later traps too, the CSRs get overwritten.
Keeping them on this thread's kernel stack, and writing `sepc` and `sstatus`
back just before returning, makes `thread_yield` safe inside a trap.

| Cause                                 | Action                                                          |
|---------------------------------------|-----------------------------------------------------------------|
| Supervisor timer interrupt            | `timer_interrupt()`, then `thread_yield()`                      |
| Supervisor external interrupt         | Claim the IRQ from the PLIC; if non-zero, call `uart_interrupt()` for `uart_irq()` or `virtio_blk_interrupt()` for `virtio_blk_irq()`, then complete the IRQ; finally `thread_yield()` |
| `ecall` from U-mode (cause 8)         | `syscall_handle(a7, a0, a1, a2)`, store the result in the frame's `a0`, and return to `sepc + 4` |
| Load or store page fault from U-mode (cause 13 or 15) | Call `thread_fault(tval)`. If it maps a heap or stack page, resume at `sepc` to retry the instruction; otherwise print the trap and end the program with `thread_exit(-1)` |
| Breakpoint from S-mode                | Print the trap, skip the `ebreak` (2 or 4 bytes), and continue  |
| Any other exception from U-mode       | Print the trap and end the program with `thread_exit(-1)`       |
| Any other exception from S-mode       | Print the trap and `panic`                                      |

Instruction page faults (`EXC_INSN_PAGE_FAULT`, cause 12) are never
demand-paged, keeping the heap and stack non-executable (W^X).

The breakpoint rule lets you drop an `ebreak` into kernel code as a cheap "I got
here" marker. The instruction length check reads the low two bits of the
instruction: `11` means a 32-bit instruction, anything else a 16-bit compressed
one.

## Interrupts and preemption

Every interrupt ends with `thread_yield()`, so the 100 Hz timer gives each
runnable thread a time slice of at most 10 ms. Since the kernel runs with
`sstatus.SIE` off (see [Architecture](architecture.md#design-rules)),
interrupts arrive only while a thread is in U-mode or while `thread_idle` is
waiting in `wfi`. A thread is never preempted while it handles a trap.

S-mode interrupts are always enabled while the hart is in U-mode, whatever
`SIE` says. That is why `trap_enter_user` can clear `SIE` before `sret` without
blocking interrupts in the user program.

## Entering U-mode for the first time

`trap_enter_user(pc, sp, argc, argv)` in `kernel/trap.S` starts a user program
as if it were returning from a trap:

1. Clear `SIE`, so no trap can arrive while `sscratch` is non-zero in S-mode.
2. Clear `SPP`, so `sret` goes to U-mode.
3. Set `sepc` to the program's entry point.
4. Save the current kernel `sp` in `CPU_KERNEL_SP(tp)` and write `tp`
   (`&cpus[hartid]`) to `sscratch`. This sets the kernel stack and per-hart CPU
   pointer for the thread's future traps from U-mode.
5. Load the user stack pointer, `a0 = argc`, and `a1 = argv`.
6. Clear all 28 other integer registers, so the program does not start with
   kernel or previous-thread values left in them.
7. `sret`.
