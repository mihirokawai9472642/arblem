// Kernel threads: thread table, round-robin scheduler, sleep/wakeup, and
// exit/wait. A thread that runs a user program also owns its page table and
// file table, so this is also the process table. See docs/threads.md.

#include "thread.h"
#include "cpu.h"
#include "error.h"
#include "file.h"
#include "page.h"
#include "panic.h"
#include "proc.h"
#include "riscv.h"
#include "spinlock.h"
#include "vm.h"

// Slots in the thread table, including one idle thread per hart.
#define THREAD_MAX 16

void thread_switch(unsigned long *old_sp, unsigned long new_sp);
void thread_entry(void);
void thread_entry_unlock(void);

enum thread_state {
    THREAD_FREE = 0,
    THREAD_INIT,
    THREAD_RUNNABLE,
    THREAD_RUNNING,
    THREAD_SLEEPING,
    THREAD_ZOMBIE,
};

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

static struct spinlock sched_lock = SPINLOCK_INIT("sched");
static struct thread thread_table[THREAD_MAX];

// Wait channel for thread_wait. Only its address matters.
static char thread_exit_wait;

// Called from thread_entry in thread.S on a brand-new thread's first run.
// Releases the sched_lock held across thread_switch and leaves interrupts off.
void thread_entry_unlock(void) {
    cpu_current()->int_was_on = 0;
    spinlock_release(&sched_lock);
}

// Per-hart idle loop: wait for an interrupt with S-mode interrupts enabled,
// then yield to check whether another hart woke a runnable thread.
static void thread_idle(unsigned long arg) {
    (void)arg;

    for (;;) {
        csr_set(sstatus, SSTATUS_SIE);
        __asm__ volatile("wfi");
        csr_clear(sstatus, SSTATUS_SIE);
        thread_yield();
    }
}

// Free every exited thread whose parent is gone, since nobody will ever wait
// for it. Detach resources under sched_lock and free them outside the lock so
// vm_free/page_free never run while sched_lock is held.
static void thread_reap(void) {
    for (;;) {
        unsigned long *vm = 0;
        void *stack = 0;

        spinlock_acquire(&sched_lock);

        for (int i = 0; i < THREAD_MAX; i++) {
            struct thread *t = &thread_table[i];

            if (t->state == THREAD_ZOMBIE && t->parent == -1) {
                vm = t->vm;
                stack = t->stack;
                t->vm = 0;
                t->stack = 0;
                t->state = THREAD_FREE;
                break;
            }
        }

        spinlock_release(&sched_lock);

        if (stack == 0) {
            return;
        }

        if (vm != 0) {
            vm_free(vm);
        }

        page_free(stack);
    }
}

int thread_create(void (*entry)(unsigned long), unsigned long arg,
                  unsigned long *vm) {
    thread_reap();

    unsigned char *stack = page_alloc();

    if (stack == 0) {
        return -ERR_NOMEM;
    }

    // Build the frame thread_switch expects at the top of the new stack, so
    // that the first switch "returns" to thread_entry with s0 = entry and
    // s1 = arg. The other saved registers start as zero.
    unsigned long *frame =
        (unsigned long *)(stack + PAGE_SIZE - THREAD_FRAME_SIZE);

    frame[0] = (unsigned long)thread_entry; // ra
    frame[1] = (unsigned long)entry;        // s0
    frame[2] = arg;                         // s1

    // Inherit the creator's files outside sched_lock so file_ref's file_lock
    // is never nested inside sched_lock. Only the current thread modifies its
    // own files[] table.
    int cur = cpu_current()->current;
    struct file *inherited[THREAD_FILE_MAX];

    for (int fd = 0; fd < THREAD_FILE_MAX; fd++) {
        struct file *f = cur >= 0 ? thread_table[cur].files[fd] : 0;

        inherited[fd] = f != 0 ? file_ref(f) : 0;
    }

    spinlock_acquire(&sched_lock);

    int id = 0;

    while (id < THREAD_MAX && thread_table[id].state != THREAD_FREE) {
        id++;
    }

    if (id == THREAD_MAX) {
        spinlock_release(&sched_lock);

        for (int fd = 0; fd < THREAD_FILE_MAX; fd++) {
            if (inherited[fd] != 0) {
                file_close(inherited[fd]);
            }
        }

        page_free(stack);

        return -ERR_AGAIN;
    }

    struct thread *t = &thread_table[id];

    t->state = THREAD_INIT;
    t->is_idle = 0;
    t->sp = (unsigned long)frame;
    t->chan = 0;
    t->vm = vm;
    t->stack = stack;
    t->parent = cur;
    t->exit_code = 0;
    t->user_sp = 0;
    t->user_argc = 0;
    t->user_argv = 0;
    t->heap_base = 0;
    t->heap_end = 0;

    for (int fd = 0; fd < THREAD_FILE_MAX; fd++) {
        t->files[fd] = inherited[fd];
    }

    spinlock_release(&sched_lock);

    return id;
}

