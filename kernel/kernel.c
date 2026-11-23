// kernel_main: bring up each subsystem in order and start the shell.
// See docs/boot.md.

#include "bcache.h"
#include "console.h"
#include "cpu.h"
#include "fd.h"
#include "fdt.h"
#include "kfs.h"
#include "kprintf.h"
#include "page.h"
#include "panic.h"
#include "plic.h"
#include "proc.h"
#include "sbi.h"
#include "thread.h"
#include "timer.h"
#include "uart.h"
#include "virtio_blk.h"
#include "vm.h"

// First byte after the kernel image, page aligned (kernel/linker.ld).
extern char __kernel_end[];

// First kernel thread: runs once the scheduler and idle thread exist, so
// fs_init and disk-backed proc_spawn can sleep on VirtIO DMA interrupts.
static void kernel_init(unsigned long arg) {
    (void)arg;

    if (fs_init() == 0) {
        struct fs_super sb;

        fs_super_info(&sb);
        kprintf("FS: %x blocks, %x inodes\n", (unsigned long)sb.total_blocks,
                (unsigned long)sb.num_inodes);
    } else {
        kprintf("FS: none\n");
    }

    // Give kernel_init the console so shell inherits stdin, stdout, and stderr
    // atomically during proc_spawn before another hart can run it.
    if (thread_alloc_fd(console_file()) != FD_STDIN ||
        thread_alloc_fd(console_file()) != FD_STDOUT ||
        thread_alloc_fd(console_file()) != FD_STDERR) {
        panic("cannot attach console");
    }

    int shell = proc_spawn("shell", 0, 0);

    if (shell < 0) {
        panic("cannot start the shell");
    }

    // Orphan itself so thread_reap frees this boot thread's stack and slot.
    thread_exit(0);
}

// Called from _start in boot.S with the arguments OpenSBI passed in a0 and a1.
// Interrupts stay globally off (sstatus.SIE = 0) until the first thread runs.
void kernel_main(unsigned long hartid, unsigned long dtb) {
    cpu_init(hartid);

    kprintf("Hello World from Arblem :)\n");

    kprintf("HartID: %x\n", cpu_current()->hartid);
    kprintf("DTB: %x\n", dtb);
    kprintf("SBI: %x\n", sbi_spec_version());

    struct fdt_info info;

    if (fdt_parse(dtb, &info) != 0) {
        panic("invalid device tree");
    }

    kprintf("RAM: %x + %x\n", info.ram_base, info.ram_size);
    kprintf("UART: %x (irq %x)\n", info.uart.base,
            (unsigned long)info.uart.irq);
    kprintf("PLIC: %x + %x\n", info.plic.base, info.plic.size);
    kprintf("VirtIO: %x slots\n", (unsigned long)info.virtio_count);

    if (info.ram_size == 0 || info.uart.size == 0 || info.plic.size == 0 ||
        info.timebase_freq < 100) {
        panic("incomplete device tree");
    }

    unsigned long kernel_end = (unsigned long)__kernel_end;
    unsigned long ram_end =
        (info.ram_base + info.ram_size) & ~(PAGE_SIZE - 1UL);
    unsigned long dtb_start = dtb & ~(PAGE_SIZE - 1UL);
    unsigned long dtb_end =
        (dtb + info.dtb_size + PAGE_SIZE - 1UL) & ~(PAGE_SIZE - 1UL);

    if (ram_end <= kernel_end) {
        panic("no usable RAM");
    }

    // Reserve the DTB pages, and add usable RAM below and above the DTB.
    if (dtb_start >= kernel_end && dtb_start < ram_end) {
        page_init(kernel_end, dtb_start);

        if (dtb_end < ram_end) {
            page_init(dtb_end, ram_end);
        }
    } else {
        page_init(kernel_end, ram_end);
    }

    vm_init(&info);

    uart_init(info.uart.base, info.uart.irq);
    plic_init(info.plic.base);
    plic_enable(info.uart.irq);

    if (virtio_blk_init(&info) == 0) {
        plic_enable(virtio_blk_irq());
        kprintf("Disk: %x sectors (irq %x)\n", virtio_blk_sectors(),
                (unsigned long)virtio_blk_irq());
    } else {
        kprintf("Disk: none\n");
    }

    bcache_init();
    timer_init(info.timebase_freq);

    int init = thread_create(kernel_init, 0, 0);

    if (init < 0) {
        panic("cannot start kernel_init");
    }

    thread_ready(init);

    // Does not return.
    thread_start();
}
