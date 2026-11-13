#include "thread.h"
#include "page.h"
#include "vm.h"

#define THREAD_MAX        8
#define THREAD_FRAME_SIZE 112

void thread_switch(unsigned long *old_sp, unsigned long new_sp);
void thread_entry(void);

static unsigned long thread_sp[THREAD_MAX];
static void *thread_chan[THREAD_MAX];
static unsigned long *thread_vm[THREAD_MAX];
static void *thread_stack[THREAD_MAX];
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

    while (id < THREAD_MAX && thread_stack[id] != 0) {
        id++;
    }

    if (id == THREAD_MAX) {
        return -1;
    }

    unsigned char *stack = page_alloc();
    unsigned long *frame = (unsigned long *)(stack + PAGE_SIZE - THREAD_FRAME_SIZE);

    frame[0] = (unsigned long)thread_entry;
    frame[1] = (unsigned long)entry;

    thread_sp[id] = (unsigned long)frame;
    thread_chan[id] = 0;
    thread_vm[id] = vm;
    thread_stack[id] = stack;

    return id;
}

void thread_start(void) {
    unsigned long unused;

    thread_create(thread_idle, 0);

    vm_switch(thread_vm[0]);
    thread_switch(&unused, thread_sp[0]);
}

void thread_yield(void) {
    int prev = thread_current;

    do {
        thread_current++;

        if (thread_current == THREAD_MAX) {
            thread_current = 0;
        }
    } while (thread_stack[thread_current] == 0 || thread_chan[thread_current] != 0);

    if (thread_current == prev) {
        return;
    }

    vm_switch(thread_vm[thread_current]);
    thread_switch(&thread_sp[prev], thread_sp[thread_current]);
}

void thread_sleep(void *chan) {
    thread_chan[thread_current] = chan;
    thread_yield();
}

void thread_wakeup(void *chan) {
    for (int i = 0; i < THREAD_MAX; i++) {
        if (thread_chan[i] == chan) {
            thread_chan[i] = 0;
        }
    }
}

void thread_exit(void) {
    thread_wakeup(&thread_exit_wait);
    thread_sleep(&thread_dead);
}

int thread_wait(int id) {
    if (id < 0 || id >= THREAD_MAX || thread_stack[id] == 0) {
        return -1;
    }

    while (thread_chan[id] != &thread_dead) {
        thread_sleep(&thread_exit_wait);
    }

    vm_free(thread_vm[id]);
    page_free(thread_stack[id]);
    thread_stack[id] = 0;

    return 0;
}
