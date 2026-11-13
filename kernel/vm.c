#include "vm.h"
#include "page.h"
#include "panic.h"
#include "riscv.h"
#include "virt.h"

#define PTE_FROM_PA(pa) (((unsigned long)(pa) >> 12) << 10)
#define PTE_TO_PA(pte)   (((pte) >> 10) << 12)

#define VM_VA_END (1UL << 38)

extern char __kernel_start[];
extern char __kernel_end[];

static unsigned long *vm_kernel_root;
static unsigned long *vm_current_root;

static unsigned long *vm_walk(unsigned long *root, unsigned long va, int alloc) {
    unsigned long *table = root;

    for (int level = 2; level > 0; level--) {
        unsigned long *pte = &table[(va >> (12 + 9 * level)) & 0x1ff];

        if ((*pte & PTE_V) == 0) {
            if (!alloc) {
                return 0;
            }

            void *page = page_alloc();

            if (page == 0) {
                return 0;
            }

            *pte = PTE_FROM_PA(page) | PTE_V;
        }

        table = (unsigned long *)PTE_TO_PA(*pte);
    }

    return &table[(va >> 12) & 0x1ff];
}

int vm_map(unsigned long *root, unsigned long va, unsigned long pa, unsigned long size, unsigned long perm) {
    for (unsigned long off = 0; off < size; off += PAGE_SIZE) {
        unsigned long *pte = vm_walk(root, va + off, 1);

        if (pte == 0) {
            return -1;
        }

        *pte = PTE_FROM_PA(pa + off) | perm | PTE_A | PTE_D | PTE_V;
    }

    return 0;
}

static void vm_map_identity(unsigned long start, unsigned long end, unsigned long perm) {
    if (vm_map(vm_kernel_root, start, start, end - start, perm) != 0) {
        panic("out of memory while mapping the kernel");
    }
}

void vm_init(unsigned long ram_end) {
    vm_kernel_root = page_alloc();

    if (vm_kernel_root == 0) {
        panic("out of memory while mapping the kernel");
    }

    vm_map_identity(VIRT_PLIC, VIRT_PLIC + VIRT_PLIC_SIZE, PTE_R | PTE_W);
    vm_map_identity(VIRT_UART0, VIRT_UART0 + VIRT_UART0_SIZE, PTE_R | PTE_W);
    vm_map_identity((unsigned long)__kernel_start, (unsigned long)__kernel_end, PTE_R | PTE_W | PTE_X);
    vm_map_identity((unsigned long)__kernel_end, ram_end, PTE_R | PTE_W);

    vm_switch(vm_kernel_root);
}

unsigned long *vm_create(void) {
    unsigned long *root = page_alloc();

    if (root == 0) {
        return 0;
    }

    for (int i = 0; i < 512; i++) {
        root[i] = vm_kernel_root[i];
    }

    return root;
}

void vm_switch(unsigned long *root) {
    if (root == 0) {
        root = vm_kernel_root;
    }

    vm_current_root = root;

    csr_write(satp, SATP_SV39 | ((unsigned long)root >> 12));
    __asm__ volatile ("sfence.vma" : : : "memory");
}

static void vm_free_table(unsigned long *table, int level) {
    for (int i = 0; i < 512; i++) {
        if ((table[i] & PTE_V) == 0) {
            continue;
        }

        if (level > 0) {
            vm_free_table((unsigned long *)PTE_TO_PA(table[i]), level - 1);
        } else {
            page_free((void *)PTE_TO_PA(table[i]));
        }
    }

    page_free(table);
}

void vm_free(unsigned long *root) {
    for (int i = 0; i < 512; i++) {
        if (root[i] != vm_kernel_root[i]) {
            vm_free_table((unsigned long *)PTE_TO_PA(root[i]), 1);
        }
    }

    page_free(root);
}

static int vm_check_user(unsigned long addr, unsigned long len, unsigned long perm) {
    unsigned long end = addr + len;

    if (end < addr || end > VM_VA_END) {
        return -1;
    }

    for (unsigned long va = addr & ~(PAGE_SIZE - 1); va < end; va += PAGE_SIZE) {
        unsigned long *pte = vm_walk(vm_current_root, va, 0);

        if (pte == 0 || (*pte & (PTE_V | PTE_U | perm)) != (PTE_V | PTE_U | perm)) {
            return -1;
        }
    }

    return 0;
}

int vm_copy_from_user(void *dst, unsigned long src, unsigned long len) {
    if (vm_check_user(src, len, PTE_R) != 0) {
        return -1;
    }

    char *d = dst;
    const char *s = (const char *)src;

    csr_set(sstatus, SSTATUS_SUM);

    for (unsigned long i = 0; i < len; i++) {
        d[i] = s[i];
    }

    csr_clear(sstatus, SSTATUS_SUM);

    return 0;
}

int vm_copy_to_user(unsigned long dst, const void *src, unsigned long len) {
    if (vm_check_user(dst, len, PTE_W) != 0) {
        return -1;
    }

    char *d = (char *)dst;
    const char *s = src;

    csr_set(sstatus, SSTATUS_SUM);

    for (unsigned long i = 0; i < len; i++) {
        d[i] = s[i];
    }

    csr_clear(sstatus, SSTATUS_SUM);

    return 0;
}
