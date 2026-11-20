# Known issues

Bugs and limits of the current design, roughly from most to least important.
Remove an entry when it is fixed.

## Bugs

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

### Files

- **Console `read` returns one byte per call.** There is no line discipline in
  the kernel.

### Devices

- **`poweroff` reports a failed shutdown as `-ERR_NOSYS`**, because
  `syscall_handle` falls through to the "unknown call" return.
