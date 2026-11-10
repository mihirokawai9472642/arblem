#pragma once

void vm_init(unsigned long ram_end);
unsigned long *vm_create(void);
void vm_map(unsigned long *root, unsigned long va, unsigned long pa, unsigned long size, unsigned long perm);
void vm_switch(unsigned long *root);
int vm_copy_from_user(void *dst, unsigned long src, unsigned long len);
