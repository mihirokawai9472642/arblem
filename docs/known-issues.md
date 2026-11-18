# Known issues

Bugs and limits of the current design, roughly from most to least important.
Remove an entry when it is fixed.

## Bugs

### `gp` and `tp` leak between processes

`trap_entry` saves only `ra`, `t0`-`t6`, `a0`-`a7` and `sp`, and
`thread_switch` saves only `ra` and `s0`-`s11`. `gp` and `tp` are saved by
neither, so when the scheduler switches from one user program to another, the
second program sees the first one's values and keeps whatever it writes. A
test program that sets `tp`, spins while another program writes `tp`, and then
reads it back, finds it changed.

No current program uses `gp` or `tp`, but thread-local storage or linker gp
relaxation would break, and the values leak information between processes.

Code: `kernel/trap.S`, `kernel/thread.S`.

### The end of RAM is guessed from the DTB address

`kernel_main` passes the device tree address to `page_init` and `vm_init` as the
end of usable memory. This works because OpenSBI happens to place the DTB near
the top of RAM, but nothing guarantees it. The memory above the DTB is wasted,
and the DTB itself is not mapped once paging is on, so it cannot be read later.

Code: `kernel_main` in `kernel/kernel.c`.

## Limits

### Kernel

- **One hart, no locks.** The kernel relies on running with interrupts off and
  on a single hart. See [Architecture](architecture.md#design-rules).
- **No kernel stack guard.** Each kernel stack is one page, with ordinary
  memory on both sides. An overflow silently corrupts the neighboring page.
- **Fixed-size tables.** At most 8 threads, including the idle thread and the
  shell, and 8 file descriptors per thread.
- **Nothing restarts the shell.** If the shell faults or exits, it is freed
  like any other orphan, only the idle thread is left, and the machine hangs
  silently.
- **Device addresses are hard-coded** for QEMU `virt` in `kernel/virt.h`.

### Processes

- **Programs are embedded in the kernel image.** There is no file system, and
  adding a program means editing `kernel/user.S` and `kernel/proc.c`.
- **One stack page, no heap, no arguments.** A program gets 4 KiB of stack,
  cannot grow its memory, and `main` takes no `argc` or `argv`.
- **Program names are shorter than 16 bytes**, the size of the kernel buffer in
  `syscall_spawn`.

### Files

- **No reference counts, no `close`, no `dup`, no pipes.** The console is the
  only file, and it is never freed.
- **Console `read` returns one byte per call.** There is no line discipline in
  the kernel.

### Devices

- **`uart_putc` does not wait for the transmitter.** QEMU accepts bytes
  immediately, but real hardware would drop them.
- **Unread input keeps the UART interrupt asserted.** The receive interrupt
  stays raised until someone reads `RBR`, but `uart_interrupt` only wakes
  readers. When a byte arrives while no thread is reading (for example while
  the shell waits for `count`), a level-triggered PLIC on real hardware would
  deliver the interrupt again right after every completion. QEMU's PLIC does
  not, so this does not show up there.
- **`poweroff` reports a failed shutdown as `-ERR_NOSYS`**, because
  `syscall_handle` falls through to the "unknown call" return.
