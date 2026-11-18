// Physical page allocator: a linked list of free 4 KiB pages. Each free page
// holds the pointer to the next one in its first word, so the list costs no
// memory of its own. See docs/memory.md.

#include "page.h"

struct page_node {
    struct page_node *next;
};

static struct page_node *page_free_list;
static unsigned long page_free_pages;

void page_init(unsigned long start, unsigned long end) {
    start = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1UL);

    for (unsigned long p = start; p + PAGE_SIZE <= end; p += PAGE_SIZE) {
        page_free((void *)p);
    }
}

void *page_alloc(void) {
    struct page_node *page = page_free_list;

    if (page == 0) {
        return 0;
    }

    page_free_list = page->next;
    page_free_pages--;

    // Callers rely on zeroed pages: new page tables start empty, and a new
    // program's .bss starts as zero.
    unsigned long *words = (unsigned long *)page;

    for (unsigned long i = 0; i < PAGE_SIZE / sizeof(unsigned long); i++) {
        words[i] = 0;
    }

    return page;
}

// No check for double frees or for pages outside the managed range.
void page_free(void *page) {
    struct page_node *node = page;

    node->next = page_free_list;
    page_free_list = node;
    page_free_pages++;
}

unsigned long page_free_count(void) {
    return page_free_pages;
}
