#include "timer.h"
#include "riscv.h"
#include "virt.h"

#define TIMER_INTERVAL (VIRT_TIMEBASE_FREQ / 100)

static unsigned long timer_tick_count;

static void timer_set_next(void) {
    csr_write(stimecmp, csr_read(time) + TIMER_INTERVAL);
}

void timer_init(void) {
    timer_set_next();
    csr_set(sie, SIE_STIE);
}

void timer_interrupt(void) {
    timer_tick_count++;
    timer_set_next();
}

unsigned long timer_ticks(void) {
    return timer_tick_count;
}
