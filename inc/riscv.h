#pragma once

#define csr_read(csr) ({                             \
    unsigned long __v;                               \
    __asm__ volatile ("csrr %0, " #csr : "=r"(__v)); \
    __v;                                             \
})

#define csr_write(csr, val) ({                                            \
    __asm__ volatile ("csrw " #csr ", %0" : : "r"((unsigned long)(val)) : "memory"); \
})

#define csr_set(csr, bits) ({                                              \
    __asm__ volatile ("csrs " #csr ", %0" : : "r"((unsigned long)(bits)) : "memory"); \
})

#define csr_clear(csr, bits) ({                                              \
    __asm__ volatile ("csrc " #csr ", %0" : : "r"((unsigned long)(bits)) : "memory"); \
})

#define EXC_INSN_MISALIGNED  0
#define EXC_INSN_ACCESS      1
#define EXC_ILLEGAL_INSN     2
#define EXC_BREAKPOINT       3
#define EXC_LOAD_MISALIGNED  4
#define EXC_LOAD_ACCESS      5
#define EXC_STORE_MISALIGNED 6
#define EXC_STORE_ACCESS     7
#define EXC_USER_ECALL       8
#define EXC_SUPERVISOR_ECALL 9
#define EXC_INSN_PAGE_FAULT  12
#define EXC_LOAD_PAGE_FAULT  13
#define EXC_STORE_PAGE_FAULT 15

#define IRQ_S_SOFT  1
#define IRQ_S_TIMER 5
#define IRQ_S_EXT   9

#define MSTATUS_MPP   (3UL << 11)
#define MSTATUS_MPP_S (1UL << 11)

#define MCOUNTEREN_TM (1UL << 1)

#define MENVCFG_STCE (1UL << 63)

#define PMP_R     (1UL << 0)
#define PMP_W     (1UL << 1)
#define PMP_X     (1UL << 2)
#define PMP_NAPOT (3UL << 3)

#define PMP_NAPOT_ADDR(base, size) (((base) >> 2) | (((size) >> 3) - 1))

#define SSTATUS_SIE (1UL << 1)
#define SSTATUS_SPP (1UL << 8)
#define SSTATUS_SUM (1UL << 18)
#define SIE_STIE    (1UL << IRQ_S_TIMER)
#define SIE_SEIE    (1UL << IRQ_S_EXT)

#define SCAUSE_INTERRUPT           (1UL << 63)
#define SCAUSE_SUPERVISOR_TIMER    (SCAUSE_INTERRUPT | IRQ_S_TIMER)
#define SCAUSE_SUPERVISOR_EXTERNAL (SCAUSE_INTERRUPT | IRQ_S_EXT)

#define SATP_SV39 (8UL << 60)

#define PTE_V (1UL << 0)
#define PTE_R (1UL << 1)
#define PTE_W (1UL << 2)
#define PTE_X (1UL << 3)
#define PTE_U (1UL << 4)
#define PTE_A (1UL << 6)
#define PTE_D (1UL << 7)
