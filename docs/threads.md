# Threads

`kernel/thread.c` and `kernel/thread.S` implement kernel threads, the
round-robin scheduler, sleep/wakeup, and exit/wait. Every user program runs in
one thread, so this is also Arblem's process table (see
[Processes](processes.md)).

## The thread table

```c
struct thread {
    enum thread_state state;
    int is_idle;             // Non-zero for a per-hart idle thread
    unsigned long sp;        // Saved kernel sp while switched out
    void *chan;              // Wait channel while THREAD_SLEEPING
    unsigned long *vm;       // Page table root, 0 for the kernel's
    void *stack;             // Kernel stack page, 0 when THREAD_FREE
    int parent;              // ID of the creating thread, -1 if none
    int exit_code;           // Set by thread_exit
    unsigned long user_sp;   // Initial U-mode sp
    unsigned long user_argc; // Initial U-mode a0 (argc)
    unsigned long user_argv; // Initial U-mode a1 (argv)
    unsigned long heap_base; // First byte of the user heap (page aligned)
    unsigned long heap_end;  // Current program break (sbrk)
    struct file *files[THREAD_FILE_MAX];
};
```

The table has `THREAD_MAX` (16) slots, including one dedicated idle thread per
hart (`is_idle = 1`), and is guarded by `sched_lock`. Each hart records its own
running thread and idle thread in `cpu_current()->current` (`-1` before
`thread_start`) and `cpu_current()->idle`. A thread's ID is its slot index.

A thread is in one of six explicit states (`enum thread_state`):

| State             | Meaning                                                     | Scheduled? |
|-------------------|-------------------------------------------------------------|------------|
| `THREAD_FREE`     | Unused slot (`stack == 0`)                                  | No         |
| `THREAD_INIT`     | Allocated and populated before `thread_ready()` is called   | No         |
| `THREAD_RUNNABLE` | Ready to run on any hart                                    | Yes        |
| `THREAD_RUNNING`  | Currently running on one hart                               | No (already on a hart) |
| `THREAD_SLEEPING` | Blocked on `chan` until `thread_wakeup`                     | No         |
| `THREAD_ZOMBIE`   | Exited; waits to be freed by `thread_wait` or `thread_reap` | No         |

## Creating a thread

`thread_create(entry, arg, vm)` first calls `thread_reap()`, allocates a
one-page kernel stack, inherits the creator's open files outside `sched_lock`
(so `file_lock` is never nested inside `sched_lock`), and then acquires
`sched_lock` to claim the first `THREAD_FREE` slot in `THREAD_INIT` state. At
the top of the new stack it builds a fake `thread_switch` frame:

```
stack + 4096 -> +-------------------+
                | (unused)          |
                | s2-s11            |
                | s1 = arg          |  frame[2]
                | s0 = entry        |  frame[1]
  t->sp ------> | ra = thread_entry |  frame[0]
                +-------------------+
```

The first time a hart switches to the thread, `thread_switch` "returns" to
`thread_entry` (`kernel/thread.S`), which calls `thread_entry_unlock()` to
release the `sched_lock` held across `thread_switch` (with `sstatus.SIE` kept
off) and jumps to `entry(arg)`.

Because `THREAD_INIT` threads are ignored by the scheduler, callers finish
setting up `thread_set_start` and `thread_set_heap` and then call
`thread_ready(id)` to transition the slot to `THREAD_RUNNABLE`. Threads created
during boot (`cpu_current()->current == -1`) get an empty file table and no
parent.

## Switching (`thread_switch` in `kernel/thread.S`)

`thread_switch(&old->sp, new->sp)`:

1. Pushes `ra` and `s0`-`s11` on the current stack (`THREAD_FRAME_SIZE`, 112
   bytes: 13 registers rounded up to 16 bytes).
2. Stores `sp` into `*old_sp`.
3. Loads `sp` from `new_sp` and pops the same registers.
4. Returns, which jumps to the `ra` it just loaded.

