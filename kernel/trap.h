#pragma once

// Bytes trap_entry pushes on the kernel stack: 19 registers (ra, t0-t6, a0-a7,
// sp, gp, tp), rounded up to keep sp 16-byte aligned. Must match struct
// trap_frame in trap.c and the offsets in trap.S.
#define TRAP_FRAME_SIZE 160

#ifndef __ASSEMBLER__
// Enter U-mode at pc with stack pointer sp and initial arguments a0 = argc,
// a1 = argv. Clears all other integer registers. Does not return; the thread
// comes back into the kernel only through traps. Defined in trap.S.
void trap_enter_user(unsigned long pc, unsigned long sp, unsigned long argc,
                     unsigned long argv);
#endif
