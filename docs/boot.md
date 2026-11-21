# Boot

This page follows the machine from reset to the shell's first prompt.

## 1. QEMU and OpenSBI

`make run` starts QEMU with:

```
qemu-system-riscv64 -machine virt -nographic -bios default \
    -global virtio-mmio.force-legacy=false \
    -drive file=build/disk.img,if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0,bus=virtio-mmio-bus.0 \
    -kernel build/kernel.elf
```

- `-bios default` loads QEMU's bundled OpenSBI at `0x80000000` and runs it in
  M-mode.
- `-global virtio-mmio.force-legacy=false` configures QEMU's VirtIO MMIO slots
  for Modern VirtIO 1.0+ (MMIO v2).
- `-drive ... -device virtio-blk-device,...` attaches the 1 MiB raw disk image
  `build/disk.img` (created automatically if missing) as a VirtIO block device.
- `-kernel` loads `build/kernel.elf` at its link address, `0x80200000`.

OpenSBI sets up M-mode, delegates the S-mode interrupts and exceptions to
S-mode, and jumps to `0x80200000` in S-mode with:

| Register | Value                                        |
|----------|----------------------------------------------|
| `a0`     | ID of the booting hart                       |
| `a1`     | Physical address of the device tree (DTB)    |

Paging is off at this point (`satp` is 0).

## 2. `_start` (`kernel/boot.S`)

The linker script puts `.text.boot` first, so `_start` is at `0x80200000`.

1. Point `sp` at a 4 KiB boot stack reserved in `.bss`.
2. Clear `.bss` (`__bss_start` to `__bss_end`) byte by byte. The boot stack
   is in `.bss` too, which is fine because nothing has been pushed on it yet.
3. Set `stvec` to `trap_entry`, so any trap from now on goes to the kernel.
4. Set `sscratch` to 0. This marks "the hart is in S-mode" for the trap entry
   code (see [Traps](traps.md#the-sscratch-convention)).
5. Call `kernel_main(hartid, dtb)`, keeping `a0` and `a1` from OpenSBI.

## 3. `kernel_main` (`kernel/kernel.c`)

| Step                                   | What it does                                                         |
|----------------------------------------|----------------------------------------------------------------------|
| `kprintf(...)`                         | Print the banner, hart ID, DTB address and SBI version through SBI   |
| `fdt_parse(dtb, &info)`                | Parse the DTB for RAM, timer frequency, PLIC, UART, and VirtIO MMIO slots; print detected hardware and validate required fields |
| `page_init(...)` (below and above DTB) | Reserve the DTB pages `[dtb_start, dtb_end)` and add usable RAM below (`[__kernel_end, dtb_start)`) and above (`[dtb_end, ram_end)`) to the free list |
| `vm_init(&info)`                       | Build the kernel page table (MMIO devices, kernel, and RAM up to `ram_end`) and turn on Sv39 paging |
| `uart_init(base, irq)`                 | Record the UART MMIO base and IRQ and enable its receive interrupt   |
| `plic_init(base)`, `plic_enable(uart.irq)` | Record the PLIC base, route UART interrupts to this hart's S-mode, and enable `sie.SEIE` |
| `virtio_blk_init(&info)`, `plic_enable(irq)` | Probe VirtIO MMIO slots for a Modern v2 block device, initialize queue 0, and enable its PLIC IRQ if found |
| `timer_init(timebase_freq)`            | Compute the 100 Hz interval from the DTB's `timebase-frequency`, arm the first timer interrupt, and enable `sie.STIE` |
| `proc_spawn("shell", 0, 0)`            | Load the shell's ELF into a new address space, in a new thread       |
| `thread_set_file(...)` x3              | Give the shell the console as stdin, stdout and stderr               |
| `thread_start()`                       | Create the idle thread and switch to thread 0                        |

`sie` bits only enable interrupt *sources*. The global switch `sstatus.SIE`
stays off during `kernel_main`, so no interrupt arrives until the first thread
runs.

`fdt_parse` runs before paging is turned on, while the DTB can be read directly
at its physical address. `kernel_main` keeps the DTB's own pages reserved (out
of the free list) and reclaims any usable RAM above them up to `ram_end`.

## 4. The first thread

`thread_start` creates the idle thread and switches to slot 0 of the thread
table. The shell was created first, so slot 0 holds the shell, and the idle
thread is in slot 1. The switch abandons the boot stack: from here on, code
always runs on a thread's own kernel stack.

The shell thread starts in `thread_entry` (`kernel/thread.S`), which calls
`proc_entry` (`kernel/proc.c`), which calls `trap_enter_user`
(`kernel/trap.S`). That last function fakes a return from a trap: it sets
`sepc` to the ELF entry point, clears `sstatus.SPP`, and executes `sret`, which
drops the hart into U-mode at `_start` in `user/start.c`. The shell then prints
`> `.

## Physical memory layout

QEMU's `virt` machine has 128 MiB of RAM by default.

```
0x0c000000  PLIC (6 MiB)
0x10000000  UART0 (16550)
0x10001000  VirtIO MMIO (8 slots, 4 KiB each)
...
0x80000000  OpenSBI
0x80200000  kernel .text        (__kernel_start)
            kernel .rodata      (__text_end)
            kernel .data, .bss  (__rodata_end), boot stack in .bss
            free pages          (__kernel_end, page aligned)
            ...
dtb         device tree (reserved pages, placed near top of RAM)
            free pages          (above DTB, up to ram_end)
0x88000000  end of RAM
```
