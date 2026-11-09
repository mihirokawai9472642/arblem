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

    return page;
}

void page_free(void *page) {
    struct page_node *node = page;

    node->next = page_free_list;
    page_free_list = node;
    page_free_pages++;
}

unsigned long page_free_count(void) {
    return page_free_pages;
}
