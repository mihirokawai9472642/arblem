#include "kprintf.h"
#include "plic.h"
#include "shell.h"
#include "thread.h"
#include "timer.h"
#include "uart.h"
#include "virt.h"

void kernel_main(unsigned long hartid, unsigned long dtb) {
    kprintf("Hello World from Arblem :)\n");

    kprintf("HartID: %x\n", hartid);
    kprintf("DTB: %x\n", dtb);

    uart_init();
    plic_init();
    plic_enable(VIRT_UART0_IRQ);
    timer_init();

    thread_create(shell_main);
    thread_start();
}
