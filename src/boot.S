.section .text.boot
.global _start

_start:
    la sp, stack_top

    la t0, __bss_start
    la t1, __bss_end

2:
    bgeu t0, t1, 3f
    sb zero, 0(t0)
    addi t0, t0, 1
    j 2b

3:
    la t0, trap_entry
    csrw mtvec, t0
    csrw mscratch, zero

    call kernel_main

1:
    wfi
    j 1b

.bss
.balign 16
.space 4096

stack_top:
