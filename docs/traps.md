# Traps

A *trap* is any transfer of control into the kernel: an interrupt (timer,
UART), a system call (`ecall` from U-mode), or an exception (page fault,
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

## The `sscratch` convention

`sscratch` is a CSR that the hardware never touches. Arblem uses it to tell
where a trap came from:

| Where the hart is | `sscratch` holds                                |
|-------------------|-------------------------------------------------|
| U-mode            | Kernel stack pointer for the current thread's next trap |
| S-mode            | 0                                               |

`trap_entry` starts with `csrrw sp, sscratch, sp`, which swaps the two:

- **From U-mode**, `sp` becomes the kernel stack, and `sscratch` now holds the
  user `sp`.
- **From S-mode**, `sp` becomes 0. The code sees the zero and reads
  `sscratch` back into `sp`, so it keeps using the current kernel stack.

Either way, the original `sp` is now in `sscratch`. The entry code saves it into
the trap frame and writes 0 into `sscratch`, because the hart is now in S-mode.
A trap taken while handling this one (a breakpoint, for example) then takes the
"from S-mode" path.

On the way out, if the trap is returning to U-mode, `trap_entry` pops the frame
off the kernel stack and writes the resulting `sp` back into `sscratch` before
restoring registers. That is the same value `sscratch` held on entry. Kernel
code that enters U-mode for the first time, `trap_enter_user`, sets `sscratch`
in the same way.

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
touches them. They still hold the user program's values when the trap arrives,
but if the trap switches threads, the next program overwrites them. Saving
them in the frame, which lives on this thread's kernel stack, gives every
program its own copy.

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
| Supervisor external interrupt         | Claim the IRQ from the PLIC, call `uart_interrupt()` if it is the UART, complete the IRQ, then `thread_yield()` |
| `ecall` from U-mode (cause 8)         | `syscall_handle(a7, a0, a1, a2)`, store the result in the frame's `a0`, and return to `sepc + 4` |
| Breakpoint from S-mode                | Print the trap, skip the `ebreak` (2 or 4 bytes), and continue  |
| Any other exception from U-mode       | Print the trap and end the program with `thread_exit(-1)`       |
| Any other exception from S-mode       | Print the trap and `panic`                                      |

The breakpoint rule lets you drop an `ebreak` into kernel code as a cheap "I got
here" marker. The instruction length check reads the low two bits of the
instruction: `11` means a 32-bit instruction, anything else a 16-bit compressed
one.

## Interrupts and preemption

Every interrupt ends with `thread_yield()`, so the 100 Hz timer gives each
runnable thread a time slice of at most 10 ms. Since the kernel runs with
`sstatus.SIE` off (see [Architecture](architecture.md#design-rules)),
interrupts arrive only while a thread is in U-mode, while the idle thread is in
`wfi`, or while a new thread runs its first few instructions. A thread is never
preempted while it handles a trap.

S-mode interrupts are always enabled while the hart is in U-mode, whatever
`SIE` says. That is why `trap_enter_user` can clear `SIE` before `sret` without
blocking interrupts in the user program.

## Entering U-mode for the first time

`trap_enter_user(pc, sp)` in `kernel/trap.S` starts a user program as if it were
returning from a trap:

1. Clear `SIE`, so no trap can arrive while `sscratch` is non-zero in S-mode.
2. Clear `SPP`, so `sret` goes to U-mode.
3. Set `sepc` to the program's entry point.
4. Put the current kernel `sp` in `sscratch`. This is the kernel stack for the
   thread's future traps.
5. Load the user stack pointer.
6. Clear `gp` and `tp`, so the program does not start with the values the
   previous program left in them.
7. `sret`.

The other registers are not cleared, so the program starts with whatever the
kernel left in them. See [Known issues](known-issues.md).