Only callee-saved registers need saving, because `thread_switch` is called like
any C function: the compiler already assumes it clobbers the rest.

## Scheduling (`thread_yield` and `thread_sched`)

All context switches go through `thread_sched()` with `sched_lock` held,
`cpu_current()->lock_depth == 1` (no other spinlock held), and `sstatus.SIE`
cleared:

1. `thread_pick_next` walks the table forward from the current slot, wrapping
   around, and picks the next `THREAD_RUNNABLE` thread with `!is_idle`. If none
   is runnable, it falls back to `cpu_current()->idle` (each hart runs only its
   own idle thread).
2. Marks the chosen thread `THREAD_RUNNING`, updates `cpu_current()->current`,
   switches the page table with `vm_switch`, preserves `c->int_was_on` across
   `thread_switch`, and restores `int_was_on` on the destination hart (`tp`
   reloaded via `cpu_current()`, since the thread may have migrated).
3. `sched_lock` stays held across `thread_switch` so no other hart can pick the
   outgoing thread (or free a zombie's stack) before its registers are saved; the
   next thread releases `sched_lock` in `thread_sched`'s caller (or in
   `thread_entry_unlock` on its first run).

Each hart's `thread_idle` enables `sstatus.SIE` only around `wfi`, clears `SIE`,
and calls `thread_yield()` whenever an interrupt wakes it.

`thread_init_hart()` creates a hart's dedicated idle thread (`is_idle = 1`,
stored in `cpu_current()->idle`), and `thread_start()` calls it if needed,
claims the first runnable thread under `sched_lock`, and enters `thread_switch`
without returning. `thread_yield` is called from `trap_handler` after every
interrupt (preemption) and from `thread_idle`.

## Sleep and wakeup

A *channel* is any address, used only as a name. `thread_sleep(chan, lk)` takes
the spinlock `lk` protecting the caller's wait condition:

```c
spinlock_acquire(&lk);
while (!condition) {
    thread_sleep(&channel, &lk);
}
spinlock_release(&lk);
```

Inside `thread_sleep(chan, lk)`, the kernel acquires `sched_lock` *before*
releasing `lk`, marks the thread `THREAD_SLEEPING` on `chan`, calls
`thread_sched()`, clears `chan`, swaps `sched_lock` back out, and re-acquires
`lk`. Because `thread_wakeup(chan)` acquires `sched_lock`, a wakeup on another
hart cannot slip in between the condition check and the thread entering
`THREAD_SLEEPING` (preventing lost wakeups).

| Channel             | Sleeps on it                     | Woken by                                     |
|---------------------|----------------------------------|----------------------------------------------|
| `&uart_rx_wait`     | A thread reading the console     | The UART receive interrupt (`uart_lock`)     |
| `&blk_busy_wait`    | A thread waiting for the block device | Completion of `virtio_blk_rw` (`blk_lock`) |
| `&blk_done_wait`    | A thread waiting for block DMA   | `virtio_blk_interrupt` (`blk_lock`)          |
| `&bcache_wait`      | A thread waiting for a cache buffer being read | `bcache_get` after disk I/O (`bcache_lock`) |
| `&p->read_end`      | A thread reading an empty pipe   | `pipe_write` or closing the last write end (`p->lock`) |
| `&p->write_end`     | A thread writing a full pipe     | `pipe_read` or closing the last read end (`p->lock`) |
| `sl` (`struct sleeplock *`) | A thread waiting on a `sleeplock` (`fs_lock`) | `sleeplock_release` (`sl->lk`) |
| `&thread_exit_wait` | A parent in `thread_wait`        | Any thread calling `thread_exit` (`sched_lock`) |

## Exit and wait

`thread_exit(code)`:

1. Closes every open file descriptor with `file_close` outside `sched_lock` (so
   a parent reading a pipe sees EOF before `thread_wait`, and close callbacks
   can acquire pipe or filesystem locks).
2. Acquires `sched_lock`, stores `exit_code`, and turns every child into an
   orphan (`parent = -1`).
3. Wakes any waiter on `&thread_exit_wait`, marks the thread `THREAD_ZOMBIE`,
   and calls `thread_sched()` while still holding `sched_lock`. Because
   `sched_lock` is released only after `thread_switch` switches to another
   thread's stack, no other hart can free this zombie's stack or page table
   until it is completely off them.

The thread keeps its slot, stack and page table until either its parent collects
it (`thread_wait`) or `thread_reap` frees an orphan:

`thread_wait(id, &code)`:

1. Acquires `sched_lock` and fails with `-ERR_CHILD` unless `id` is not
   `THREAD_FREE` and its `parent` is the caller.
2. While `t->state != THREAD_ZOMBIE`, sets `self->chan = &thread_exit_wait` and
   `self->state = THREAD_SLEEPING` directly under `sched_lock`, and calls
   `thread_sched()`.
3. Copies `exit_code`, detaches `vm` and `stack`, resets `state` to
   `THREAD_FREE`, releases `sched_lock`, and frees `vm` and `stack` outside the
   lock.

`thread_reap` runs at the start of `thread_yield` and `thread_create`. It
acquires `sched_lock`, detaches one orphan `THREAD_ZOMBIE` (`parent == -1`),
marks its slot `THREAD_FREE`, releases `sched_lock`, and calls `vm_free` (if
`vm != 0`, skipping kernel-only threads such as `kernel_init`) and `page_free`
outside the lock, repeating until no orphan zombies remain.

## File table

Each thread has `THREAD_FILE_MAX` (8) file pointers, indexed by file
descriptor. `thread_file(fd)` returns the current thread's file for `fd`, or 0
if `fd` is out of range or unused. `thread_alloc_fd(f)` installs `f` in the
lowest free descriptor of the current thread, and `thread_close_fd(fd)` removes
it and drops its reference.

A `struct file` (`kernel/file.h`) holds a reference count `ref`, a private
pointer `priv`, and `read`, `write`, and `close` function pointers. `file_ref`
and `file_close` (`kernel/file.c`) guard `ref` with `file_lock` (`struct
spinlock`); when `ref` reaches zero, `file_close` drops `file_lock` before
calling `close` so pipe (`p->lock`) and filesystem (`fs_lock`) close paths can
acquire their own locks safely. Today there are three implementations: the static
console (`kernel/console.c`, `close = 0`), one-page ring-buffer pipes
(`kernel/pipe.c`, per-pipe `struct spinlock lock`, freed outside `p->lock` once
both ends close), and filesystem open files (`struct fs_file` in `kernel/kfs.c`,
16-slot static pool protected by `fs_lock`).

## Heap and demand paging

Each user thread records `[heap_base, heap_end)` (`kernel/proc.h` bounds it at
`PROC_HEAP_LIMIT = 0x7ffef000`). `thread_set_heap(id, base)` initializes both to
the page-aligned end of the ELF segments, and `thread_sbrk(delta)` moves
`heap_end`: growing advances the break lazily without allocating physical pages,
while shrinking calls `vm_unmap` to free every page wholly above the new break.

`thread_fault(va)` handles missing pages inside `[heap_base, heap_end)` and the
64 KiB stack range `[PROC_STACK_MIN, PROC_STACK_TOP)` (`0x7fff0000`-`0x80000000`),
leaving `[0x7ffef000, 0x7fff0000)` unmapped as a stack guard page. It refuses
addresses outside those ranges or pages that are already mapped (permission
faults), and otherwise allocates a zeroed page and maps it `PTE_R | PTE_W | PTE_U`
followed by `sfence.vma`. Both `trap_handler` (for U-mode load/store page
faults) and `vm_check_user` (for syscalls touching unallocated heap or stack
pages) call `thread_fault`.
