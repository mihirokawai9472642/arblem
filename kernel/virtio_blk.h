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

// Read or write one 512-byte sector into or from kernel buffer buf.
// Returns 0 on success, or -ERR_IO if no disk is present, sector is out of
// range, or the device reports an error.
int virtio_blk_read(unsigned long sector, void *buf);
int virtio_blk_write(unsigned long sector, const void *buf);

// Called by trap_handler for the block device IRQ.
void virtio_blk_interrupt(void);
