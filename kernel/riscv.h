#pragma once

// RISC-V privileged architecture: CSR access and bit definitions.
// Included from both C and assembly, so only macros belong here.

// Read, write, set bits in, or clear bits in a CSR given by name,
// e.g. csr_read(sstatus).
#define csr_read(csr)                                                          \
    ({                                                                         \
        unsigned long __v;                                                     \
        __asm__ volatile("csrr %0, " #csr : "=r"(__v));                        \
        __v;                                                                   \
    })

#define csr_write(csr, val)                                                    \
    ({                                                                         \
        __asm__ volatile("csrw " #csr ", %0"                                   \
                         :                                                     \
                         : "r"((unsigned long)(val))                           \
                         : "memory");                                          \
    })

#define csr_set(csr, bits)                                                     \
    ({                                                                         \
        __asm__ volatile("csrs " #csr ", %0"                                   \
                         :                                                     \
                         : "r"((unsigned long)(bits))                          \
                         : "memory");                                          \
    })

#define csr_clear(csr, bits)                                                   \
    ({                                                                         \
        __asm__ volatile("csrc " #csr ", %0"                                   \
                         :                                                     \
                         : "r"((unsigned long)(bits))                          \
                         : "memory");                                          \
    })

// scause exception codes (interrupt bit clear).
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

// S-mode interrupt numbers: scause codes with the interrupt bit set, and bit
// positions in sie and sip.
#define IRQ_S_SOFT  1
#define IRQ_S_TIMER 5
#define IRQ_S_EXT   9

#define SSTATUS_SIE (1UL << 1)  // S-mode interrupts enabled
#define SSTATUS_SPP (1UL << 8)  // Privilege before the trap: 0 = U, 1 = S
#define SSTATUS_SUM (1UL << 18) // S-mode may access U pages
#define SIE_STIE    (1UL << IRQ_S_TIMER)
#define SIE_SEIE    (1UL << IRQ_S_EXT)

#define SCAUSE_INTERRUPT           (1UL << 63)
#define SCAUSE_SUPERVISOR_TIMER    (SCAUSE_INTERRUPT | IRQ_S_TIMER)
#define SCAUSE_SUPERVISOR_EXTERNAL (SCAUSE_INTERRUPT | IRQ_S_EXT)

// satp MODE field selecting Sv39 paging.
#define SATP_SV39 (8UL << 60)

// Page table entry flags. See docs/memory.md.
#define PTE_V (1UL << 0) // Valid
#define PTE_R (1UL << 1) // Readable
#define PTE_W (1UL << 2) // Writable
#define PTE_X (1UL << 3) // Executable
#define PTE_U (1UL << 4) // Accessible from U-mode
#define PTE_A (1UL << 6) // Accessed
#define PTE_D (1UL << 7) // Dirty
