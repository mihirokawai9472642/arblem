// Modern VirtIO 1.0+ (MMIO v2) block device driver. Uses a single split
// virtqueue (queue 0).

#include "virtio_blk.h"
#include "error.h"
#include "fdt.h"
#include "spinlock.h"
#include "thread.h"

// VirtIO MMIO v2 register offsets (all 32-bit little-endian).
#define VIRTIO_MMIO_MAGIC               0x000
#define VIRTIO_MMIO_VERSION             0x004
#define VIRTIO_MMIO_DEVICE_ID           0x008
#define VIRTIO_MMIO_DEVICE_FEATURES     0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL           0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX       0x034
#define VIRTIO_MMIO_QUEUE_NUM           0x038
#define VIRTIO_MMIO_QUEUE_READY         0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY        0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS    0x060
#define VIRTIO_MMIO_INTERRUPT_ACK       0x064
#define VIRTIO_MMIO_STATUS              0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW      0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH     0x084
#define VIRTIO_MMIO_QUEUE_DRIVER_LOW    0x090
#define VIRTIO_MMIO_QUEUE_DRIVER_HIGH   0x094
#define VIRTIO_MMIO_QUEUE_DEVICE_LOW    0x0a0
#define VIRTIO_MMIO_QUEUE_DEVICE_HIGH   0x0a4
#define VIRTIO_MMIO_CONFIG              0x100

#define VIRTIO_MAGIC          0x74726976U // "virt" in little-endian
#define VIRTIO_VERSION_MODERN 2U
#define VIRTIO_DEV_BLK        2U

// Device status bits (VirtIO spec 2.1).
#define VIRTIO_STATUS_ACKNOWLEDGE (1U << 0)
#define VIRTIO_STATUS_DRIVER      (1U << 1)
#define VIRTIO_STATUS_DRIVER_OK   (1U << 2)
#define VIRTIO_STATUS_FEATURES_OK (1U << 3)
#define VIRTIO_STATUS_FAILED      (1U << 7)

// VIRTIO_F_VERSION_1 is feature bit 32: bit 0 of feature word 1.
#define VIRTIO_F_VERSION_1_BIT (1U << 0)

// Number of descriptors in requestq (power of two; one block request uses 3).
#define VIRTIO_QUEUE_SIZE 8

// Descriptor flags (VirtIO spec 2.7.5).
#define VIRTQ_DESC_F_NEXT  1U // Buffer continues via the next field
#define VIRTQ_DESC_F_WRITE 2U // Buffer is device-writable (driver reads)

// Block request types and status codes (VirtIO spec 5.2.6).
#define VIRTIO_BLK_T_IN  0U // Read from disk
#define VIRTIO_BLK_T_OUT 1U // Write to disk
#define VIRTIO_BLK_S_OK  0U

// Split virtqueue layouts (VirtIO spec 2.7).
struct virtq_desc {
    unsigned long addr;
    unsigned int len;
    unsigned short flags;
    unsigned short next;
};

struct virtq_avail {
    unsigned short flags;
    unsigned short idx;
    unsigned short ring[VIRTIO_QUEUE_SIZE];
};

struct virtq_used_elem {
    unsigned int id;
    unsigned int len;
};

struct virtq_used {
    unsigned short flags;
    unsigned short idx;
    struct virtq_used_elem ring[VIRTIO_QUEUE_SIZE];
};

// Block request header (VirtIO spec 5.2.6).
struct virtio_blk_req {
    unsigned int type;
    unsigned int reserved;
    unsigned long sector;
};

_Static_assert(sizeof(struct virtq_desc) == 16, "virtq_desc must be 16 bytes");
_Static_assert(sizeof(struct virtq_used_elem) == 8,
               "virtq_used_elem must be 8 bytes");
_Static_assert(sizeof(struct virtio_blk_req) == 16,
               "virtio_blk_req must be 16 bytes");

// Alignment requirements from VirtIO spec 2.7: desc 16, avail 2, used 4.
// Placed in .bss (zeroed at boot and identity-mapped, so virtual == physical).
static _Alignas(16) struct virtq_desc blk_desc[VIRTIO_QUEUE_SIZE];
static _Alignas(2) struct virtq_avail blk_avail;
static _Alignas(4) struct virtq_used blk_used;

// Single in-flight request header and completion status byte.
static struct virtio_blk_req blk_req;
static unsigned char blk_status;

