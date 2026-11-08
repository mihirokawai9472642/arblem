#include "thread.h"

#define THREAD_MAX        4
#define THREAD_STACK_SIZE 4096
#define THREAD_FRAME_SIZE 112

void thread_switch(unsigned long *old_sp, unsigned long new_sp);

static unsigned char thread_stacks[THREAD_MAX][THREAD_STACK_SIZE];
__attribute__((aligned(16)));
static unsigned long thread_sp[THREAD_MAX];
static int thread_count;
static int thread_current;

void thread_create(void (*entry)(void)) {
    unsigned char *top = thread_stacks[thread_count] + THREAD_STACK_SIZE;
    unsigned long *frame = (unsigned long *)(top - THREAD_FRAME_SIZE);

    frame[0] = (unsigned long)entry;

    thread_sp[thread_count] = (unsigned long)frame;

    thread_count++;
}

void thread_start(void) {
    unsigned long unused;

    thread_switch(&unused, thread_sp[0]);
}

void thread_yield(void) {
    int prev = thread_current;

    thread_current++;

    if (thread_current == thread_count) {
        thread_current = 0;
    }

    if (thread_current == prev) {
        return;
    }

    thread_switch(&thread_sp[prev], thread_sp[thread_current]);
}
