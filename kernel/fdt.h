#pragma once

// Minimal Flattened Device Tree (FDT / DTB) parser for QEMU virt.

#define FDT_VIRTIO_MAX 8
#define FDT_HART_MAX   8

struct fdt_mmio {
    unsigned long base;
    unsigned long size;
    unsigned int irq;
};

struct fdt_info {
    unsigned long ram_base;
    unsigned long ram_size;
    unsigned long dtb_size;
    unsigned long timebase_freq;
    unsigned long harts[FDT_HART_MAX];
    int hart_count;
    struct fdt_mmio plic;
    struct fdt_mmio uart;
    struct fdt_mmio virtio[FDT_VIRTIO_MAX];
    int virtio_count;
};

// Parse the DTB at physical address dtb into *out. Must be called before
// paging is turned on (or while the DTB is mapped). Returns 0 on success, -1
// if the DTB header or structure is invalid.
int fdt_parse(unsigned long dtb, struct fdt_info *out);
