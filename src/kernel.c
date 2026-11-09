#include "kprintf.h"
#include "plic.h"
#include "pmp.h"
#include "shell.h"
#include "thread.h"
#include "timer.h"
#include "trap.h"
#include "uart.h"
#include "virt.h"

extern char __user_start[];
extern char __user_end[];
extern char __user_stack_top[];

void user_main(void);

static void kernel_user_thread(void) {
    trap_enter_user((unsigned long)user_main, (unsigned long)__user_stack_top);
}

void kernel_main(unsigned long hartid, unsigned long dtb) {
    kprintf("Hello World from Arblem :)\n");

    kprintf("HartID: %x\n", hartid);
    kprintf("DTB: %x\n", dtb);

    uart_init();
    plic_init();
    plic_enable(VIRT_UART0_IRQ);
    pmp_init((unsigned long)__user_start, (unsigned long)__user_end);
    timer_init();

    thread_create(kernel_user_thread);
    thread_create(shell_main);
    thread_start();
}
