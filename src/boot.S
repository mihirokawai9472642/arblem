.section .text.boot
.global _start

_start:
    la sp, stack_top
    call kernel_main

1:
    j 1b

.bss
.balign 16
.space 4096

stack_top:
