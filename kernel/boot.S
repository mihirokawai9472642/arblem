// Kernel entry point.
//
// OpenSBI jumps here in S-mode with paging off, a0 = hart ID and a1 = the
// physical address of the device tree. The linker script puts .text.boot
// first, so _start sits at the load address 0x80200000.
// See docs/boot.md.

.section .text.boot
.global _start

_start:
    la sp, stack_top

    // Zero .bss. The boot stack is in .bss too, but nothing is on it yet.
    la t0, __bss_start
    la t1, __bss_end

2:
    bgeu t0, t1, 3f
    sb zero, 0(t0)
    addi t0, t0, 1
    j 2b

3:
    // Send every trap to trap_entry, and mark the hart as being in S-mode
    // (sscratch = 0) for the trap entry code. See docs/traps.md.
    la t0, trap_entry
    csrw stvec, t0
    csrw sscratch, zero

    // a0 and a1 still hold the hart ID and DTB address from OpenSBI.
    call kernel_main

    // kernel_main never returns, but stop the hart if it ever does.
1:
    wfi
    j 1b

// Stack for the boot code. thread_start abandons it when the first thread
// runs; every thread then uses its own kernel stack.
.bss
.balign 16
.space 4096

stack_top:
