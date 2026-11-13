.section .rodata
.global user_hello
.global user_hello_end
.global user_count
.global user_count_end
.global user_fault
.global user_fault_end
.global user_shell
.global user_shell_end

.balign 8
user_hello:
    .incbin "build/user/hello.elf"
user_hello_end:

.balign 8
user_count:
    .incbin "build/user/count.elf"
user_count_end:

.balign 8
user_fault:
    .incbin "build/user/fault.elf"
user_fault_end:

.balign 8
user_shell:
    .incbin "build/user/shell.elf"
user_shell_end:
