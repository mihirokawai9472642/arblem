#include "riscv.h"

#define FIRMWARE_BASE 0x80000000
#define FIRMWARE_SIZE 0x200000
#define KERNEL_BASE   (FIRMWARE_BASE + FIRMWARE_SIZE)

#define FIRMWARE_MEDELEG ((1 << EXC_INSN_MISALIGNED) | (1 << EXC_INSN_ACCESS) | \
                          (1 << EXC_ILLEGAL_INSN) | (1 << EXC_BREAKPOINT) | \
                          (1 << EXC_LOAD_MISALIGNED) | (1 << EXC_LOAD_ACCESS) | \
                          (1 << EXC_STORE_MISALIGNED) | (1 << EXC_STORE_ACCESS) | \
                          (1 << EXC_USER_ECALL) | (1 << EXC_INSN_PAGE_FAULT) | \
                          (1 << EXC_LOAD_PAGE_FAULT) | (1 << EXC_STORE_PAGE_FAULT))

#define FIRMWARE_MIDELEG ((1 << IRQ_S_SOFT) | (1 << IRQ_S_TIMER) | (1 << IRQ_S_EXT))

#define FIRMWARE_PMPCFG0 (PMP_NAPOT | ((PMP_NAPOT | PMP_R | PMP_W | PMP_X) << 8))

.section .text
.global _start

_start:
    la t0, firmware_trap
    csrw mtvec, t0

    li t0, FIRMWARE_MEDELEG
    csrw medeleg, t0
    li t0, FIRMWARE_MIDELEG
    csrw mideleg, t0

    li t0, PMP_NAPOT_ADDR(FIRMWARE_BASE, FIRMWARE_SIZE)
    csrw pmpaddr0, t0
    li t0, -1
    csrw pmpaddr1, t0
    li t0, FIRMWARE_PMPCFG0
    csrw pmpcfg0, t0

    li t0, MCOUNTEREN_TM
    csrw mcounteren, t0
    li t0, MENVCFG_STCE
    csrs menvcfg, t0

    li t0, MSTATUS_MPP
    csrc mstatus, t0
    li t0, MSTATUS_MPP_S
    csrs mstatus, t0

    li t0, KERNEL_BASE
    csrw mepc, t0

    mret

.balign 4
firmware_trap:
    wfi
    j firmware_trap
