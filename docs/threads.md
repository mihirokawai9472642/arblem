# Threads

`kernel/thread.c` and `kernel/thread.S` implement kernel threads, the
round-robin scheduler, sleep/wakeup, and exit/wait. Every user program runs in
one thread, so this is also Arblem's process table (see
[Processes](processes.md)).

## The thread table

```c
struct thread {
    unsigned long sp;        // saved kernel sp while the thread is switched out
    void *chan;              // what the thread sleeps on; 0 = runnable
    unsigned long *vm;       // page table root; 0 = kernel page table
    void *stack;             // kernel stack page; 0 = slot is free
    int parent;              // creator's ID; -1 = none
    int exit_code;           // valid once the thread has exited
    struct file *files[THREAD_FILE_MAX];
};
```

The table has `THREAD_MAX` (8) slots. A thread's ID is its slot index. A slot
is free when `stack` is 0.

A thread is in one of these states. There is no explicit state field; the state
follows from `stack` and `chan`:

| State     | `stack` | `chan`          | Scheduled? |
|-----------|---------|-----------------|------------|
| Free      | 0       | -               | No         |
| Runnable  | set     | 0               | Yes        |
| Sleeping  | set     | wait channel    | No         |
| Exited    | set     | `&thread_dead`  | No, waits to be freed by `thread_wait` or `thread_reap` |

## Creating a thread

`thread_create(entry, arg, vm)` picks the first free slot, allocates a one-page
kernel stack, and builds a fake `thread_switch` frame at the top of it:

```
stack + 4096 -> +-------------------+
                | (unused)          |
                | s2-s11            |
                | s1 = arg          |  frame[2]
                | s0 = entry        |  frame[1]
  t->sp ------> | ra = thread_entry |  frame[0]
                +-------------------+
```

The first time the scheduler switches to the thread, `thread_switch` "returns"
to `thread_entry`, which enables interrupts and calls `entry(arg)`.

The new thread inherits its creator's file table. A thread created before the
scheduler starts (`thread_current == -1`) gets an empty file table and no
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

## Scheduling (`thread_yield`)

`thread_yield` walks the table forward from the current slot, wrapping around,
and picks the next thread that is neither free nor sleeping. If that is the
current thread, it returns at once. Otherwise it switches the page table with
`vm_switch` and the registers with `thread_switch`.

The idle thread is always runnable, so the search always ends. It loops on
`wfi` with interrupts enabled, which stops the hart until the next interrupt.

Before it picks the next thread, `thread_yield` calls `thread_reap` to free
exited orphans (see [Exit and wait](#exit-and-wait)).

`thread_yield` is called from two places:

- `trap_handler`, after every interrupt (preemption).
- `thread_sleep`, when a thread blocks.

`thread_start` runs once, at the end of boot. It creates the idle thread and
switches to slot 0 without saving the boot code's registers, since there is
nothing to come back to.

## Sleep and wakeup

A *channel* is any address, used only as a name. `thread_sleep(chan)` marks the
current thread as sleeping on `chan` and yields. `thread_wakeup(chan)` makes
every thread sleeping on `chan` runnable again. The usual pattern is a loop
that rechecks the condition, because a wakeup only means "something changed":

```c
while (!condition) {
    thread_sleep(&channel);
}
```

| Channel           | Sleeps on it                     | Woken by                    |
|-------------------|----------------------------------|-----------------------------|
| `&uart_rx_wait`   | A thread reading the console     | The UART receive interrupt  |
| `&thread_exit_wait` | A parent in `thread_wait`      | Any thread calling `thread_exit` |
| `&thread_dead`    | An exited thread, forever        | Nobody                      |

Because the kernel runs with interrupts off, nothing can happen between
checking the condition and calling `thread_sleep`. A wakeup that arrives in
that gap cannot be lost.

## Exit and wait

`thread_exit(code)`:

1. Stores the exit code.
2. Makes every child an orphan (`parent = -1`).
3. Wakes all threads in `thread_wait`.
4. Sleeps on `&thread_dead` and never runs again.

The thread keeps its slot, stack and page table, so it is a *zombie* until
someone frees it. It cannot free them itself, because it is still running on
that kernel stack and page table. Who frees it depends on its parent:

- **The parent is alive**: the parent collects it with `thread_wait`.
- **There is no parent** (`parent == -1`, an *orphan*): `thread_reap` frees it.

`thread_wait(id, &code)`:

1. Fails with `-ERR_CHILD` unless `id` is a live slot whose parent is the
   caller.
2. Sleeps on `&thread_exit_wait` until the child is on `&thread_dead`.
3. Copies the exit code and frees the child with `thread_free`: its page
   table, user pages and kernel stack, and its slot.

`thread_reap` runs at the start of every `thread_yield`. It frees, with
`thread_free`, every zombie whose `parent` is -1, except the current thread,
which may have just exited and is still running on its own stack. That covers
both orders:

- **The child exited before its parent**: the parent's `thread_exit` turns it
  into an orphan, and the parent's own `thread_yield` frees it right away.
- **The child exits after its parent**: it is already an orphan, so the next
  `thread_yield` on another thread frees it. The idle thread yields on every
  timer interrupt, so this takes at most about 10 ms.

`thread_yield` always runs with interrupts off, and the zombie's page table is
never the active one there, so freeing needs no lock and no extra TLB flush.

## File table

Each thread has `THREAD_FILE_MAX` (8) file pointers, indexed by file
descriptor. `thread_file(fd)` returns the current thread's file for `fd`, or 0
if `fd` is out of range or unused. `thread_set_file(id, fd, f)` lets the kernel
install a file in another thread; boot uses it to give the shell the console.

A `struct file` (`kernel/file.h`) is a pair of function pointers, `read` and
`write`, which take a *user* buffer address. The only file today is the
console (`kernel/console.c`). Files have no reference count and there is no
`close`, because the console is a static object that is never freed.
