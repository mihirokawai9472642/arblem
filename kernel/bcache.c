// LRU block cache: maps 4 KiB filesystem block numbers onto 512-byte VirtIO
// sectors. Buffers are kept in a circular doubly-linked list ordered from most
// recently used (bcache_head.next) to least recently used (bcache_head.prev).

#include "bcache.h"
#include "panic.h"
#include "spinlock.h"
#include "thread.h"
#include "virtio_blk.h"

#define BCACHE_COUNT             16
#define BCACHE_SECTORS_PER_BLOCK (BCACHE_BLOCK_SIZE / VIRTIO_BLK_SECTOR_SIZE)

static struct spinlock bcache_lock = SPINLOCK_INIT("bcache");

// Static pool of cache buffers in .bss (identity-mapped, so b->data is safe
// for VirtIO DMA).
static struct bcache_buf bcache_bufs[BCACHE_COUNT];

// Sentinel node for the circular LRU list.
static struct bcache_buf bcache_head;

// Wait channel for threads waiting on a buffer currently being read from disk.
static char bcache_wait;

void bcache_init(void) {
    bcache_head.prev = &bcache_head;
    bcache_head.next = &bcache_head;

    for (int i = 0; i < BCACHE_COUNT; i++) {
        struct bcache_buf *b = &bcache_bufs[i];

        b->next = bcache_head.next;
        b->prev = &bcache_head;
        bcache_head.next->prev = b;
        bcache_head.next = b;
    }
}

static struct bcache_buf *bcache_find(unsigned int blockno) {
    for (struct bcache_buf *b = bcache_head.next; b != &bcache_head;
         b = b->next) {
        if ((b->valid || b->busy) && b->blockno == blockno) {
            return b;
        }
    }

    return 0;
}

static int bcache_read_block(struct bcache_buf *b) {
    unsigned long sector = (unsigned long)b->blockno * BCACHE_SECTORS_PER_BLOCK;

    for (unsigned long i = 0; i < BCACHE_SECTORS_PER_BLOCK; i++) {
        int err =
            virtio_blk_read(sector + i, b->data + i * VIRTIO_BLK_SECTOR_SIZE);

        if (err < 0) {
            return err;
        }
    }

    return 0;
}

struct bcache_buf *bcache_get(unsigned int blockno) {
    spinlock_acquire(&bcache_lock);

    for (;;) {
        struct bcache_buf *b = bcache_find(blockno);

        if (b != 0) {
            if (b->busy) {
                thread_sleep(&bcache_wait, &bcache_lock);
                continue;
            }

            b->ref++;
            spinlock_release(&bcache_lock);

            return b;
        }

        // Cache miss: recycle the least recently used unpinned buffer.
        for (b = bcache_head.prev; b != &bcache_head; b = b->prev) {
            if (b->ref == 0 && !b->busy) {
                b->blockno = blockno;
                b->valid = 0;
                b->busy = 1;
                b->ref = 1;

                // Release bcache_lock during disk I/O because virtio_blk_read
                // sleeps. b->busy = 1 prevents other threads from reusing or
                // reading this buffer before the read completes.
                spinlock_release(&bcache_lock);
                int err = bcache_read_block(b);
                spinlock_acquire(&bcache_lock);

                b->busy = 0;

                if (err == 0) {
                    b->valid = 1;
                } else {
                    b->ref = 0;
                }

                thread_wakeup(&bcache_wait);
                spinlock_release(&bcache_lock);

                return err == 0 ? b : 0;
            }
        }

        spinlock_release(&bcache_lock);

        return 0;
    }
}

int bcache_write(struct bcache_buf *b) {
    unsigned long sector = (unsigned long)b->blockno * BCACHE_SECTORS_PER_BLOCK;

    for (unsigned long i = 0; i < BCACHE_SECTORS_PER_BLOCK; i++) {
        int err =
            virtio_blk_write(sector + i, b->data + i * VIRTIO_BLK_SECTOR_SIZE);

        if (err < 0) {
            return err;
        }
    }

    return 0;
}

void bcache_put(struct bcache_buf *b) {
    spinlock_acquire(&bcache_lock);

    if (b->ref <= 0) {
        spinlock_release(&bcache_lock);
        panic("bcache_put: refcount underflow");
    }

    b->ref--;

    if (b->ref == 0) {
        // Move to the MRU (head) end of the LRU list.
        b->next->prev = b->prev;
        b->prev->next = b->next;

        b->next = bcache_head.next;
        b->prev = &bcache_head;
        bcache_head.next->prev = b;
        bcache_head.next = b;
    }

    spinlock_release(&bcache_lock);
}
