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

#define csr_clear(csr, bits) ({                                              \
    __asm__ volatile ("csrc " #csr ", %0" : : "r"((unsigned long)(bits))); \
})

#define MSTATUS_MPP (3UL << 11)
#define MSTATUS_MIE (1UL << 3)
#define MIE_MTIE    (1UL << 7)
#define MIE_MEIE    (1UL << 11)

#define MCAUSE_INTERRUPT        (1UL << 63)
#define MCAUSE_BREAKPOINT       3
#define MCAUSE_MACHINE_TIMER    (MCAUSE_INTERRUPT | 7)
#define MCAUSE_MACHINE_EXTERNAL (MCAUSE_INTERRUPT | 11)
#define MCAUSE_USER_ECALL       8

#define PMP_R   (1UL << 0)
#define PMP_W   (1UL << 1)
#define PMP_X   (1UL << 2)
#define PMP_TOR (1UL << 3)
