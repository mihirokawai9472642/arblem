.section .text.boot
.global _start

_start:
    la sp, stack_top

    la t0, trap_entry
    csrw mtvec, t0

    call kernel_main

1:
    wfi
    j 1b

.bss
.balign 16
.space 4096

stack_top:
