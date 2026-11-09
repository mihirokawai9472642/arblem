#include "proc.h"
#include "page.h"
#include "riscv.h"
#include "thread.h"
#include "trap.h"
#include "vm.h"

#define PROC_BASE      0x40000000UL
#define PROC_STACK_TOP 0x80000000UL

static void proc_entry(void) {
    trap_enter_user(PROC_BASE, PROC_STACK_TOP);
}

void proc_create(const char *image, unsigned long size) {
    unsigned long *vm = vm_create();

    for (unsigned long off = 0; off < size; off += PAGE_SIZE) {
        char *page = page_alloc();

        for (unsigned long i = 0; i < PAGE_SIZE && off + i < size; i++) {
            page[i] = image[off + i];
        }

        vm_map(vm, PROC_BASE + off, (unsigned long)page, PAGE_SIZE, PTE_R | PTE_W | PTE_X | PTE_U);
    }

    unsigned long stack = (unsigned long)page_alloc();

    vm_map(vm, PROC_STACK_TOP - PAGE_SIZE, stack, PAGE_SIZE, PTE_R | PTE_W | PTE_U);

    thread_create(proc_entry, vm);
}
