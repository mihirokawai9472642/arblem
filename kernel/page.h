#pragma once

#define PAGE_SIZE 4096

void page_init(unsigned long start, unsigned long end);
void *page_alloc(void);
void page_free(void *page);
unsigned long page_free_count(void);
