SECTIONS {
    . = 0x80200000;

    __kernel_start = .;

    .text : {
        *(.text.boot)
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .bss : {
        __bss_start = .;
        *(.bss .bss.* .sbss .sbss.*)
        __bss_end = .;
    }

    . = ALIGN(4096);
    __kernel_end = .;
}
