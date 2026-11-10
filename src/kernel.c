#include "kprintf.h"
#include "page.h"
#include "plic.h"
#include "proc.h"
#include "thread.h"
#include "timer.h"
#include "uart.h"
#include "virt.h"
#include "vm.h"

extern char __kernel_end[];

extern char user_hello[], user_hello_end[];
extern char user_count[], user_count_end[];
extern char user_shell[], user_shell_end[];

void kernel_main(unsigned long hartid, unsigned long dtb) {
    kprintf("Hello World from Arblem :)\n");

    kprintf("HartID: %x\n", hartid);
    kprintf("DTB: %x\n", dtb);

    page_init((unsigned long)__kernel_end, dtb);
    vm_init(dtb);

    uart_init();
    plic_init();
    plic_enable(VIRT_UART0_IRQ);
    timer_init();

    proc_create(user_hello, user_hello_end - user_hello);
    proc_create(user_count, user_count_end - user_count);
    proc_create(user_count, user_count_end - user_count);
    proc_create(user_shell, user_shell_end - user_shell);
    thread_start();
}
