// Kernel threads: thread table, round-robin scheduler, sleep/wakeup, and
// exit/wait. A thread that runs a user program also owns its page table and
// file table, so this is also the process table. See docs/threads.md.
//
// The kernel runs on one hart with interrupts off while it handles a trap,
// so nothing here needs a lock.

#include "thread.h"
#include "error.h"
#include "page.h"
#include "panic.h"
#include "vm.h"

// Slots in the thread table, including the idle thread.
#define THREAD_MAX 8

void thread_switch(unsigned long *old_sp, unsigned long new_sp);
void thread_entry(void);

// A thread's state follows from two fields:
//   stack == 0                       free slot
//   stack != 0, chan == 0            runnable
//   stack != 0, chan != 0            sleeping on chan
//   stack != 0, chan == &thread_dead exited, waiting for thread_wait
struct thread {
    unsigned long sp;      // Saved kernel sp while switched out
    void *chan;            // Wait channel, 0 if runnable
    unsigned long *vm;     // Page table root, 0 for the kernel's
    void *stack;           // Kernel stack page, 0 if the slot is free
    int parent;            // ID of the creating thread, -1 if none
    int exit_code;         // Set by thread_exit
    struct file *files[THREAD_FILE_MAX];
};

static struct thread thread_table[THREAD_MAX];

// Index of the running thread, -1 until thread_start.
static int thread_current = -1;

// Wait channels. Only their addresses matter.
static char thread_dead;       // Exited threads sleep here forever
static char thread_exit_wait;  // thread_wait sleeps here

// Runs when nothing else can. Always runnable, so thread_yield always finds a
// thread. wfi with interrupts enabled stops the hart until the next interrupt.
static void thread_idle(unsigned long arg) {
    (void)arg;

    for (;;) {
        __asm__ volatile ("wfi");
    }
}

int thread_create(void (*entry)(unsigned long), unsigned long arg, unsigned long *vm) {
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
    unsigned long *frame = (unsigned long *)(stack + PAGE_SIZE - THREAD_FRAME_SIZE);

    frame[0] = (unsigned long)thread_entry;  // ra
    frame[1] = (unsigned long)entry;         // s0
    frame[2] = arg;                          // s1

    struct thread *t = &thread_table[id];

    t->sp = (unsigned long)frame;
    t->chan = 0;
    t->vm = vm;
    t->stack = stack;
    t->parent = thread_current;

    // Inherit the creator's files. Threads created during boot get none.
    for (int fd = 0; fd < THREAD_FILE_MAX; fd++) {
        t->files[fd] = thread_current >= 0 ? thread_table[thread_current].files[fd] : 0;
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

void thread_yield(void) {
    int prev = thread_current;

    // Find the next slot that is in use and not sleeping, wrapping around.
    // The idle thread guarantees that the loop ends.
    do {
        thread_current++;

        if (thread_current == THREAD_MAX) {
            thread_current = 0;
        }
    } while (thread_table[thread_current].stack == 0 || thread_table[thread_current].chan != 0);

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
    thread_table[thread_current].exit_code = code;

    // Children become orphans. Nothing frees an orphan after it exits yet;
    // see docs/known-issues.md.
    for (int i = 0; i < THREAD_MAX; i++) {
        if (thread_table[i].parent == thread_current) {
            thread_table[i].parent = -1;
        }
    }

    // Let any thread_wait recheck its child, then stop for good. The slot,
    // stack and page table stay until the parent's thread_wait frees them.
    thread_wakeup(&thread_exit_wait);
    thread_sleep(&thread_dead);
}

int thread_wait(unsigned long id, int *code) {
    if (id >= THREAD_MAX || thread_table[id].stack == 0 || thread_table[id].parent != thread_current) {
        return -ERR_CHILD;
    }

    struct thread *t = &thread_table[id];

    // Every exit wakes every waiter, so recheck that it was this child.
    while (t->chan != &thread_dead) {
        thread_sleep(&thread_exit_wait);
    }

    *code = t->exit_code;

    // The child never runs again, so its page table and kernel stack can go.
    vm_free(t->vm);
    page_free(t->stack);
    t->stack = 0;

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
