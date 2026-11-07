#pragma once

#define csr_read(csr) ({                             \
    unsigned long __v;                               \
    __asm__ volatile ("csrr %0, " #csr : "=r"(__v)); \
    __v;                                             \
})

#define csr_write(csr, val) ({                                            \
    __asm__ volatile ("csrw " #csr ", %0" : : "r"((unsigned long)(val))); \
})

#define csr_set(csr, bits) ({                                              \
    __asm__ volatile ("csrs " #csr ", %0" : : "r"((unsigned long)(bits))); \
})

#define MSTATUS_MIE (1UL << 3)
#define MIE_MTIE    (1UL << 7)

#define MCAUSE_INTERRUPT     (1UL << 63)
#define MCAUSE_BREAKPOINT    3
#define MCAUSE_MACHINE_TIMER (MCAUSE_INTERRUPT | 7)
