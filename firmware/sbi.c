#include "firmware.h"
#include "riscv.h"
#include "sbinum.h"
#include "virt.h"

#define SBI_SPEC_VERSION (2UL << 24)

struct sbi_frame {
    unsigned long ra;
    unsigned long t0, t1, t2, t3, t4, t5, t6;
    unsigned long a0, a1, a2, a3, a4, a5, a6, a7;
};

__attribute__((noreturn)) static void sbi_halt(void) {
    for (;;) {
        __asm__ volatile ("wfi");
    }
}

static long sbi_base(unsigned long fid, unsigned long *value) {
    if (fid == SBI_BASE_GET_SPEC_VERSION) {
        *value = SBI_SPEC_VERSION;
        return SBI_SUCCESS;
    }

    return SBI_ERR_NOT_SUPPORTED;
}

static long sbi_srst(unsigned long fid, unsigned long type, unsigned long reason) {
    if (fid != SBI_SRST_SYSTEM_RESET) {
        return SBI_ERR_NOT_SUPPORTED;
    }

    if (type > SBI_SRST_TYPE_WARM_REBOOT) {
        return SBI_ERR_INVALID_PARAM;
    }

    if (type != SBI_SRST_TYPE_SHUTDOWN) {
        return SBI_ERR_NOT_SUPPORTED;
    }

    if (reason == SBI_SRST_REASON_NONE) {
        *(volatile unsigned int *)VIRT_TEST = VIRT_TEST_PASS;
    } else {
        *(volatile unsigned int *)VIRT_TEST = VIRT_TEST_FAIL | (1 << 16);
    }

    sbi_halt();
}

static void sbi_putc(char c) {
    *(volatile unsigned char *)VIRT_UART0 = c;
}

static long sbi_dbcn(unsigned long fid, unsigned long arg0, unsigned long arg1, unsigned long arg2, unsigned long *value) {
    if (fid == SBI_DBCN_WRITE_BYTE) {
        sbi_putc(arg0);
        return SBI_SUCCESS;
    }

    if (fid != SBI_DBCN_WRITE) {
        return SBI_ERR_NOT_SUPPORTED;
    }

    unsigned long len = arg0;
    unsigned long addr = arg1;

    if (arg2 != 0 || addr < KERNEL_BASE || addr + len < addr) {
        return SBI_ERR_INVALID_PARAM;
    }

    for (unsigned long i = 0; i < len; i++) {
        sbi_putc(((const char *)addr)[i]);
    }

    *value = len;

    return SBI_SUCCESS;
}

void sbi_handler(struct sbi_frame *f) {
    if (csr_read(mcause) != EXC_SUPERVISOR_ECALL) {
        sbi_halt();
    }

    unsigned long value = 0;
    long error;

    if (f->a7 == SBI_EXT_BASE) {
        error = sbi_base(f->a6, &value);
    } else if (f->a7 == SBI_EXT_SRST) {
        error = sbi_srst(f->a6, f->a0, f->a1);
    } else if (f->a7 == SBI_EXT_DBCN) {
        error = sbi_dbcn(f->a6, f->a0, f->a1, f->a2, &value);
    } else {
        error = SBI_ERR_NOT_SUPPORTED;
    }

    f->a0 = error;
    f->a1 = value;

    csr_write(mepc, csr_read(mepc) + 4);
}
