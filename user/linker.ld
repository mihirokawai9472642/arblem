SECTIONS {
    . = 0x40000000;

    .text : {
        *(.text.start)
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.* .srodata .srodata.*)
    }

    .data : {
        *(.data .data.* .sdata .sdata.*)
    }

    .bss : {
        *(.bss .bss.* .sbss .sbss.*)
    }
}
