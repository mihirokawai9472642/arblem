// Trap entry and exit, and the first entry into U-mode.
//
// sscratch convention: while the hart runs in U-mode, sscratch holds the
// kernel stack pointer of the current thread; while it runs in S-mode,
// sscratch is 0. See docs/traps.md.

#include "riscv.h"
#include "trap.h"

.section .text
.global trap_entry
.global trap_enter_user
.balign 4

// stvec points here (direct mode), so every trap starts at this instruction.
trap_entry:
    // Swap sp and sscratch. From U-mode this loads the kernel stack. From
    // S-mode it loads 0, so take the original sp back from sscratch.
    csrrw sp, sscratch, sp
    bnez sp, 1f
    csrr sp, sscratch

1:
    // Save the caller-saved registers, plus gp and tp, in a trap frame
    // (struct trap_frame in trap.c). trap_handler is a C function and
    // preserves s0-s11 itself.
    addi sp, sp, -TRAP_FRAME_SIZE

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

    // gp and tp are neither caller- nor callee-saved: no function touches
    // them, so they still hold the user program's values. Save them too, or
    // the next program to run would overwrite them.
    sd gp, 136(sp)
    sd tp, 144(sp)

    // sscratch now holds the interrupted sp. Save it, and set sscratch to 0
    // because the hart is in S-mode until the sret below.
    csrrw t0, sscratch, zero
    sd t0, 128(sp)

    mv a0, sp
    call trap_handler

    // Returning to U-mode (SPP = 0): the next trap from U-mode must find
    // this thread's kernel stack, which is sp without the frame.
    csrr t0, sstatus
    andi t0, t0, SSTATUS_SPP
    bnez t0, 2f

    addi t0, sp, TRAP_FRAME_SIZE
    csrw sscratch, t0

2:
    // Restore the frame, possibly modified by trap_handler (a0 holds a
    // system call's return value). sp goes last, since it is the base.
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
    ld gp, 136(sp)
    ld tp, 144(sp)
    ld sp, 128(sp)

    sret

// void trap_enter_user(unsigned long pc, unsigned long sp,
//                      unsigned long argc, unsigned long argv)
//
// Enter U-mode at pc with stack pointer sp, a0 = argc, and a1 = argv, as if
// returning from a trap. Clears all other integer registers so kernel values
// do not leak into the program. The current kernel sp becomes this thread's
// kernel stack for later traps.
trap_enter_user:
    // No trap may arrive while sscratch is non-zero in S-mode. In U-mode,
    // S-mode interrupts are enabled whatever SIE says.
    csrci sstatus, SSTATUS_SIE

    // sret returns to U-mode (SPP = 0) at sepc.
    li t0, SSTATUS_SPP
    csrc sstatus, t0
    csrw sepc, a0

    csrw sscratch, sp
    mv sp, a1
    mv a0, a2
    mv a1, a3

    // Clear every other integer register so the program does not see values
    // left behind by the kernel or a previous thread.
    mv ra, zero
    mv gp, zero
    mv tp, zero
    mv t0, zero
    mv t1, zero
    mv t2, zero
    mv s0, zero
    mv s1, zero
    mv a2, zero
    mv a3, zero
    mv a4, zero
    mv a5, zero
    mv a6, zero
    mv a7, zero
    mv s2, zero
    mv s3, zero
    mv s4, zero
    mv s5, zero
    mv s6, zero
    mv s7, zero
    mv s8, zero
    mv s9, zero
    mv s10, zero
    mv s11, zero
    mv t3, zero
    mv t4, zero
    mv t5, zero
    mv t6, zero

    sret
