#include "thread.h"
#include "error.h"
#include "page.h"
#include "panic.h"
#include "vm.h"

#define THREAD_MAX 8

void thread_switch(unsigned long *old_sp, unsigned long new_sp);
void thread_entry(void);

struct thread {
    unsigned long sp;
    void *chan;
    unsigned long *vm;
    void *stack;
};

static struct thread thread_table[THREAD_MAX];
static int thread_current;
static char thread_dead;
static char thread_exit_wait;

static void thread_idle(void) {
    for (;;) {
        __asm__ volatile ("wfi");
    }
}

int thread_create(void (*entry)(void), unsigned long *vm) {
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

    unsigned long *frame = (unsigned long *)(stack + PAGE_SIZE - THREAD_FRAME_SIZE);

    frame[0] = (unsigned long)thread_entry;
    frame[1] = (unsigned long)entry;

    struct thread *t = &thread_table[id];

    t->sp = (unsigned long)frame;
    t->chan = 0;
    t->vm = vm;
    t->stack = stack;

    return id;
}

void thread_start(void) {
    unsigned long unused;

    if (thread_create(thread_idle, 0) < 0) {
        panic("cannot create the idle thread");
    }

    vm_switch(thread_table[0].vm);
    thread_switch(&unused, thread_table[0].sp);
}

void thread_yield(void) {
    int prev = thread_current;

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

void thread_exit(void) {
    thread_wakeup(&thread_exit_wait);
    thread_sleep(&thread_dead);
}

int thread_wait(int id) {
    if (id < 0 || id >= THREAD_MAX || thread_table[id].stack == 0) {
        return -ERR_CHILD;
    }

    struct thread *t = &thread_table[id];

    while (t->chan != &thread_dead) {
        thread_sleep(&thread_exit_wait);
    }

    vm_free(t->vm);
    page_free(t->stack);
    t->stack = 0;

    return 0;
}
