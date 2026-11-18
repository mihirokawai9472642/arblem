# Boot

This page follows the machine from reset to the shell's first prompt.

## 1. QEMU and OpenSBI

`make run` starts QEMU with:

```
qemu-system-riscv64 -machine virt -nographic -bios default -kernel build/kernel.elf
```

- `-bios default` loads QEMU's bundled OpenSBI at `0x80000000` and runs it in
  M-mode.
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

| Step                              | What it does                                                         |
|-----------------------------------|----------------------------------------------------------------------|
| `kprintf(...)`                    | Print the banner, hart ID, DTB address and SBI version through SBI   |
| `page_init(__kernel_end, dtb)`    | Put every page from the end of the kernel up to the DTB on the free list |
| `vm_init(dtb)`                    | Build the kernel page table and turn on Sv39 paging                  |
| `uart_init()`                     | Enable the UART's receive interrupt                                  |
| `plic_init()`, `plic_enable(10)`  | Route UART interrupts to this hart's S-mode and enable `sie.SEIE`    |
| `timer_init()`                    | Arm the first timer interrupt and enable `sie.STIE`                  |
| `proc_spawn("shell")`             | Load the shell's ELF into a new address space, in a new thread       |
| `thread_set_file(...)` x3         | Give the shell the console as stdin, stdout and stderr               |
| `thread_start()`                  | Create the idle thread and switch to thread 0                        |

`sie` bits only enable interrupt *sources*. The global switch `sstatus.SIE`
stays off during `kernel_main`, so no interrupt arrives until the first thread
runs.

The kernel does not parse the device tree. It assumes that OpenSBI put the DTB
near the top of RAM, and treats the DTB's address as the end of usable memory.
See [Known issues](known-issues.md).

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
...
0x80000000  OpenSBI
0x80200000  kernel .text        (__kernel_start)
            kernel .rodata      (__text_end)
            kernel .data, .bss  (__rodata_end), boot stack in .bss
            free pages          (__kernel_end, page aligned)
            ...
dtb         device tree, placed by OpenSBI near the top of RAM
0x88000000  end of RAM
```
