#include "vm.h"
#include "page.h"
#include "riscv.h"
#include "virt.h"

#define PTE_FROM_PA(pa) (((unsigned long)(pa) >> 12) << 10)
#define PTE_TO_PA(pte)   (((pte) >> 10) << 12)

extern char __kernel_start[];
extern char __user_start[];
extern char __user_end[];
extern char __kernel_end[];

static unsigned long *vm_root;

static unsigned long *vm_walk(unsigned long va) {
    unsigned long *table = vm_root;

    for (int level = 2; level > 0; level--) {
        unsigned long *pte = &table[(va >> (12 + 9 * level)) & 0x1ff];

        if ((*pte & PTE_V) == 0) {
            *pte = PTE_FROM_PA(page_alloc()) | PTE_V;
        }

        table = (unsigned long *)PTE_TO_PA(*pte);
    }

    return &table[(va >> 12) & 0x1ff];
}

static void vm_map(unsigned long va, unsigned long pa, unsigned long size, unsigned long perm) {
    for (unsigned long off = 0; off < size; off += PAGE_SIZE) {
        *vm_walk(va + off) = PTE_FROM_PA(pa + off) | perm | PTE_A | PTE_D | PTE_V;
    }
}

static void vm_map_identity(unsigned long start, unsigned long end, unsigned long perm) {
    vm_map(start, start, end - start, perm);
}

void vm_init(unsigned long ram_end) {
    vm_root = page_alloc();

    vm_map_identity(VIRT_PLIC, VIRT_PLIC + VIRT_PLIC_SIZE, PTE_R | PTE_W);
    vm_map_identity(VIRT_UART0, VIRT_UART0 + VIRT_UART0_SIZE, PTE_R | PTE_W);
    vm_map_identity((unsigned long)__kernel_start, (unsigned long)__user_start, PTE_R | PTE_W | PTE_X);
    vm_map_identity((unsigned long)__user_start, (unsigned long)__user_end, PTE_R | PTE_W | PTE_X | PTE_U);
    vm_map_identity((unsigned long)__kernel_end, ram_end, PTE_R | PTE_W);

    csr_write(satp, SATP_SV39 | ((unsigned long)vm_root >> 12));
    __asm__ volatile ("sfence.vma");
}
