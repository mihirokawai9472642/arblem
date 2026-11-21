// kernel_main: bring up each subsystem in order and start the shell.
// See docs/boot.md.

#include "console.h"
#include "fd.h"
#include "fdt.h"
#include "kprintf.h"
#include "page.h"
#include "panic.h"
#include "plic.h"
#include "proc.h"
#include "sbi.h"
#include "thread.h"
#include "timer.h"
#include "uart.h"
#include "vm.h"

// First byte after the kernel image, page aligned (kernel/linker.ld).
extern char __kernel_end[];

// Called from _start in boot.S with the arguments OpenSBI passed in a0 and a1.
// Interrupts stay globally off (sstatus.SIE = 0) until the first thread runs.
void kernel_main(unsigned long hartid, unsigned long dtb) {
    kprintf("Hello World from Arblem :)\n");

    kprintf("HartID: %x\n", hartid);
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
    timer_init(info.timebase_freq);

    int shell = proc_spawn("shell", 0, 0);

    if (shell < 0) {
        panic("cannot start the shell");
    }

    // The shell was created before any thread ran, so it inherited no files.
    // Give it the console, which the programs it spawns then inherit.
    thread_set_file(shell, FD_STDIN, console_file());
    thread_set_file(shell, FD_STDOUT, console_file());
    thread_set_file(shell, FD_STDERR, console_file());

    // Does not return.
    thread_start();
}
