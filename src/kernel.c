#include "kprintf.h"
#include "thread.h"
#include "timer.h"

static void delay(void) {
    for (volatile unsigned long i = 0; i < 30000000; i++) {
    }
}

static void thread_a(void) {
    for (;;) {
        kprintf("A\n");
        delay();
    }
}

static void thread_b(void) {
    for (;;) {
        kprintf("B\n");
        delay();
    }
}

void kernel_main(unsigned long hartid, unsigned long dtb) {
    kprintf("Hello World from Arblem :)\n");

    kprintf("HartID: %x\n", hartid);
    kprintf("DTB: %x\n", dtb);

    timer_init();

    thread_create(thread_a);
    thread_create(thread_b);
    thread_start();
}
