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
static int thread_count;
static int thread_current;
static char thread_dead;

static void thread_idle(void) {
    for (;;) {
        __asm__ volatile ("wfi");
    }
}

void thread_create(void (*entry)(void), unsigned long *vm) {
    unsigned char *top = (unsigned char *)page_alloc() + PAGE_SIZE;
    unsigned long *frame = (unsigned long *)(top - THREAD_FRAME_SIZE);

    frame[0] = (unsigned long)thread_entry;
    frame[1] = (unsigned long)entry;

    thread_sp[thread_count] = (unsigned long)frame;
    thread_vm[thread_count] = vm;

    thread_count++;
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

        if (thread_current == thread_count) {
            thread_current = 0;
        }
    } while (thread_chan[thread_current] != 0);

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
    for (int i = 0; i < thread_count; i++) {
        if (thread_chan[i] == chan) {
            thread_chan[i] = 0;
        }
    }
}

void thread_exit(void) {
    thread_sleep(&thread_dead);
}
