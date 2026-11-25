#pragma once

// Print a kernel message. Formats: %x takes an unsigned long and prints it as
// 0x plus 16 hex digits; %s takes a string.
void kprintf(const char *f, ...);

// Disable kprintf's internal spinlock before printing a panic message, so a
// panic cannot deadlock if another hart holds the lock.
void kprintf_disable_lock(void);
