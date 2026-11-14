#include "sbi.h"

#define SBI_SUCCESS 0
#define SBI_EXT_BASE 0x10
#define SBI_EXT_SRST 0x53525354
#define SBI_EXT_DBCN 0x4442434e

#define SBI_BASE_GET_SPEC_VERSION 0
#define SBI_SRST_SYSTEM_RESET     0
#define SBI_SRST_TYPE_SHUTDOWN    0
#define SBI_DBCN_WRITE            0

struct sbi_ret {
    long error;
    unsigned long value;
};

static struct sbi_ret sbi_call(unsigned long eid, unsigned long fid, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    register unsigned long a0 __asm__("a0") = arg0;
    register unsigned long a1 __asm__("a1") = arg1;
    register unsigned long a2 __asm__("a2") = arg2;
    register unsigned long a6 __asm__("a6") = fid;
    register unsigned long a7 __asm__("a7") = eid;

    __asm__ volatile ("ecall" : "+r"(a0), "+r"(a1) : "r"(a2), "r"(a6), "r"(a7) : "memory");

    return (struct sbi_ret){a0, a1};
}

unsigned long sbi_spec_version(void) {
    struct sbi_ret ret = sbi_call(SBI_EXT_BASE, SBI_BASE_GET_SPEC_VERSION, 0, 0, 0);

    return ret.error == SBI_SUCCESS ? ret.value : 0;
}

void sbi_shutdown(unsigned long reason) {
    sbi_call(SBI_EXT_SRST, SBI_SRST_SYSTEM_RESET, SBI_SRST_TYPE_SHUTDOWN, reason, 0);
}

long sbi_console_write(const char *buf, unsigned long len) {
    while (len > 0) {
        struct sbi_ret ret = sbi_call(SBI_EXT_DBCN, SBI_DBCN_WRITE, len, (unsigned long)buf, 0);

        if (ret.error != SBI_SUCCESS) {
            return ret.error;
        }

        buf += ret.value;
        len -= ret.value;
    }

    return SBI_SUCCESS;
}
