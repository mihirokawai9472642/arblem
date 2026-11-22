// Kernel threads: thread table, round-robin scheduler, sleep/wakeup, and
// exit/wait. A thread that runs a user program also owns its page table and
// file table, so this is also the process table. See docs/threads.md.
//
// The kernel runs on one hart with interrupts off while it handles a trap,
// so nothing here needs a lock.

#include "thread.h"
#include "error.h"
#include "file.h"
#include "page.h"
#include "panic.h"
#include "proc.h"
#include "riscv.h"
#include "vm.h"

// Slots in the thread table, including the idle thread.
#define THREAD_MAX 8

void thread_switch(unsigned long *old_sp, unsigned long new_sp);
void thread_entry(void);

// A thread's state follows from two fields:
//   stack == 0                       free slot
//   stack != 0, chan == 0            runnable
//   stack != 0, chan != 0            sleeping on chan
//   stack != 0, chan == &thread_dead exited, waiting to be freed
struct thread {
    unsigned long sp;        // Saved kernel sp while switched out
    void *chan;              // Wait channel, 0 if runnable
    unsigned long *vm;       // Page table root, 0 for the kernel's
    void *stack;             // Kernel stack page, 0 if the slot is free
    int parent;              // ID of the creating thread, -1 if none
    int exit_code;           // Set by thread_exit
    unsigned long user_sp;   // Initial U-mode sp
    unsigned long user_argc; // Initial U-mode a0 (argc)
    unsigned long user_argv; // Initial U-mode a1 (argv)
    unsigned long heap_base; // First byte of the user heap (page aligned)
    unsigned long heap_end;  // Current program break (sbrk)
    struct file *files[THREAD_FILE_MAX];
};

static struct thread thread_table[THREAD_MAX];

// Index of the running thread, -1 until thread_start.
static int thread_current = -1;

// Wait channels. Only their addresses matter.
static char thread_dead;      // Exited threads sleep here forever
static char thread_exit_wait; // thread_wait sleeps here

// Runs when nothing else can. Always runnable, so thread_yield always finds a
// thread. wfi with interrupts enabled stops the hart until the next interrupt.
static void thread_idle(unsigned long arg) {
    (void)arg;

    for (;;) {
        __asm__ volatile("wfi");
    }
}

int thread_create(void (*entry)(unsigned long), unsigned long arg,
                  unsigned long *vm) {
    int id = 0;

    while (id < THREAD_MAX && thread_table[id].stack != 0) {
        id++;
    }

    if (id == THREAD_MAX) {
        return -ERR_AGAIN;
    }

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

    struct thread *t = &thread_table[id];

    t->sp = (unsigned long)frame;
    t->chan = 0;
    t->vm = vm;
    t->stack = stack;
    t->parent = thread_current;
    t->user_sp = 0;
    t->user_argc = 0;
    t->user_argv = 0;
    t->heap_base = 0;
    t->heap_end = 0;

    // Inherit the creator's files. Threads created during boot get none.
    for (int fd = 0; fd < THREAD_FILE_MAX; fd++) {
        struct file *f =
            thread_current >= 0 ? thread_table[thread_current].files[fd] : 0;

        t->files[fd] = f != 0 ? file_ref(f) : 0;
    }

    return id;
}

void thread_start(void) {
    unsigned long unused;

    if (thread_create(thread_idle, 0, 0) < 0) {
        panic("cannot create the idle thread");
    }

    // Start slot 0, the first thread kernel_main created (the shell). The
    // boot stack's sp goes to a dummy, since nothing will switch back to it.
    thread_current = 0;

    vm_switch(thread_table[0].vm);
    thread_switch(&unused, thread_table[0].sp);
}

// Release a thread that has exited: its page table, the user pages mapped in
// it, and its kernel stack. The slot becomes free.
static void thread_free(struct thread *t) {
    if (t->vm != 0) {
        vm_free(t->vm);
    }

    page_free(t->stack);
    t->stack = 0;
}

// Free every exited thread whose parent is gone, since nobody will ever wait
// for it. The current thread is skipped: if it has just exited, it is still
// running on its own kernel stack and page table.
static void thread_reap(void) {
    for (int i = 0; i < THREAD_MAX; i++) {
        struct thread *t = &thread_table[i];

        if (i != thread_current && t->stack != 0 && t->chan == &thread_dead &&
            t->parent == -1) {
            thread_free(t);
        }
    }
}

void thread_yield(void) {
    int prev = thread_current;

    thread_reap();

    // Find the next slot that is in use and not sleeping, wrapping around.
    // The idle thread guarantees that the loop ends.
    do {
        thread_current++;

        if (thread_current == THREAD_MAX) {
            thread_current = 0;
        }
    } while (thread_table[thread_current].stack == 0 ||
             thread_table[thread_current].chan != 0);

    if (thread_current == prev) {
        return;
    }

    vm_switch(thread_table[thread_current].vm);
    thread_switch(&thread_table[prev].sp, thread_table[thread_current].sp);
}

// Callers check their condition in a loop around thread_sleep. Interrupts are
// off in the kernel, so a wakeup cannot slip in between the check and the
// sleep.
void thread_sleep(void *chan) {
    thread_table[thread_current].chan = chan;
    thread_yield();
}

void thread_wakeup(void *chan) {
    for (int i = 0; i < THREAD_MAX; i++) {
        if (thread_table[i].chan == chan) {
            thread_table[i].chan = 0;
        }
    }
}

void thread_exit(int code) {
    struct thread *t = &thread_table[thread_current];

    t->exit_code = code;

    // Close open files now, before turning into a zombie, so a parent
    // waiting for EOF on a pipe does not deadlock waiting for thread_wait.
    for (int fd = 0; fd < THREAD_FILE_MAX; fd++) {
        if (t->files[fd] != 0) {
            file_close(t->files[fd]);
            t->files[fd] = 0;
        }
    }

    // Children become orphans. Once an orphan has exited, thread_reap frees
    // it, since no parent is left to wait for it.
    for (int i = 0; i < THREAD_MAX; i++) {
        if (thread_table[i].parent == thread_current) {
            thread_table[i].parent = -1;
        }
    }

    // Let any thread_wait recheck its child, then stop for good. The slot,
    // stack and page table stay until the parent's thread_wait, or
    // thread_reap for an orphan, frees them.
    thread_wakeup(&thread_exit_wait);
    thread_sleep(&thread_dead);
}

int thread_wait(unsigned long id, int *code) {
    if (id >= THREAD_MAX || thread_table[id].stack == 0 ||
        thread_table[id].parent != thread_current) {
        return -ERR_CHILD;
    }

    struct thread *t = &thread_table[id];

    // Every exit wakes every waiter, so recheck that it was this child.
    while (t->chan != &thread_dead) {
        thread_sleep(&thread_exit_wait);
    }

    *code = t->exit_code;

    // The child never runs again, so its page table and kernel stack can go.
    thread_free(t);

    return 0;
}

struct file *thread_file(unsigned long fd) {
    if (fd >= THREAD_FILE_MAX) {
        return 0;
    }

    return thread_table[thread_current].files[fd];
}

void thread_set_file(int id, unsigned long fd, struct file *f) {
    thread_table[id].files[fd] = f;
}

int thread_alloc_fd(struct file *f) {
    struct thread *t = &thread_table[thread_current];

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

    struct thread *t = &thread_table[thread_current];
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
    struct thread *t = &thread_table[thread_current];

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
    struct thread *t = &thread_table[thread_current];
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
    if (thread_current < 0) {
        return -1;
    }

    struct thread *t = &thread_table[thread_current];

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
