# Known issues

Bugs and limits of the current design, roughly from most to least important.
Remove an entry when it is fixed.

## Bugs

No known bugs at the moment.

## Limits

### Kernel

- **One hart, no locks.** The kernel relies on running with interrupts off and
  on a single hart. See [Architecture](architecture.md#design-rules).
- **No kernel stack guard.** Each kernel stack is one page, with ordinary
  memory on both sides. An overflow silently corrupts the neighboring page.
- **Fixed-size tables.** At most 8 threads (including the idle thread), 8 file
  descriptors per thread, 16 open filesystem file descriptions (`FS_OPEN_MAX`),
  and 16 block cache buffers (`BCACHE_COUNT`).
- **Nothing restarts the shell.** If the shell faults or exits, it is freed
  like any other orphan, only the idle thread is left, and the machine hangs
  silently.

### Files

- **Console `read` returns one byte per call.** There is no line discipline in
  the kernel.
- **No per-process working directory (`cwd`).** Relative paths always resolve from
  `/`.
- **Unlinking an open file is not deferred.** `fs_unlink` frees the inode and its
  blocks immediately rather than waiting for open descriptors to close.
- **No crash journaling.** Filesystem writes are write-through, not atomic across
  multiple metadata blocks.

### Devices

- **`poweroff` reports a failed shutdown as `-ERR_NOSYS`**, because
  `syscall_handle` falls through to the "unknown call" return.
- **One in-flight block request at a time.** `virtio_blk_rw` serializes requests
  with `blk_busy` and reuses descriptors 0-2.
