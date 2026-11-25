// Trap entry and exit, and the first entry into U-mode.
//
// sscratch / tp convention:
//   - In S-mode: tp holds &cpus[hartid] and sscratch is 0.
//   - In U-mode: tp holds the user program's tp, sscratch holds &cpus[hartid],
//     and cpus[hartid].kernel_sp holds the current thread's kernel stack top.
// See docs/traps.md.

#include "cpu.h"
#include "riscv.h"
#include "trap.h"

.section .text
.global trap_entry
.global trap_enter_user
.balign 4

// stvec points here (direct mode), so every trap starts at this instruction.
trap_entry:
    // Swap tp and sscratch. From U-mode, tp becomes &cpus[hartid] (non-zero)
    // and sscratch holds the user program's tp. From S-mode, tp becomes 0 and
    // sscratch holds &cpus[hartid].
    csrrw tp, sscratch, tp
    bnez tp, 1f

    // Trap from S-mode: restore kernel tp from sscratch and clear sscratch.
    csrrw tp, sscratch, zero
    addi sp, sp, -TRAP_FRAME_SIZE
    sd t0, 8(sp)
    addi t0, sp, TRAP_FRAME_SIZE
    sd t0, 128(sp)
    sd tp, 144(sp)
    j 2f

1:
    // Trap from U-mode: stash user sp in cpu->user_sp, switch to this
    // thread's kernel stack, and clear sscratch now that we are in S-mode.
    sd sp, CPU_USER_SP(tp)
    ld sp, CPU_KERNEL_SP(tp)
    addi sp, sp, -TRAP_FRAME_SIZE
    sd t0, 8(sp)
    ld t0, CPU_USER_SP(tp)
    sd t0, 128(sp)
    csrrw t0, sscratch, zero
    sd t0, 144(sp)

2:
    // Save the remaining caller-saved registers and gp in the trap frame
    // (struct trap_frame in trap.c). t0 (8), sp (128), and tp (144) are
    // already saved above. trap_handler is a C function and preserves s0-s11.
    sd ra, 0(sp)
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
    sd gp, 136(sp)

    mv a0, sp
    call trap_handler

    // Returning to U-mode (SPP = 0): record this thread's kernel stack in the
    // current hart's struct cpu (the thread may have migrated to another hart
    // during trap_handler), stash &cpus[hartid] in sscratch, and restore user
    // tp. Returning to S-mode (SPP = 1) skips this so tp keeps pointing to the
    // current physical hart's struct cpu.
    csrr t0, sstatus
    andi t0, t0, SSTATUS_SPP
    bnez t0, 3f

    addi t0, sp, TRAP_FRAME_SIZE
    sd t0, CPU_KERNEL_SP(tp)
    csrw sscratch, tp
    ld tp, 144(sp)

3:
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

    sd sp, CPU_KERNEL_SP(tp)
    csrw sscratch, tp
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
