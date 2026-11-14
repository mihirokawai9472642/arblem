SECTIONS {
    . = 0x80200000;

    __kernel_start = .;

    .text : {
        *(.text.boot)
        *(.text .text.*)
    }

    . = ALIGN(4096);
    __text_end = .;

    .rodata : {
        *(.rodata .rodata.* .srodata .srodata.*)
    }

    . = ALIGN(4096);
    __rodata_end = .;

    .data : {
        *(.data .data.* .sdata .sdata.*)
    }

    .bss : {
        __bss_start = .;
        *(.bss .bss.* .sbss .sbss.*)
        __bss_end = .;
    }

    . = ALIGN(4096);
    __kernel_end = .;
}