void thread_ready(int id) {
    spinlock_acquire(&sched_lock);
    thread_table[id].state = THREAD_RUNNABLE;
    spinlock_release(&sched_lock);
}

// Find the next non-idle runnable thread after start, or fall back to this
// hart's own idle thread. Caller must hold sched_lock.
static int thread_pick_next(int start) {
    for (int step = 1; step <= THREAD_MAX; step++) {
        int i = (start + step) % THREAD_MAX;

        if (thread_table[i].state == THREAD_RUNNABLE &&
            !thread_table[i].is_idle) {
            return i;
        }
    }

    return cpu_current()->idle;
}

void thread_start(void) {
    unsigned long unused;
    int idle = thread_create(thread_idle, 0, 0);

    if (idle < 0) {
        panic("cannot create the idle thread");
    }

    spinlock_acquire(&sched_lock);

    struct cpu *c = cpu_current();

    thread_table[idle].is_idle = 1;
    thread_table[idle].state = THREAD_RUNNABLE;
    c->idle = idle;

    int next = thread_pick_next(THREAD_MAX - 1);

    thread_table[next].state = THREAD_RUNNING;
    c->current = next;

    vm_switch(thread_table[next].vm);
    thread_switch(&unused, thread_table[next].sp);
    panic("thread_start: returned");
}

// Switch from the current thread to the next runnable thread (or this hart's
// idle thread). Caller must hold sched_lock (and no other spinlock) and must
// already have updated the current thread's state.
static void thread_sched(void) {
    if (!spinlock_holding(&sched_lock)) {
        panic("thread_sched: not holding sched_lock");
    }

    struct cpu *c = cpu_current();

    if (c->lock_depth != 1) {
        panic("thread_sched: holding extra lock");
    }

    if ((csr_read(sstatus) & SSTATUS_SIE) != 0) {
        panic("thread_sched: interrupts enabled");
    }

    int prev = c->current;
    int next = thread_pick_next(prev);

    thread_table[next].state = THREAD_RUNNING;

    if (next == prev) {
        return;
    }

    // Preserve int_was_on across the switch: another thread may resume on this
    // hart with a different saved interrupt state.
    int int_was_on = c->int_was_on;

    c->current = next;
    vm_switch(thread_table[next].vm);
    thread_switch(&thread_table[prev].sp, thread_table[next].sp);

    // After thread_switch we may be on a different hart: reload cpu_current().
    cpu_current()->int_was_on = int_was_on;
}

void thread_yield(void) {
    thread_reap();

    spinlock_acquire(&sched_lock);
    thread_table[cpu_current()->current].state = THREAD_RUNNABLE;
    thread_sched();
    spinlock_release(&sched_lock);
}

void thread_sleep(void *chan) {
    spinlock_acquire(&sched_lock);

    struct thread *t = &thread_table[cpu_current()->current];

    t->chan = chan;
    t->state = THREAD_SLEEPING;
    thread_sched();
    t->chan = 0;

    spinlock_release(&sched_lock);
}

static void thread_wakeup_locked(void *chan) {
    for (int i = 0; i < THREAD_MAX; i++) {
        if (thread_table[i].state == THREAD_SLEEPING &&
            thread_table[i].chan == chan) {
            thread_table[i].state = THREAD_RUNNABLE;
        }
    }
}

void thread_wakeup(void *chan) {
    spinlock_acquire(&sched_lock);
    thread_wakeup_locked(chan);
    spinlock_release(&sched_lock);
}

void thread_exit(int code) {
    int cur = cpu_current()->current;
    struct thread *t = &thread_table[cur];

    // Close open files now, before turning into a zombie, so a parent
    // waiting for EOF on a pipe does not deadlock waiting for thread_wait.
    // Done outside sched_lock because file_close acquires file_lock and may
    // call pipe/fs close callbacks.
    for (int fd = 0; fd < THREAD_FILE_MAX; fd++) {
        if (t->files[fd] != 0) {
            file_close(t->files[fd]);
            t->files[fd] = 0;
        }
    }

    spinlock_acquire(&sched_lock);

    t->exit_code = code;

    // Children become orphans. Once an orphan has exited, thread_reap frees
    // it, since no parent is left to wait for it.
    for (int i = 0; i < THREAD_MAX; i++) {
        if (thread_table[i].state != THREAD_FREE &&
            thread_table[i].parent == cur) {
            thread_table[i].parent = -1;
        }
    }

    // Wake any parent in thread_wait, mark ourselves a zombie, and switch off
    // this stack while still holding sched_lock so no other hart can free our
    // stack or page table until after thread_switch finishes.
    thread_wakeup_locked(&thread_exit_wait);
    t->state = THREAD_ZOMBIE;
    thread_sched();

    panic("thread_exit: zombie resumed");
}

