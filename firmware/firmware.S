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

#define FIRMWARE_FRAME_SIZE 128

.section .text
.global _start

_start:
    la t0, firmware_trap
    csrw mtvec, t0
    la t0, firmware_stack_top
    csrw mscratch, t0

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
    csrrw sp, mscratch, sp
    addi sp, sp, -FIRMWARE_FRAME_SIZE

    sd ra, 0(sp)
    sd t0, 8(sp)
    sd t1, 16(sp)
    sd t2, 24(sp)
    sd t3, 32(sp)
    sd t4, 40(sp)
    sd t5, 48(sp)
    sd t6, 56(sp)
    sd a0, 64(sp)
    sd a1, 72(sp)
    sd a2, 80(sp)
    sd a3, 88(sp)
    sd a4, 96(sp)
    sd a5, 104(sp)
    sd a6, 112(sp)
    sd a7, 120(sp)

    mv a0, sp
    call sbi_handler

    ld ra, 0(sp)
    ld t0, 8(sp)
    ld t1, 16(sp)
    ld t2, 24(sp)
    ld t3, 32(sp)
    ld t4, 40(sp)
    ld t5, 48(sp)
    ld t6, 56(sp)
    ld a0, 64(sp)
    ld a1, 72(sp)
    ld a2, 80(sp)
    ld a3, 88(sp)
    ld a4, 96(sp)
    ld a5, 104(sp)
    ld a6, 112(sp)
    ld a7, 120(sp)

    addi sp, sp, FIRMWARE_FRAME_SIZE
    csrrw sp, mscratch, sp
    mret

.section .bss
.balign 16
.space 4096

firmware_stack_top:
