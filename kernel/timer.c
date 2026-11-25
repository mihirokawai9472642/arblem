// Periodic timer interrupt, programmed through the stimecmp CSR (Sstc
// extension) without SBI calls. See docs/devices.md.

#include "timer.h"
#include "cpu.h"
#include "riscv.h"

// Ticks of the time CSR between interrupts: 100 interrupts per second.
static unsigned long timer_interval;
static unsigned long timer_boot_hart;
static unsigned long timer_tick_count;

// A timer interrupt is pending while time >= stimecmp, so moving stimecmp
// into the future also clears the current one.
static void timer_set_next(void) {
    csr_write(stimecmp, csr_read(time) + timer_interval);
}

void timer_init_hart(void) {
    timer_set_next();
    csr_set(sie, SIE_STIE);
}

void timer_init(unsigned long timebase_freq) {
    timer_interval = timebase_freq / 100;
    timer_boot_hart = cpu_current()->hartid;
    timer_init_hart();
}

void timer_interrupt(void) {
    // Only the boot hart advances timer_tick_count so ticks stay at 100 Hz
    // regardless of how many harts are running.
    if (cpu_current()->hartid == timer_boot_hart) {
        __atomic_add_fetch(&timer_tick_count, 1, __ATOMIC_RELAXED);
    }

    timer_set_next();
}

unsigned long timer_ticks(void) {
    return __atomic_load_n(&timer_tick_count, __ATOMIC_RELAXED);
}
