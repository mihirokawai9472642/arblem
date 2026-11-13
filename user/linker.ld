SECTIONS {
    . = 0x40000000;

    .text : {
        *(.text .text.*)
    }

    . = ALIGN(4096);

    .rodata : {
        *(.rodata .rodata.* .srodata .srodata.*)
    }

    . = ALIGN(4096);

    .data : {
        *(.data .data.* .sdata .sdata.*)
    }

    .bss : {
        *(.bss .bss.* .sbss .sbss.*)
    }
}
