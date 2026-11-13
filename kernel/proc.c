#include "proc.h"
#include "error.h"
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

static char *proc_map_page(unsigned long *vm, unsigned long va, unsigned long perm) {
    char *page = page_alloc();

    if (page == 0) {
        return 0;
    }

    if (vm_map(vm, va, (unsigned long)page, PAGE_SIZE, perm) != 0) {
        page_free(page);
        return 0;
    }

    return page;
}

static int proc_create(const char *image, unsigned long size) {
    unsigned long *vm = vm_create();

    if (vm == 0) {
        return -ERR_NOMEM;
    }

    int err = -ERR_NOMEM;

    for (unsigned long off = 0; off < size; off += PAGE_SIZE) {
        char *page = proc_map_page(vm, PROC_BASE + off, PTE_R | PTE_W | PTE_X | PTE_U);

        if (page == 0) {
            goto fail;
        }

        for (unsigned long i = 0; i < PAGE_SIZE && off + i < size; i++) {
            page[i] = image[off + i];
        }
    }

    if (proc_map_page(vm, PROC_STACK_TOP - PAGE_SIZE, PTE_R | PTE_W | PTE_U) == 0) {
        goto fail;
    }

    int id = thread_create(proc_entry, vm);

    if (id < 0) {
        err = id;
        goto fail;
    }

    return id;

fail:
    vm_free(vm);

    return err;
}

int proc_spawn(const char *name) {
    for (unsigned long i = 0; i < sizeof(proc_images) / sizeof(proc_images[0]); i++) {
        if (str_equal(name, proc_images[i].name)) {
            return proc_create(proc_images[i].start, proc_images[i].end - proc_images[i].start);
        }
    }

    return -ERR_NOENT;
}
