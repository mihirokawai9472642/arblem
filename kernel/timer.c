// Periodic timer interrupt, programmed through the stimecmp CSR (Sstc
// extension) without SBI calls. See docs/devices.md.

#include "timer.h"
#include "riscv.h"
#include "virt.h"

// Ticks of the time CSR between interrupts: 100 interrupts per second.
#define TIMER_INTERVAL (VIRT_TIMEBASE_FREQ / 100)

static unsigned long timer_tick_count;

// A timer interrupt is pending while time >= stimecmp, so moving stimecmp
// into the future also clears the current one.
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
