#pragma once

unsigned long sbi_spec_version(void);
void sbi_shutdown(unsigned long reason);
long sbi_console_write(const char *buf, unsigned long len);