int thread_wait(unsigned long id, int *code) {
    spinlock_acquire(&sched_lock);

    int cur = cpu_current()->current;

    if (id >= THREAD_MAX || thread_table[id].state == THREAD_FREE ||
        thread_table[id].parent != cur) {
        spinlock_release(&sched_lock);

        return -ERR_CHILD;
    }

    struct thread *t = &thread_table[id];

    // Every exit wakes every waiter, so recheck that it was this child.
    while (t->state != THREAD_ZOMBIE) {
        struct thread *self = &thread_table[cur];

        self->chan = &thread_exit_wait;
        self->state = THREAD_SLEEPING;
        thread_sched();
        self->chan = 0;
    }

    *code = t->exit_code;

    unsigned long *vm = t->vm;
    void *stack = t->stack;

    t->vm = 0;
    t->stack = 0;
    t->state = THREAD_FREE;

    spinlock_release(&sched_lock);

    // The child never runs again, so its page table and kernel stack can go.
    if (vm != 0) {
        vm_free(vm);
    }

    page_free(stack);

    return 0;
}

struct file *thread_file(unsigned long fd) {
    if (fd >= THREAD_FILE_MAX) {
        return 0;
    }

    return thread_table[cpu_current()->current].files[fd];
}

int thread_alloc_fd(struct file *f) {
    struct thread *t = &thread_table[cpu_current()->current];

    for (int fd = 0; fd < THREAD_FILE_MAX; fd++) {
        if (t->files[fd] == 0) {
            t->files[fd] = f;

            return fd;
        }
    }

    return -ERR_MFILE;
}

int thread_close_fd(unsigned long fd) {
    if (fd >= THREAD_FILE_MAX) {
        return -ERR_BADF;
    }

    struct thread *t = &thread_table[cpu_current()->current];
    struct file *f = t->files[fd];

    if (f == 0) {
        return -ERR_BADF;
    }

    t->files[fd] = 0;
    file_close(f);

    return 0;
}

void thread_set_start(int id, unsigned long sp, unsigned long argc,
                      unsigned long argv) {
    struct thread *t = &thread_table[id];

    t->user_sp = sp;
    t->user_argc = argc;
    t->user_argv = argv;
}

void thread_get_start(unsigned long *sp, unsigned long *argc,
                      unsigned long *argv) {
    struct thread *t = &thread_table[cpu_current()->current];

    *sp = t->user_sp;
    *argc = t->user_argc;
    *argv = t->user_argv;
}

// Round addr up to the next PAGE_SIZE boundary.
static unsigned long page_round_up(unsigned long addr) {
    return (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1UL);
}

void thread_set_heap(int id, unsigned long base) {
    struct thread *t = &thread_table[id];

    t->heap_base = base;
    t->heap_end = base;
}

long thread_sbrk(long delta) {
    struct thread *t = &thread_table[cpu_current()->current];
    unsigned long old_end = t->heap_end;

    if (delta == 0) {
        return (long)old_end;
    }

    if (delta > 0) {
        unsigned long inc = (unsigned long)delta;

        if (inc > PROC_HEAP_LIMIT - old_end) {
            return -ERR_NOMEM;
        }

        t->heap_end = old_end + inc;

        return (long)old_end;
    }

    // delta < 0: negate in unsigned arithmetic to avoid UB on LONG_MIN.
    unsigned long dec = -(unsigned long)delta;

    if (dec > old_end - t->heap_base) {
        return -ERR_NOMEM;
    }

    unsigned long new_end = old_end - dec;
    unsigned long old_top = page_round_up(old_end);
    unsigned long new_top = page_round_up(new_end);

    for (unsigned long va = new_top; va < old_top; va += PAGE_SIZE) {
        vm_unmap(t->vm, va);
    }

    t->heap_end = new_end;

    return (long)old_end;
}

int thread_fault(unsigned long va) {
    int cur = cpu_current()->current;

    if (cur < 0) {
        return -1;
    }

    struct thread *t = &thread_table[cur];

    if (t->vm == 0) {
        return -1;
    }

    int in_heap = va >= t->heap_base && va < t->heap_end;
    int in_stack = va >= PROC_STACK_MIN && va < PROC_STACK_TOP;

    if (!in_heap && !in_stack) {
        return -1;
    }

    unsigned long page_va = va & ~(PAGE_SIZE - 1UL);

    // If the page is already mapped, this was a permission fault (not a
    // missing page). Refuse it rather than overwriting the PTE.
    if (vm_is_mapped(t->vm, page_va)) {
        return -1;
    }

    void *page = page_alloc();

    if (page == 0) {
        return -1;
    }

    if (vm_map(t->vm, page_va, (unsigned long)page, PAGE_SIZE,
               PTE_R | PTE_W | PTE_U) != 0) {
        page_free(page);

        return -1;
    }

    __asm__ volatile("sfence.vma" : : : "memory");

    return 0;
}
