#include "proc.h"
#include "page.h"
#include "riscv.h"
#include "str.h"
#include "thread.h"
#include "trap.h"
#include "vm.h"

#define PROC_BASE      0x40000000UL
#define PROC_STACK_TOP 0x80000000UL

struct proc_image {
    const char *name;
    const char *start;
    const char *end;
};

extern char user_hello[], user_hello_end[];
extern char user_count[], user_count_end[];
extern char user_shell[], user_shell_end[];

static const struct proc_image proc_images[] = {
    {"hello", user_hello, user_hello_end},
    {"count", user_count, user_count_end},
    {"shell", user_shell, user_shell_end},
};

static void proc_entry(void) {
    trap_enter_user(PROC_BASE, PROC_STACK_TOP);
}

static int proc_create(const char *image, unsigned long size) {
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

    int id = thread_create(proc_entry, vm);

    if (id < 0) {
        vm_free(vm);
    }

    return id;
}

int proc_spawn(const char *name) {
    for (unsigned long i = 0; i < sizeof(proc_images) / sizeof(proc_images[0]); i++) {
        if (str_equal(name, proc_images[i].name)) {
            return proc_create(proc_images[i].start, proc_images[i].end - proc_images[i].start);
        }
    }

    return -1;
}
