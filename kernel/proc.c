// Processes: load an embedded ELF program into a new address space and start
// a thread that enters it in U-mode. See docs/processes.md.

#include "proc.h"
#include "elf.h"
#include "error.h"
#include "page.h"
#include "riscv.h"
#include "str.h"
#include "thread.h"
#include "trap.h"
#include "vm.h"

// User programs live in [PROC_BASE, PROC_STACK_TOP), which is exactly one
// top-level page table entry (see vm_create). The top page is the stack.
#define PROC_BASE      0x40000000UL
#define PROC_STACK_TOP 0x80000000UL

// An ELF file embedded in the kernel image by user.S.
struct proc_image {
    const char *name;
    const char *start;
    const char *end;
};

extern char user_hello[], user_hello_end[];
extern char user_count[], user_count_end[];
extern char user_fault[], user_fault_end[];
extern char user_orphan[], user_orphan_end[];
extern char user_regcheck[], user_regcheck_end[];
extern char user_regclobber[], user_regclobber_end[];
extern char user_pipecheck[], user_pipecheck_end[];
extern char user_upper[], user_upper_end[];
extern char user_shell[], user_shell_end[];

// The programs proc_spawn can start, by name.
static const struct proc_image proc_images[] = {
    {"hello", user_hello, user_hello_end},
    {"count", user_count, user_count_end},
    {"fault", user_fault, user_fault_end},
    {"orphan", user_orphan, user_orphan_end},
    {"regcheck", user_regcheck, user_regcheck_end},
    {"regclobber", user_regclobber, user_regclobber_end},
    {"pipecheck", user_pipecheck, user_pipecheck_end},
    {"upper", user_upper, user_upper_end},
    {"shell", user_shell, user_shell_end},
};

// Thread entry function of every process: enter U-mode at the ELF entry point
// with the stack and argc/argv prepared by proc_create. Does not return.
static void proc_entry(unsigned long entry) {
    unsigned long sp = 0;
    unsigned long argc = 0;
    unsigned long argv = 0;

    thread_get_start(&sp, &argc, &argv);
    trap_enter_user(entry, sp, argc, argv);
}

