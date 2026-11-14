#include "kprintf.h"
#include "panic.h"
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
    unsigned long cause = csr_read(scause);
    unsigned long epc = csr_read(sepc);
    unsigned long status = csr_read(sstatus);
    unsigned long tval = csr_read(stval);

    if (cause & SCAUSE_INTERRUPT) {
        if (cause == SCAUSE_SUPERVISOR_TIMER) {
            timer_interrupt();
        }

        if (cause == SCAUSE_SUPERVISOR_EXTERNAL) {
            unsigned int irq = plic_claim();

            if (irq == VIRT_UART0_IRQ) {
                uart_interrupt();
            }

            plic_complete(irq);
        }

        thread_yield();
        csr_write(sepc, epc);
        csr_write(sstatus, status);
        return;
    }

    if (cause == EXC_USER_ECALL) {
        f->a0 = syscall_handle(f->a7, f->a0, f->a1, f->a2);
        csr_write(sepc, epc + 4);
        csr_write(sstatus, status);
        return;
    }

    kprintf("Trap!\n");
    kprintf("scause: %x\n", cause);
    kprintf("sepc: %x\n", epc);
    kprintf("stval: %x\n", tval);

    if (cause == EXC_BREAKPOINT && (status & SSTATUS_SPP)) {
        unsigned short insn = *(volatile unsigned short *)epc;
        epc += ((insn & 3) == 3) ? 4 : 2;
        csr_write(sepc, epc);
        return;
    }

    if ((status & SSTATUS_SPP) == 0) {
        thread_exit(-1);
    }

    panic("unexpected trap in kernel");
}
