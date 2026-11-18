#pragma once

// Physical page allocator. See docs/memory.md.

#define PAGE_SIZE 4096

// Add every whole page between start (rounded up) and end to the free list.
void page_init(unsigned long start, unsigned long end);

// Allocate one zero-filled page. Returns 0 when no page is free.
void *page_alloc(void);

// Return a page from page_alloc to the free list.
void page_free(void *page);

// Number of free pages.
unsigned long page_free_count(void);
