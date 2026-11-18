#pragma once

// Reasons for sbi_shutdown.
#define SBI_SRST_REASON_NONE    0
#define SBI_SRST_REASON_FAILURE 1

// SBI specification version (major in bits 30:24, minor in bits 23:0), or 0
// if the call fails.
unsigned long sbi_spec_version(void);

// Power off the machine. Returns only if the firmware refuses.
void sbi_shutdown(unsigned long reason);

// Write len bytes from buf (a kernel address) to the firmware's console.
// Returns 0, or a negative SBI error code.
long sbi_console_write(const char *buf, unsigned long len);
