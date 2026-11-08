#include "kprintf.h"
#include "plic.h"
#include "riscv.h"
#include "thread.h"
#include "timer.h"
#include "uart.h"
#include "virt.h"

void trap_handler(void) {
    unsigned long cause = csr_read(mcause);
    unsigned long epc = csr_read(mepc);
    unsigned long status =csr_read(mstatus);
    unsigned long tval = csr_read(mtval);

    if (cause & MCAUSE_INTERRUPT) {
        if (cause == MCAUSE_MACHINE_TIMER) {
            timer_interrupt();
        }

        if (cause == MCAUSE_MACHINE_EXTERNAL) {
            unsigned int irq = plic_claim();

            if (irq == VIRT_UART0_IRQ) {
                uart_interrupt();
            }

            plic_complete(irq);
        }

        thread_yield();
        csr_write(mepc, epc);
        csr_write(mstatus, status);
        return;
    }

    kprintf("Trap!\n");
    kprintf("mcause: %x\n", cause);
    kprintf("mepc: %x\n", epc);
    kprintf("mtval: %x\n", tval);

    if (cause == MCAUSE_BREAKPOINT) {
        unsigned short insn = *(volatile unsigned short *)epc;
        epc += ((insn & 3) == 3) ? 4 : 2;
        csr_write(mepc, epc);
        return;
    }

    for (;;) {
        __asm__ volatile ("wfi");
    }
}
