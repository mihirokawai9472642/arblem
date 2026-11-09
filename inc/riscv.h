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

#define SATP_SV39 (8UL << 60)

#define PTE_V (1UL << 0)
#define PTE_R (1UL << 1)
#define PTE_W (1UL << 2)
#define PTE_X (1UL << 3)
#define PTE_U (1UL << 4)
#define PTE_A (1UL << 6)
#define PTE_D (1UL << 7)
