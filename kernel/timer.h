#pragma once

// Set the timebase frequency (Hz) on the boot hart, arm its first timer
// interrupt, and enable timer interrupts (sie.STIE).
void timer_init(unsigned long timebase_freq);

// Arm the first timer interrupt and enable sie.STIE on the current hart.
void timer_init_hart(void);

// Called by trap_handler on each timer interrupt. Arms the next one.
void timer_interrupt(void);

// Timer interrupts since boot, at 100 per second.
unsigned long timer_ticks(void);
