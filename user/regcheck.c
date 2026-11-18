// Check that gp and tp keep their values while another program changes them,
// and that a new program starts with them cleared. Prints "regs: ok" when both
// hold. The smoke test runs this.

#include "sys.h"

#define REGCHECK_GP 0x1111
#define REGCHECK_TP 0x2222

int main(void) {
    unsigned long gp;
    unsigned long tp;

    __asm__ volatile ("mv gp, %0" : : "r"(REGCHECK_GP));
    __asm__ volatile ("mv tp, %0" : : "r"(REGCHECK_TP));

    long pid = sys_spawn("regclobber", str_len("regclobber"));

    if (pid < 0) {
        sys_puts("regs: cannot start regclobber\n");

        return 1;
    }

    // Spin long enough for the timer to switch to regclobber many times.
    for (volatile unsigned long i = 0; i < 30000000; i++) {
    }

    __asm__ volatile ("mv %0, gp" : "=r"(gp));
    __asm__ volatile ("mv %0, tp" : "=r"(tp));

    int status = -1;

    sys_wait(pid, &status);

    if (gp != REGCHECK_GP || tp != REGCHECK_TP) {
        sys_puts("regs: clobbered\n");
    } else if (status != 0) {
        sys_puts("regs: inherited\n");
    } else {
        sys_puts("regs: ok\n");
    }

    return 0;
}
