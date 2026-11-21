#pragma once

// VirtIO block device driver (Modern VirtIO 1.0+ over MMIO v2).

#define VIRTIO_BLK_SECTOR_SIZE 512

struct fdt_info;

// Probe the VirtIO MMIO slots in info for a v2 block device and initialize
// its request queue. Returns 0 on success, -1 if no usable block device is
// found.
int virtio_blk_init(const struct fdt_info *info);

// Return the PLIC IRQ number of the block device (0 if none).
unsigned int virtio_blk_irq(void);

// Return the disk capacity in 512-byte sectors (0 if none).
unsigned long virtio_blk_sectors(void);
