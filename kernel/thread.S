// Context switch between kernel threads. See docs/threads.md.

#include "riscv.h"
#include "thread.h"

.section .text
.global thread_switch
.global thread_entry

// void thread_switch(unsigned long *old_sp, unsigned long new_sp)
//
// Save the callee-saved registers on the current stack, store sp in *old_sp,
// then switch to new_sp and restore the registers saved there. Returns into
// the new thread, at the ra loaded from its stack. Caller-saved registers
// need no saving: the compiler already treats this as an ordinary call.
thread_switch:
    addi sp, sp, -THREAD_FRAME_SIZE

    sd ra, 0(sp)
    sd s0, 8(sp)
    sd s1, 16(sp)
    sd s2, 24(sp)
    sd s3, 32(sp)
    sd s4, 40(sp)
    sd s5, 48(sp)
    sd s6, 56(sp)
    sd s7, 64(sp)
    sd s8, 72(sp)
    sd s9, 80(sp)
    sd s10, 88(sp)
    sd s11, 96(sp)

    sd sp, 0(a0)
    mv sp, a1

    ld ra, 0(sp)
    ld s0, 8(sp)
    ld s1, 16(sp)
    ld s2, 24(sp)
    ld s3, 32(sp)
    ld s4, 40(sp)
    ld s5, 48(sp)
    ld s6, 56(sp)
    ld s7, 64(sp)
    ld s8, 72(sp)
    ld s9, 80(sp)
    ld s10, 88(sp)
    ld s11, 96(sp)

    addi sp, sp, THREAD_FRAME_SIZE

    ret

// First code of a new thread. thread_create builds a frame whose ra points
// here, with s0 = the thread's entry function and s1 = its argument.
// Interrupts are enabled, so a kernel-only thread (the idle thread) can be
// preempted.
thread_entry:
    csrsi sstatus, SSTATUS_SIE
    mv a0, s1
    jr s0
