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

#define SSTATUS_SIE (1UL << 1)
#define SSTATUS_SPP (1UL << 8)
#define SIE_STIE    (1UL << 5)
#define SIE_SEIE    (1UL << 9)

#define SCAUSE_INTERRUPT           (1UL << 63)
#define SCAUSE_BREAKPOINT          3
#define SCAUSE_SUPERVISOR_TIMER    (SCAUSE_INTERRUPT | 5)
#define SCAUSE_SUPERVISOR_EXTERNAL (SCAUSE_INTERRUPT | 9)
#define SCAUSE_USER_ECALL          8

#define PMP_R   (1UL << 0)
#define PMP_W   (1UL << 1)
#define PMP_X   (1UL << 2)
#define PMP_TOR (1UL << 3)
