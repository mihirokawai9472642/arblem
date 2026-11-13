.section .rodata
.global user_hello
.global user_hello_end
.global user_count
.global user_count_end
.global user_fault
.global user_fault_end
.global user_shell
.global user_shell_end

user_hello:
    .incbin "build/user/hello.bin"
user_hello_end:

user_count:
    .incbin "build/user/count.bin"
user_count_end:

user_fault:
    .incbin "build/user/fault.bin"
user_fault_end:

user_shell:
    .incbin "build/user/shell.bin"
user_shell_end:
