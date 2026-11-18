#pragma once

// Arm the first timer interrupt and enable timer interrupts (sie.STIE).
void timer_init(void);

// Called by trap_handler on each timer interrupt. Arms the next one.
void timer_interrupt(void);

// Timer interrupts since boot, at 100 per second.
unsigned long timer_ticks(void);
