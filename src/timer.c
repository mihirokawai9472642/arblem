#include "timer.h"
#include "riscv.h"
#include "virt.h"

#define CLINT_MTIMECMP ((volatile unsigned long *)(VIRT_CLINT + 0x4000))
#define CLINT_MTIME    ((volatile unsigned long *)(VIRT_CLINT + 0xbff8))

#define TIMER_INTERVAL (VIRT_TIMEBASE_FREQ / 100)

static unsigned long timer_tick_count;

static void timer_set_next(void) {
    *CLINT_MTIMECMP = *CLINT_MTIME + TIMER_INTERVAL;
}

void timer_init(void) {
    timer_set_next();
    csr_set(mie, MIE_MTIE);
}

void timer_interrupt(void) {
    timer_tick_count++;
    timer_set_next();
}

unsigned long timer_ticks(void) {
    return timer_tick_count;
}
