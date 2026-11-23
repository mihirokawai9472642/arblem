#pragma once

// Per-hart state, pointed to by the tp register while running in S-mode.
// Included from both C and assembly (trap.S), so C declarations are guarded.

#define CPU_MAX 8

// Byte offsets of fields accessed by trap.S. Checked with _Static_assert in
// cpu.c so struct cpu and trap.S cannot drift apart.
#define CPU_KERNEL_SP 0
#define CPU_USER_SP   8

#ifndef __ASSEMBLER__

struct cpu {
    unsigned long
        kernel_sp;         // Current thread's kernel stack top for U-mode traps
    unsigned long user_sp; // Scratch slot for user sp during trap entry
    unsigned long hartid;  // Hardware thread ID from OpenSBI
    unsigned long *vm_root; // Active page table root on this hart
    int lock_depth;         // Depth of spinlock_push_off() nesting
    int int_was_on;         // Whether sstatus.SIE was set at lock_depth 0
};

// Initialize this hart's struct cpu and point tp at it. Must be called before
// any function that uses cpu_current().
void cpu_init(unsigned long hartid);

// Return the current hart's struct cpu (stored in the tp register in S-mode).
static inline struct cpu *cpu_current(void) {
    struct cpu *c;

    __asm__ volatile("mv %0, tp" : "=r"(c));
    return c;
}

#endif
