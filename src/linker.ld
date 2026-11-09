SECTIONS {
    . = 0x80000000;

    .text : {
        *(.text.boot)
        *(EXCLUDE_FILE(*/user/*) .text .text.*)
    }

    .rodata : {
        *(EXCLUDE_FILE(*/user/*) .rodata .rodata.*)
    }

    .bss : {
        __bss_start = .;
        *(EXCLUDE_FILE(*/user/*) .bss .bss.* .sbss .sbss.*)
        __bss_end = .;
    }

    .user : ALIGN(4096) {
        __user_start = .;
        */user/*(.text .text.* .rodata .rodata.* .data .data.* .sdata .sdata.* .bss .bss.* .sbss .sbss.*)
        . = ALIGN(16);
        . += 4096;
        __user_stack_top = .;
        . = ALIGN(4096);
        __user_end = .;
    }

    __kernel_end = .;
}
