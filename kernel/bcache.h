#pragma once

// Fixed-size LRU block cache over the VirtIO block device. Each block is
// 4 KiB (8 sectors of 512 bytes), matching PAGE_SIZE.

#define BCACHE_BLOCK_SIZE 4096

struct bcache_buf {
    unsigned int blockno;
    int valid;
    int busy;
    int ref;
    struct bcache_buf *prev;
    struct bcache_buf *next;
    unsigned char data[BCACHE_BLOCK_SIZE];
};

// Initialize the LRU list of cache buffers. Must be called once during boot.
void bcache_init(void);

// Return a pinned buffer (ref incremented) holding block blockno, reading it
// from disk on a cache miss. Returns 0 on I/O error or if all buffers are in
// use. Callers must release the buffer with bcache_put.
struct bcache_buf *bcache_get(unsigned int blockno);

// Write the buffer's contents back to disk (write-through). Returns 0 or
// -ERR_IO.
int bcache_write(struct bcache_buf *b);

// Release a buffer obtained from bcache_get. When ref reaches 0, the buffer
// moves to the MRU end of the LRU list and becomes eligible for eviction.
void bcache_put(struct bcache_buf *b);
