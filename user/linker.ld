/*
 * User program layout. Programs start at 0x40000000, the bottom of the user
 * range in kernel/proc.c. Each group of sections starts on its own page, so
 * every segment can get its own permissions (W^X) and proc_load accepts it.
 * The entry point is _start in user/start.c. See docs/processes.md.
 */
SECTIONS {
    . = 0x40000000;

    /* R X */
    .text : {
        *(.text .text.*)
    }

    . = ALIGN(4096);

    /* R */
    .rodata : {
        *(.rodata .rodata.* .srodata .srodata.*)
    }

    . = ALIGN(4096);

    /* R W, with .bss after .data in the same segment. */
    .data : {
        *(.data .data.* .sdata .sdata.*)
    }

    .bss : {
        *(.bss .bss.* .sbss .sbss.*)
    }
}
