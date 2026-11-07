#include "timer.h"
#include "kprintf.h"
#include "riscv.h"
#include "virt.h"

#define CLINT_MTIMECMP ((volatile unsigned long *)(VIRT_CLINT + 0x4000))
#define CLINT_MTIME    ((volatile unsigned long *)(VIRT_CLINT + 0xbff8))

#define TIMER_INTERVAL VIRT_TIMEBASE_FREQ

static void timer_set_next(void) {
    *CLINT_MTIMECMP = *CLINT_MTIME + TIMER_INTERVAL;
}

void timer_init(void) {
    timer_set_next();
    csr_set(mie, MIE_MTIE);
}

void timer_interrupt(void) {
    kprintf("Tick: %x\n", *CLINT_MTIME);
    timer_set_next();
}
