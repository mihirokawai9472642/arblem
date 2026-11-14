#pragma once

#define SBI_SRST_REASON_NONE    0
#define SBI_SRST_REASON_FAILURE 1

unsigned long sbi_spec_version(void);
void sbi_shutdown(unsigned long reason);
long sbi_console_write(const char *buf, unsigned long len);
