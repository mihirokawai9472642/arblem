/*
 * Kernel memory layout. OpenSBI occupies 0x80000000 and jumps to 0x80200000,
 * where QEMU loads this image.
 *
 * __text_end and __rodata_end are page aligned so that code, read-only data
 * and writable data never share a page, which lets vm_init map them with
 * different permissions (W^X). See docs/memory.md.
 */
SECTIONS {
    . = 0x80200000;

    __kernel_start = .;

    .text : {
        /* _start (boot.S) must come first. */
        *(.text.boot)
        *(.text .text.*)
    }

    . = ALIGN(4096);
    __text_end = .;

    /* Also holds the embedded user ELF files from user.S. */
    .rodata : {
        *(.rodata .rodata.* .srodata .srodata.*)
    }

    . = ALIGN(4096);
    __rodata_end = .;

    .data : {
        *(.data .data.* .sdata .sdata.*)
    }

    /* Zeroed by _start. Includes the boot stack. */
    .bss : {
        __bss_start = .;
        *(.bss .bss.* .sbss .sbss.*)
        __bss_end = .;
    }

    /* Free memory for page_init starts here. */
    . = ALIGN(4096);
    __kernel_end = .;
}
