#include "kprintf.h"
#include "shell.h"
#include "thread.h"
#include "timer.h"

void kernel_main(unsigned long hartid, unsigned long dtb) {
    kprintf("Hello World from Arblem :)\n");

    kprintf("HartID: %x\n", hartid);
    kprintf("DTB: %x\n", dtb);

    timer_init();

    thread_create(shell_main);
    thread_start();
}