static struct spinlock blk_lock = SPINLOCK_INIT("virtio_blk");
static unsigned long blk_base;
static unsigned int blk_irq_num;
static unsigned long blk_sectors;
static unsigned short blk_used_idx;
static int blk_busy;
static char blk_busy_wait;
static char blk_done_wait;

static unsigned int mmio_read(unsigned long base, unsigned long off) {
    return *(volatile unsigned int *)(base + off);
}

static void mmio_write(unsigned long base, unsigned long off,
                       unsigned int val) {
    *(volatile unsigned int *)(base + off) = val;
}

// Order CPU memory accesses against device DMA and MMIO writes.
static void dma_barrier(void) {
    __asm__ volatile("fence iorw, iorw" : : : "memory");
}

static int virtio_blk_setup(unsigned long base, unsigned int irq) {
    // Reset the device.
    mmio_write(base, VIRTIO_MMIO_STATUS, 0);

    unsigned int status = VIRTIO_STATUS_ACKNOWLEDGE;
    mmio_write(base, VIRTIO_MMIO_STATUS, status);

    status |= VIRTIO_STATUS_DRIVER;
    mmio_write(base, VIRTIO_MMIO_STATUS, status);

    // Negotiate features: require VIRTIO_F_VERSION_1 and decline all optional
    // block features (read-only, event_idx, indirect descriptors, etc.).
    mmio_write(base, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);

    if ((mmio_read(base, VIRTIO_MMIO_DEVICE_FEATURES) &
         VIRTIO_F_VERSION_1_BIT) == 0) {
        mmio_write(base, VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_FAILED);
        return -1;
    }

    mmio_write(base, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    mmio_write(base, VIRTIO_MMIO_DRIVER_FEATURES, 0);
    mmio_write(base, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    mmio_write(base, VIRTIO_MMIO_DRIVER_FEATURES, VIRTIO_F_VERSION_1_BIT);

    status |= VIRTIO_STATUS_FEATURES_OK;
    mmio_write(base, VIRTIO_MMIO_STATUS, status);

    if ((mmio_read(base, VIRTIO_MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK) ==
        0) {
        mmio_write(base, VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_FAILED);
        return -1;
    }

    // Configure requestq (queue 0).
    mmio_write(base, VIRTIO_MMIO_QUEUE_SEL, 0);

    if (mmio_read(base, VIRTIO_MMIO_QUEUE_READY) != 0) {
        mmio_write(base, VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_FAILED);
        return -1;
    }

    unsigned int max_queue = mmio_read(base, VIRTIO_MMIO_QUEUE_NUM_MAX);

    if (max_queue == 0 || max_queue < VIRTIO_QUEUE_SIZE) {
        mmio_write(base, VIRTIO_MMIO_STATUS, status | VIRTIO_STATUS_FAILED);
        return -1;
    }

    mmio_write(base, VIRTIO_MMIO_QUEUE_NUM, VIRTIO_QUEUE_SIZE);

    unsigned long desc_pa = (unsigned long)blk_desc;
    unsigned long avail_pa = (unsigned long)&blk_avail;
    unsigned long used_pa = (unsigned long)&blk_used;

    mmio_write(base, VIRTIO_MMIO_QUEUE_DESC_LOW, (unsigned int)desc_pa);
    mmio_write(base, VIRTIO_MMIO_QUEUE_DESC_HIGH,
               (unsigned int)(desc_pa >> 32));
    mmio_write(base, VIRTIO_MMIO_QUEUE_DRIVER_LOW, (unsigned int)avail_pa);
    mmio_write(base, VIRTIO_MMIO_QUEUE_DRIVER_HIGH,
               (unsigned int)(avail_pa >> 32));
    mmio_write(base, VIRTIO_MMIO_QUEUE_DEVICE_LOW, (unsigned int)used_pa);
    mmio_write(base, VIRTIO_MMIO_QUEUE_DEVICE_HIGH,
               (unsigned int)(used_pa >> 32));

    mmio_write(base, VIRTIO_MMIO_QUEUE_READY, 1);

    // Read 64-bit disk capacity (in 512-byte sectors) from config space.
    unsigned long cap_lo = mmio_read(base, VIRTIO_MMIO_CONFIG + 0);
    unsigned long cap_hi = mmio_read(base, VIRTIO_MMIO_CONFIG + 4);

    blk_sectors = cap_lo | (cap_hi << 32);
    blk_base = base;
    blk_irq_num = irq;

    status |= VIRTIO_STATUS_DRIVER_OK;
    mmio_write(base, VIRTIO_MMIO_STATUS, status);

    return 0;
}

int virtio_blk_init(const struct fdt_info *info) {
    for (int i = 0; i < info->virtio_count; i++) {
        unsigned long base = info->virtio[i].base;

        if (info->virtio[i].size < 0x200) {
            continue;
        }

        if (mmio_read(base, VIRTIO_MMIO_MAGIC) != VIRTIO_MAGIC ||
            mmio_read(base, VIRTIO_MMIO_VERSION) != VIRTIO_VERSION_MODERN ||
            mmio_read(base, VIRTIO_MMIO_DEVICE_ID) != VIRTIO_DEV_BLK) {
            continue;
        }

        return virtio_blk_setup(base, info->virtio[i].irq);
    }

    return -1;
}

unsigned int virtio_blk_irq(void) {
    return blk_irq_num;
}

unsigned long virtio_blk_sectors(void) {
    return blk_sectors;
}

static int virtio_blk_rw(unsigned long sector, unsigned long buf_pa,
                         int write) {
    if (blk_base == 0 || sector >= blk_sectors) {
        return -ERR_IO;
    }

    spinlock_acquire(&blk_lock);

    // Serialize requests so descriptors 0..2 and blk_req stay exclusive
    // while a thread sleeps waiting for DMA completion.
    while (blk_busy) {
        thread_sleep(&blk_busy_wait, &blk_lock);
    }

    blk_busy = 1;

    blk_req.type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    blk_req.reserved = 0;
    blk_req.sector = sector;
    blk_status = 0xff;

    // Descriptor 0: 16-byte request header (device reads).
    blk_desc[0].addr = (unsigned long)&blk_req;
    blk_desc[0].len = sizeof(blk_req);
    blk_desc[0].flags = VIRTQ_DESC_F_NEXT;
    blk_desc[0].next = 1;

    // Descriptor 1: 512-byte sector data. VIRTQ_DESC_F_WRITE means
    // device-writable, so it is set for reads and clear for writes.
    blk_desc[1].addr = buf_pa;
    blk_desc[1].len = VIRTIO_BLK_SECTOR_SIZE;
    blk_desc[1].flags = VIRTQ_DESC_F_NEXT | (write ? 0U : VIRTQ_DESC_F_WRITE);
    blk_desc[1].next = 2;

    // Descriptor 2: 1-byte completion status (device writes).
    blk_desc[2].addr = (unsigned long)&blk_status;
    blk_desc[2].len = 1;
    blk_desc[2].flags = VIRTQ_DESC_F_WRITE;
    blk_desc[2].next = 0;

    // Publish descriptor chain head (0) into the available ring.
    blk_avail.ring[blk_avail.idx % VIRTIO_QUEUE_SIZE] = 0;
    dma_barrier();
    blk_avail.idx++;
    dma_barrier();

    // Notify queue 0 (doorbell).
    mmio_write(blk_base, VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    // Sleep until the device advances the used ring.
    while (blk_used_idx == ((volatile struct virtq_used *)&blk_used)->idx) {
        thread_sleep(&blk_done_wait, &blk_lock);
    }

    // Ensure DMA writes to buf and blk_status are visible after used.idx.
    dma_barrier();
    blk_used_idx++;

    unsigned char status = *(volatile unsigned char *)&blk_status;

    blk_busy = 0;
    thread_wakeup(&blk_busy_wait);
    spinlock_release(&blk_lock);

    return status == VIRTIO_BLK_S_OK ? 0 : -ERR_IO;
}

int virtio_blk_read(unsigned long sector, void *buf) {
    return virtio_blk_rw(sector, (unsigned long)buf, 0);
}

int virtio_blk_write(unsigned long sector, const void *buf) {
    return virtio_blk_rw(sector, (unsigned long)buf, 1);
}

void virtio_blk_interrupt(void) {
    if (blk_base == 0) {
        return;
    }

    spinlock_acquire(&blk_lock);

    // Acknowledge the level-triggered VirtIO interrupt before plic_complete.
    unsigned int isr = mmio_read(blk_base, VIRTIO_MMIO_INTERRUPT_STATUS);

    mmio_write(blk_base, VIRTIO_MMIO_INTERRUPT_ACK, isr);
    thread_wakeup(&blk_done_wait);

    spinlock_release(&blk_lock);
}