// Allocate a zeroed page and map it at va. Returns the page's kernel address,
// or 0 when out of memory. On failure nothing stays allocated except page
// tables, which vm_free releases with the rest of the address space.
static char *proc_map_page(unsigned long *vm, unsigned long va,
                           unsigned long perm) {
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

// Translate ELF segment flags into PTE flags. RISC-V has no write-only pages,
// so a writable segment is made readable too.
static unsigned long proc_perm(unsigned int flags) {
    unsigned long perm = PTE_U;

    if (flags & (ELF_PF_R | ELF_PF_W)) {
        perm |= PTE_R;
    }

    if (flags & ELF_PF_W) {
        perm |= PTE_W;
    }

    if (flags & ELF_PF_X) {
        perm |= PTE_X;
    }

    return perm;
}

// Map and fill one PT_LOAD segment. *next is the first address the segment may
// use; it is advanced past the segment, so segments must come in address order
// and never share a page. Bytes past filesz stay zero (.bss).
static int proc_load(unsigned long *vm, const char *image,
                     const struct elf_phdr *ph, unsigned long *next) {
    unsigned long end = PROC_STACK_TOP - PAGE_SIZE;

    if (ph->vaddr % PAGE_SIZE != 0 || ph->vaddr < *next || ph->vaddr > end ||
        ph->memsz > end - ph->vaddr) {
        return -ERR_NOEXEC;
    }

    // W^X: refuse segments that are both writable and executable.
    if ((ph->flags & ELF_PF_W) && (ph->flags & ELF_PF_X)) {
        return -ERR_NOEXEC;
    }

    for (unsigned long off = 0; off < ph->memsz; off += PAGE_SIZE) {
        char *page = proc_map_page(vm, ph->vaddr + off, proc_perm(ph->flags));

        if (page == 0) {
            return -ERR_NOMEM;
        }

        for (unsigned long i = 0; i < PAGE_SIZE && off + i < ph->filesz; i++) {
            page[i] = image[ph->offset + off + i];
        }
    }

    *next = (ph->vaddr + ph->memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1UL);

    return 0;
}

// Lay out argv strings and the NULL-terminated argv pointer array at the top
// of the initial stack page. Stores the resulting 16-byte aligned user sp and
// argv address in *sp_out and *argv_out.
static int proc_build_stack(char *stack_page, int argc, const char *const *argv,
                            unsigned long *sp_out, unsigned long *argv_out) {
    if (argc <= 0 || argc > PROC_ARG_MAX) {
        return -ERR_2BIG;
    }

    unsigned long stack_va = PROC_STACK_TOP - PAGE_SIZE;
    unsigned long off = PAGE_SIZE;
    unsigned long uaddrs[PROC_ARG_MAX];

    for (int i = 0; i < argc; i++) {
        unsigned long len = str_len(argv[i]) + 1;

        if (len > off) {
            return -ERR_2BIG;
        }

        off -= len;

        for (unsigned long j = 0; j < len; j++) {
            stack_page[off + j] = argv[i][j];
        }

        uaddrs[i] = stack_va + off;
    }

    // Reserve (argc + 1) pointers (argv[0..argc-1] and the trailing NULL),
    // aligned down to 16 bytes for the RISC-V LP64D ABI.
    unsigned long table_bytes =
        (unsigned long)(argc + 1) * sizeof(unsigned long);

    if (table_bytes > off) {
        return -ERR_2BIG;
    }

    off = (off - table_bytes) & ~15UL;

    unsigned long *utab = (unsigned long *)(stack_page + off);

    for (int i = 0; i < argc; i++) {
        utab[i] = uaddrs[i];
    }

    utab[argc] = 0;

    *sp_out = stack_va + off;
    *argv_out = stack_va + off;

    return 0;
}

// Build an address space from an ELF image, map a one-page stack with
// argc/argv, and create the thread. Returns the thread ID or a negative error;
// on error everything allocated so far is freed.
static int proc_create(const char *image, unsigned long size, int argc,
                       const char *const *argv) {
    if (elf_check(image, size) != 0) {
        return -ERR_NOEXEC;
    }

    const struct elf_header *eh = (const struct elf_header *)image;
    const struct elf_phdr *ph = (const struct elf_phdr *)(image + eh->phoff);

    unsigned long *vm = vm_create();

    if (vm == 0) {
        return -ERR_NOMEM;
    }

    unsigned long next = PROC_BASE;
    int err;

    for (int i = 0; i < eh->phnum; i++) {
        if (ph[i].type != ELF_PT_LOAD) {
            continue;
        }

        err = proc_load(vm, image, &ph[i], &next);

        if (err < 0) {
            goto fail;
        }
    }

    char *stack_page =
        proc_map_page(vm, PROC_STACK_TOP - PAGE_SIZE, PTE_R | PTE_W | PTE_U);

    if (stack_page == 0) {
        err = -ERR_NOMEM;
        goto fail;
    }

    unsigned long user_sp = 0;
    unsigned long user_argv = 0;

    err = proc_build_stack(stack_page, argc, argv, &user_sp, &user_argv);

    if (err < 0) {
        goto fail;
    }

    int id = thread_create(proc_entry, eh->entry, vm);

    if (id < 0) {
        err = id;
        goto fail;
    }

    thread_set_start(id, user_sp, (unsigned long)argc, user_argv);

    return id;

fail:
    vm_free(vm);

    return err;
}

int proc_spawn(const char *name, int argc, const char *const *argv) {
    const char *default_argv[1] = {name};

    if (argc == 0 || argv == 0) {
        argc = 1;
        argv = default_argv;
    }

    for (unsigned long i = 0; i < sizeof(proc_images) / sizeof(proc_images[0]);
         i++) {
        if (str_equal(name, proc_images[i].name)) {
            return proc_create(proc_images[i].start,
                               proc_images[i].end - proc_images[i].start, argc,
                               argv);
        }
    }

    return -ERR_NOENT;
}
