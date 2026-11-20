#pragma once

// Sv39 page tables and access to user memory. See docs/memory.md.

// Build the kernel page table, identity-mapping the devices and RAM up to
// ram_end, and turn on paging.
void vm_init(unsigned long ram_end);

// Create a page table for a user program. It shares the kernel mappings.
// Returns 0 when out of memory.
unsigned long *vm_create(void);

// Map size bytes at va to physical address pa with PTE flags perm, one 4 KiB
// page at a time. Returns 0, or -1 when out of memory.
int vm_map(unsigned long *root, unsigned long va, unsigned long pa,
           unsigned long size, unsigned long perm);

// Unmap the single 4 KiB page at va in root, free its physical page, and flush
// the TLB. Does nothing if va is not mapped.
void vm_unmap(unsigned long *root, unsigned long va);

// Make root the active page table. 0 selects the kernel page table.
void vm_switch(unsigned long *root);

// Free a page table from vm_create, with every user page mapped in it.
void vm_free(unsigned long *root);

// Copy between kernel memory and user memory of the active page table, after
// checking that the user range is mapped with the needed permission.
// Return 0, or -1 if the check fails.
int vm_copy_from_user(void *dst, unsigned long src, unsigned long len);
int vm_copy_to_user(unsigned long dst, const void *src, unsigned long len);

// Copy a NUL-terminated string from user address src into dst (up to max
// bytes including the NUL). Returns the length (excluding NUL), -ERR_FAULT
// on a bad address, or -ERR_2BIG if no NUL appears within max.
int vm_copy_str_from_user(char *dst, unsigned long src, unsigned long max);
