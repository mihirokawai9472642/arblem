// Sv39 virtual memory: the kernel page table, user page tables, and checked
// copies to and from user memory. See docs/memory.md.
//
// Kernel memory is identity-mapped (virtual = physical) in every page table,
// so the kernel can follow the physical addresses stored in page table
// entries directly.

#include "vm.h"
#include "error.h"
#include "page.h"
#include "panic.h"
#include "riscv.h"
#include "thread.h"
#include "virt.h"

// A PTE holds the physical page number in bits 53:10.
#define PTE_FROM_PA(pa) (((unsigned long)(pa) >> 12) << 10)
#define PTE_TO_PA(pte)  (((pte) >> 10) << 12)

// End of the lower half of the Sv39 address space. User ranges must lie
// below it.
#define VM_VA_END (1UL << 38)

// Section boundaries from kernel/linker.ld.
extern char __kernel_start[];
extern char __text_end[];
extern char __rodata_end[];

static unsigned long *vm_kernel_root;
static unsigned long *vm_current_root;

// Return the address of the level 0 (leaf) PTE for va. Missing level 1 and
// level 0 tables are allocated if alloc is set; otherwise, or when out of
// memory, return 0.
static unsigned long *vm_walk(unsigned long *root, unsigned long va,
                              int alloc) {
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

            // V with R, W and X clear: points to the next level table.
            *pte = PTE_FROM_PA(page) | PTE_V;
        }

        table = (unsigned long *)PTE_TO_PA(*pte);
    }

    return &table[(va >> 12) & 0x1ff];
}

// A and D are set up front, because some hardware faults on a clear A or D
// bit instead of setting it, and the kernel does not use them.
int vm_map(unsigned long *root, unsigned long va, unsigned long pa,
           unsigned long size, unsigned long perm) {
    for (unsigned long off = 0; off < size; off += PAGE_SIZE) {
        unsigned long *pte = vm_walk(root, va + off, 1);

        if (pte == 0) {
            return -1;
        }

        *pte = PTE_FROM_PA(pa + off) | perm | PTE_A | PTE_D | PTE_V;
    }

    return 0;
}

void vm_unmap(unsigned long *root, unsigned long va) {
    unsigned long *pte = vm_walk(root, va, 0);

    if (pte == 0 || (*pte & PTE_V) == 0) {
        return;
    }

    page_free((void *)PTE_TO_PA(*pte));
    *pte = 0;
    __asm__ volatile("sfence.vma" : : : "memory");
}

int vm_is_mapped(unsigned long *root, unsigned long va) {
    unsigned long *pte = vm_walk(root, va, 0);

    return pte != 0 && (*pte & PTE_V) != 0;
}

static void vm_map_identity(unsigned long start, unsigned long end,
                            unsigned long perm) {
    if (start % PAGE_SIZE != 0 || end % PAGE_SIZE != 0) {
        panic("unaligned kernel mapping");
    }

    if (vm_map(vm_kernel_root, start, start, end - start, perm) != 0) {
        panic("out of memory while mapping the kernel");
    }
}

void vm_init(unsigned long ram_end) {
    vm_kernel_root = page_alloc();

    if (vm_kernel_root == 0) {
        panic("out of memory while mapping the kernel");
    }

    // No PTE_U anywhere, so U-mode cannot touch any of this. Code is not
    // writable and data is not executable (W^X).
    vm_map_identity(VIRT_PLIC, VIRT_PLIC + VIRT_PLIC_SIZE, PTE_R | PTE_W);
    vm_map_identity(VIRT_UART0, VIRT_UART0 + VIRT_UART0_SIZE, PTE_R | PTE_W);
    vm_map_identity((unsigned long)__kernel_start, (unsigned long)__text_end,
                    PTE_R | PTE_X);
    vm_map_identity((unsigned long)__text_end, (unsigned long)__rodata_end,
                    PTE_R);
    vm_map_identity((unsigned long)__rodata_end, ram_end, PTE_R | PTE_W);

    // The code keeps running after this because it is mapped at the address
    // it already runs at.
    vm_switch(vm_kernel_root);
}

// Copy all top-level entries of the kernel table, so the kernel's level 1 and
// level 0 tables are shared by every address space. User mappings go in the
// top-level entry for 0x40000000-0x7fffffff, which the kernel leaves empty,
// so they never modify a shared table.
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

    // satp = mode | physical page number of the root table. sfence.vma
    // flushes the TLB entries of the previous page table.
    csr_write(satp, SATP_SV39 | ((unsigned long)root >> 12));
    __asm__ volatile("sfence.vma" : : : "memory");
}

// Free a table and everything below it. At level 0, the entries point to
// leaf pages, which belong to the process and are freed too.
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

// Top-level entries that differ from the kernel's are the process's own.
// Shared kernel entries are left alone.
void vm_free(unsigned long *root) {
    for (int i = 0; i < 512; i++) {
        if (root[i] != vm_kernel_root[i]) {
            vm_free_table((unsigned long *)PTE_TO_PA(root[i]), 1);
        }
    }

    page_free(root);
}

// Check that every page of [addr, addr + len) is mapped in the active page
// table as a valid user page with permission perm (PTE_R or PTE_W). Missing
// pages in demand-paged heap or stack regions are faulted in on the spot.
static int vm_check_user(unsigned long addr, unsigned long len,
                         unsigned long perm) {
    unsigned long end = addr + len;

    // Reject ranges that wrap around or leave the lower half.
    if (end < addr || end > VM_VA_END) {
        return -1;
    }

    for (unsigned long va = addr & ~(PAGE_SIZE - 1); va < end;
         va += PAGE_SIZE) {
        unsigned long *pte = vm_walk(vm_current_root, va, 0);

        if (pte == 0 || (*pte & PTE_V) == 0) {
            if (thread_fault(va) != 0) {
                return -1;
            }

            pte = vm_walk(vm_current_root, va, 0);
        }

        if (pte == 0 ||
            (*pte & (PTE_V | PTE_U | perm)) != (PTE_V | PTE_U | perm)) {
            return -1;
        }
    }

    return 0;
}

// The copies use the user address directly, since the active page table is
// the calling program's. S-mode may only access U pages while sstatus.SUM is
// set, so SUM is on for the copy alone.
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

int vm_copy_str_from_user(char *dst, unsigned long src, unsigned long max) {
    for (unsigned long i = 0; i < max; i++) {
        char c = 0;

        if (vm_copy_from_user(&c, src + i, 1) != 0) {
            return -ERR_FAULT;
        }

        dst[i] = c;

        if (c == '\0') {
            return (int)i;
        }
    }

    return -ERR_2BIG;
}
