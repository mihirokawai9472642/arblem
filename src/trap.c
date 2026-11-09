#include "kprintf.h"
#include "plic.h"
#include "riscv.h"
#include "syscall.h"
#include "thread.h"
#include "timer.h"
#include "uart.h"
#include "virt.h"

struct trap_frame {
    unsigned long ra;
    unsigned long t0, t1, t2, t3, t4, t5, t6;
    unsigned long a0, a1, a2, a3, a4, a5, a6, a7;
    unsigned long sp;
};

void trap_handler(struct trap_frame *f) {
    unsigned long cause = csr_read(mcause);
    unsigned long epc = csr_read(mepc);
    unsigned long status = csr_read(mstatus);
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

    if (cause == MCAUSE_USER_ECALL) {
        f->a0 = syscall_handle(f->a7, f->a0);
        csr_write(mepc, epc + 4);
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

    if ((status & MSTATUS_MPP) == 0) {
        thread_exit();
    }

    for (;;) {
        __asm__ volatile ("wfi");
    }
}
