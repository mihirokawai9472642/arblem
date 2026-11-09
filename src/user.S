.section .rodata
.global user_hello
.global user_hello_end
.global user_count
.global user_count_end

user_hello:
    .incbin "build/user/hello.bin"
user_hello_end:

user_count:
    .incbin "build/user/count.bin"
user_count_end:
