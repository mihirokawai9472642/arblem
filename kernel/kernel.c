#include "console.h"
#include "fd.h"
#include "kprintf.h"
#include "page.h"
#include "panic.h"
#include "plic.h"
#include "proc.h"
#include "sbi.h"
#include "thread.h"
#include "timer.h"
#include "uart.h"
#include "virt.h"
#include "vm.h"

extern char __kernel_end[];

void kernel_main(unsigned long hartid, unsigned long dtb) {
    kprintf("Hello World from Arblem :)\n");

    kprintf("HartID: %x\n", hartid);
    kprintf("DTB: %x\n", dtb);
    kprintf("SBI: %x\n", sbi_spec_version());

    page_init((unsigned long)__kernel_end, dtb);
    vm_init(dtb);

    uart_init();
    plic_init();
    plic_enable(VIRT_UART0_IRQ);
    timer_init();

    int shell = proc_spawn("shell");

    if (shell < 0) {
        panic("cannot start the shell");
    }

    thread_set_file(shell, FD_STDIN, console_file());
    thread_set_file(shell, FD_STDOUT, console_file());
    thread_set_file(shell, FD_STDERR, console_file());

    thread_start();
}
