// Calls into OpenSBI, the M-mode firmware. See docs/devices.md#sbi.

#include "sbi.h"

#define SBI_SUCCESS 0

// Extension IDs. SRST and DBCN are their names in ASCII.
#define SBI_EXT_BASE 0x10
#define SBI_EXT_SRST 0x53525354 // "SRST": System Reset
#define SBI_EXT_DBCN 0x4442434e // "DBCN": Debug Console
#define SBI_EXT_HSM  0x48534d   // "HSM": Hart State Management

// Function IDs within those extensions.
#define SBI_BASE_GET_SPEC_VERSION 0
#define SBI_SRST_SYSTEM_RESET     0
#define SBI_SRST_TYPE_SHUTDOWN    0
#define SBI_DBCN_WRITE            0
#define SBI_HSM_HART_START        0

// Every SBI call returns an error code in a0 and a value in a1.
struct sbi_ret {
    long error;
    unsigned long value;
};

// ecall from S-mode traps into OpenSBI: a7 = extension, a6 = function,
// a0-a2 = arguments.
static struct sbi_ret sbi_call(unsigned long eid, unsigned long fid,
                               unsigned long arg0, unsigned long arg1,
                               unsigned long arg2) {
    register unsigned long a0 __asm__("a0") = arg0;
    register unsigned long a1 __asm__("a1") = arg1;
    register unsigned long a2 __asm__("a2") = arg2;
    register unsigned long a6 __asm__("a6") = fid;
    register unsigned long a7 __asm__("a7") = eid;

    __asm__ volatile("ecall"
                     : "+r"(a0), "+r"(a1)
                     : "r"(a2), "r"(a6), "r"(a7)
                     : "memory");

    return (struct sbi_ret){(long)a0, a1};
}

unsigned long sbi_spec_version(void) {
    struct sbi_ret ret =
        sbi_call(SBI_EXT_BASE, SBI_BASE_GET_SPEC_VERSION, 0, 0, 0);

    return ret.error == SBI_SUCCESS ? ret.value : 0;
}

void sbi_shutdown(unsigned long reason) {
    sbi_call(SBI_EXT_SRST, SBI_SRST_SYSTEM_RESET, SBI_SRST_TYPE_SHUTDOWN,
             reason, 0);
}

// DBCN takes the buffer's physical address (low and high halves). Kernel
// memory is identity-mapped, so the pointer value is the physical address.
// The firmware may write fewer bytes than asked; loop until all are out.
long sbi_console_write(const char *buf, unsigned long len) {
    while (len > 0) {
        struct sbi_ret ret =
            sbi_call(SBI_EXT_DBCN, SBI_DBCN_WRITE, len, (unsigned long)buf, 0);

        if (ret.error != SBI_SUCCESS) {
            return ret.error;
        }

        buf += ret.value;
        len -= ret.value;
    }

    return SBI_SUCCESS;
}

long sbi_hart_start(unsigned long hartid, unsigned long start_addr,
                    unsigned long opaque) {
    struct sbi_ret ret =
        sbi_call(SBI_EXT_HSM, SBI_HSM_HART_START, hartid, start_addr, opaque);

    return ret.error;
}
