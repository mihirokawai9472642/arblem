#include "kprintf.h"
#include "riscv.h"
#include "timer.h"

void kernel_main(unsigned long hartid, unsigned long dtb) {
    kprintf("Hello World from Arblem :)\n");

    kprintf("HartID: %x\n", hartid);
    kprintf("DTB: %x\n", dtb);

    timer_init();
    csr_set(mstatus, MSTATUS_MIE);
}
