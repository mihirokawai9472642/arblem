// Trap dispatch: interrupts, system calls and exceptions. trap_entry in
// trap.S saves registers and calls trap_handler. See docs/traps.md.

#include "kprintf.h"
#include "panic.h"
#include "plic.h"
#include "riscv.h"
#include "syscall.h"
#include "thread.h"
#include "timer.h"
#include "uart.h"
#include "virt.h"

// Registers saved by trap_entry, at the offsets trap.S uses. sp is the stack
// pointer of the interrupted code. gp and tp belong to the user program: the
// kernel never uses them, but another program would overwrite them.
struct trap_frame {
    unsigned long ra;
    unsigned long t0, t1, t2, t3, t4, t5, t6;
    unsigned long a0, a1, a2, a3, a4, a5, a6, a7;
    unsigned long sp;
    unsigned long gp, tp;
};

void trap_handler(struct trap_frame *f) {
    // These CSRs belong to the hart. If this trap switches threads, the next
    // thread's traps overwrite them, so keep copies on this kernel stack and
    // write sepc and sstatus back before returning.
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

        // Preemption: every interrupt gives the next thread a turn.
        thread_yield();
        csr_write(sepc, epc);
        csr_write(sstatus, status);
        return;
    }

    if (cause == EXC_USER_ECALL) {
        // The return value goes into the saved a0, and the program resumes
        // after the 4-byte ecall instruction.
        f->a0 = syscall_handle(f->a7, f->a0, f->a1, f->a2);
        csr_write(sepc, epc + 4);
        csr_write(sstatus, status);
        return;
    }

    // Demand paging for U-mode load/store page faults (heap and stack).
    // Execution faults (EXC_INSN_PAGE_FAULT) are never handled here (W^X).
    if ((status & SSTATUS_SPP) == 0 &&
        (cause == EXC_LOAD_PAGE_FAULT || cause == EXC_STORE_PAGE_FAULT) &&
        thread_fault(tval) == 0) {
        csr_write(sepc, epc);
        csr_write(sstatus, status);
        return;
    }

    kprintf("Trap!\n");
    kprintf("scause: %x\n", cause);
    kprintf("sepc: %x\n", epc);
    kprintf("stval: %x\n", tval);

    // An ebreak in kernel code is a debugging marker: report it and continue
    // with the next instruction. Low bits 11 mean a 32-bit instruction;
    // anything else is a 16-bit compressed one.
    if (cause == EXC_BREAKPOINT && (status & SSTATUS_SPP)) {
        unsigned short insn = *(volatile unsigned short *)epc;
        epc += ((insn & 3) == 3) ? 4 : 2;
        csr_write(sepc, epc);
        return;
    }

    // Any other exception from U-mode ends the program.
    if ((status & SSTATUS_SPP) == 0) {
        thread_exit(-1);
    }

    panic("unexpected trap in kernel");
}
