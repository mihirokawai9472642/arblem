// Check sbrk, lazy heap allocation, demand paging through syscalls, stack
// growth past one page, and the unmapped stack guard page. Prints "mem: ok"
// when every check passes. The smoke test runs this.

#include "error.h"
#include "str.h"
#include "sys.h"

#define PAGE_SIZE 4096L

// Recursively consume more than 64 KiB of stack so the child hits the unmapped
// guard page and is killed by the kernel.
static void blow_stack(int depth) {
    volatile char pad[PAGE_SIZE];

    pad[0] = (char)depth;

    if (depth > 0) {
        blow_stack(depth - 1);
    }

    pad[PAGE_SIZE - 1] = pad[0];
}

// Check sbrk bounds, lazy allocation (0 pages until touched), zero-fill on
// demand fault, and freeing pages on shrink.
static int check_sbrk_lazy(void) {
    long base = sys_sbrk(0);

    if (base < 0) {
        return -1;
    }

    // Cannot shrink below heap_base or grow past PROC_HEAP_LIMIT.
    if (sys_sbrk(-1) != -ERR_NOMEM || sys_sbrk(0x40000000L) != -ERR_NOMEM) {
        return -1;
    }

    unsigned long before = sys_pages();
    long p = sys_sbrk(4 * PAGE_SIZE);

    if (p != base || sys_sbrk(0) != base + 4 * PAGE_SIZE) {
        return -1;
    }

    // Growing the break must not allocate physical pages yet.
    if (sys_pages() != before) {
        return -1;
    }

    volatile char *buf = (volatile char *)p;

    // Touch page 0 and page 3 only: each fault must give a zeroed page.
    if (buf[0] != 0 || buf[3 * PAGE_SIZE] != 0) {
        return -1;
    }

    buf[0] = 'A';
    buf[3 * PAGE_SIZE] = 'Z';

    if (buf[0] != 'A' || buf[3 * PAGE_SIZE] != 'Z') {
        return -1;
    }

    // Exactly two leaf pages were faulted in (within the existing L0 table).
    if (sys_pages() != before - 2) {
        return -1;
    }

    // Shrinking back to base must unmap and free both touched pages.
    if (sys_sbrk(-4 * PAGE_SIZE) != base + 4 * PAGE_SIZE ||
        sys_sbrk(0) != base || sys_pages() != before) {
        return -1;
    }

    return 0;
}

// Check demand paging inside vm_check_user across a 2 MiB boundary (a fresh
// level-0 page table) by passing an untouched heap page to sys_read.
static int check_syscall_heap(void) {
    long span = 2L * 1024 * 1024;
    long p = sys_sbrk(span);

    if (p < 0) {
        return -1;
    }

    int fds[2] = {-1, -1};

    if (sys_pipe(fds) != 0) {
        return -1;
    }

    if (sys_write(fds[1], "lazy", 4) != 4) {
        return -1;
    }

    char *target = (char *)(p + span - PAGE_SIZE);

    if (sys_read(fds[0], target, 4) != 4 || target[0] != 'l' ||
        target[1] != 'a' || target[2] != 'z' || target[3] != 'y') {
        return -1;
    }

    if (sys_close(fds[0]) != 0 || sys_close(fds[1]) != 0 ||
        sys_sbrk(-span) < 0) {
        return -1;
    }

    return 0;
}

// Use 24 KiB (6 pages) on the stack to check automatic stack growth past the
// initial 4 KiB page.
static int check_stack_growth(void) {
    volatile char big[6 * PAGE_SIZE];

    for (long off = 0; off < (long)sizeof(big); off += PAGE_SIZE) {
        big[off] = (char)(off / PAGE_SIZE + 1);
    }

    for (long off = 0; off < (long)sizeof(big); off += PAGE_SIZE) {
        if (big[off] != (char)(off / PAGE_SIZE + 1)) {
            return -1;
        }
    }

    return 0;
}

// Spawn "memcheck overflow" and check that hitting the guard page kills it
// with exit status -1.
static int check_guard_page(void) {
    const char *argv[] = {"memcheck", "overflow", 0};
    long pid = sys_spawnv("memcheck", str_len("memcheck"), argv);

    if (pid < 0) {
        return -1;
    }

    int status = 0;

    if (sys_wait(pid, &status) != 0 || status != -1) {
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && str_equal(argv[1], "overflow")) {
        blow_stack(20);

        return 0;
    }

    if (check_sbrk_lazy() != 0) {
        sys_puts("mem: sbrk failed\n");

        return 1;
    }

    if (check_syscall_heap() != 0) {
        sys_puts("mem: syscall heap failed\n");

        return 1;
    }

    if (check_stack_growth() != 0) {
        sys_puts("mem: stack failed\n");

        return 1;
    }

    if (check_guard_page() != 0) {
        sys_puts("mem: guard failed\n");

        return 1;
    }

    sys_puts("mem: ok\n");

    return 0;
}
