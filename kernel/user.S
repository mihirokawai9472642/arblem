// User programs embedded in the kernel image. Each ELF file built by the
// Makefile is included byte for byte between user_<name> and user_<name>_end,
// and proc_images[] in proc.c maps program names to these symbols.
//
// To add a program, copy one block here and see docs/user.md.

.section .rodata
.global user_hello
.global user_hello_end
.global user_count
.global user_count_end
.global user_fault
.global user_fault_end
.global user_orphan
.global user_orphan_end
.global user_regcheck
.global user_regcheck_end
.global user_regclobber
.global user_regclobber_end
.global user_pipecheck
.global user_pipecheck_end
.global user_upper
.global user_upper_end
.global user_echo
.global user_echo_end
.global user_shell
.global user_shell_end

// 8-byte alignment lets elf_check and proc_create read the headers' 8-byte
// fields in place.
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
user_orphan:
    .incbin "build/user/orphan.elf"
user_orphan_end:

.balign 8
user_regcheck:
    .incbin "build/user/regcheck.elf"
user_regcheck_end:

.balign 8
user_regclobber:
    .incbin "build/user/regclobber.elf"
user_regclobber_end:

.balign 8
user_pipecheck:
    .incbin "build/user/pipecheck.elf"
user_pipecheck_end:

.balign 8
user_upper:
    .incbin "build/user/upper.elf"
user_upper_end:

.balign 8
user_echo:
    .incbin "build/user/echo.elf"
user_echo_end:

.balign 8
user_shell:
    .incbin "build/user/shell.elf"
user_shell_end:
