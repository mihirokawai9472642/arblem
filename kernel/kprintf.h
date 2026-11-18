#pragma once

// Print a kernel message. Formats: %x takes an unsigned long and prints it as
// 0x plus 16 hex digits; %s takes a string.
void kprintf(const char *f, ...);
