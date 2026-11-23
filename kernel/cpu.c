// Per-hart CPU state. Each hart owns one entry in cpus[], indexed by hartid,
// and keeps a pointer to it in the tp register whenever it is in S-mode.

#include "cpu.h"
#include "panic.h"
#include <stddef.h>

_Static_assert(offsetof(struct cpu, kernel_sp) == CPU_KERNEL_SP,
               "CPU_KERNEL_SP must match struct cpu");
_Static_assert(offsetof(struct cpu, user_sp) == CPU_USER_SP,
               "CPU_USER_SP must match struct cpu");

static struct cpu cpus[CPU_MAX];

void cpu_init(unsigned long hartid) {
    if (hartid >= CPU_MAX) {
        panic("hartid out of range");
    }

    struct cpu *c = &cpus[hartid];

    c->hartid = hartid;
    __asm__ volatile("mv tp, %0" : : "r"(c) : "memory");
}
