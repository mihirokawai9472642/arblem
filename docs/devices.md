# Devices

Arblem drives four pieces of QEMU `virt` hardware directly (UART, PLIC, timer,
and a VirtIO block device) and asks OpenSBI to do three more things (print kernel
messages, start secondary harts, power off). Device addresses, IRQ numbers,
enabled CPU harts, the RAM range, and the timer frequency are read from the
device tree by `kernel/fdt.c` rather than hard-coded.

## Device tree (`kernel/fdt.c`)

`fdt_parse(dtb, &info)` walks the big-endian Flattened Device Tree (FDT) blob
passed by OpenSBI in `a1` before paging is turned on:

- Validates the FDT magic (`0xd00dfeed`) and header bounds (`totalsize`,
  structure block, and strings block).
- Scans `FDT_BEGIN_NODE`, `FDT_PROP`, and `FDT_END_NODE` tokens, committing a
  node's buffered properties both when a child node begins (`FDT_BEGIN_NODE`,
  for `/cpus/cpu@N` before its `interrupt-controller` subnode) and on
  `FDT_END_NODE`.
- Extracts up to `FDT_HART_MAX` (8) enabled CPU hart IDs (`/cpus/cpu@N` with
  `device_type = "cpu"`, `status = "okay"`, and 4- or 8-byte `reg`), RAM
  (`memory`), the timer frequency (`timebase-frequency`), the PLIC
  (`riscv,plic0` / `sifive,plic-1.0.0`), the UART (`ns16550a`), and up to
  `FDT_VIRTIO_MAX` (8) `virtio,mmio` slots into `struct fdt_info`.

## UART (`kernel/uart.c`)

The console is an NS16550-compatible UART. `uart_init(base, irq)` records its
MMIO base address and PLIC IRQ number from the device tree (`0x10000000`, IRQ
10 on QEMU `virt`) and enables the receive interrupt; `uart_irq()` exposes the
IRQ number to `trap_handler`. The driver uses three registers:

| Offset | Register      | Use                                                          |
|--------|---------------|--------------------------------------------------------------|
| 0      | `RBR` / `THR` | Read a received byte / write a byte to send                  |
| 1      | `IER`         | Bit 0 enables the "data received" interrupt                  |
| 5      | `LSR`         | Bit 0 (`DR`) = byte waiting in `RBR`; bit 5 (`THRE`) = `THR` ready |

`uart_lock` (`struct spinlock`) protects all UART MMIO and ring-buffer state
across harts:

- **Output**: `uart_putc` acquires `uart_lock`, polls `LSR.THRE` until the
  transmitter holding register is empty, and writes the byte to `THR`.
- **Input**: `uart_interrupt` drains every waiting byte from `RBR` into a
  512-byte ring buffer (dropping excess bytes if full) and wakes
  `uart_rx_wait`, which clears the level-triggered receive interrupt even when
  no thread is reading. `uart_getc` sleeps with
  `thread_sleep(&uart_rx_wait, &uart_lock)` while the buffer is empty and pops
  the next byte.

`kernel/console.c` turns the UART into a `struct file`, so user programs reach
it through file descriptors 0, 1 and 2. Its `write` copies user data into a
64-byte kernel buffer one chunk at a time, and its `read` returns one byte.

## PLIC (`kernel/plic.c`)

The Platform-Level Interrupt Controller collects device interrupts and passes
them to harts. Each hart and privilege level pair is a *context*; on QEMU
`virt`, hart *h* uses context `2 * h` for M-mode and context `2 * h + 1` for
S-mode.

| Register                    | Address                              | Use                         |
|-----------------------------|--------------------------------------|-----------------------------|
| Priority of IRQ *n*         | `PLIC + 4n`                          | Set to 1; 0 would disable it |
| Enable bits, context `ctx`  | `PLIC + 0x2000 + 0x80 * ctx`         | One bit per IRQ             |
| Threshold, context `ctx`    | `PLIC + 0x200000 + 0x1000 * ctx`     | Set to 0: pass any priority above 0 |
| Claim/complete, context `ctx` | `PLIC + 0x200004 + 0x1000 * ctx`   | Read to claim, write to complete |

Handling an external interrupt is always: read the claim register for the current
hart's S-mode context to get the IRQ number, ignore 0 (no pending IRQ, or
another hart claimed it first), service the matching device (`uart_irq()` or
`virtio_blk_irq()`), and write the same number back to complete it. The PLIC
does not send that IRQ again until it is completed.

`plic_init(base)` records the PLIC base address from the device tree and calls
`plic_init_hart()`, which sets the current hart's context threshold to 0 and
sets `sie.SEIE`. Each secondary hart runs `plic_init_hart()` and `plic_enable`
during `kernel_secondary`.

## Timer (`kernel/timer.c`)

The timer uses the `stimecmp` CSR from the Sstc extension, which QEMU's default
CPU supports and OpenSBI enables. A supervisor timer interrupt is pending
whenever `time >= stimecmp`, so each hart can program its own timer without an
SBI call.

- `timer_init(timebase_freq)` computes `timer_interval = timebase_freq / 100`
  from the device tree's `timebase-frequency` (10 MHz on `virt`), records
  `timer_boot_hart`, and calls `timer_init_hart()`. Secondary harts call
  `timer_init_hart()` to arm their own `stimecmp` and set `sie.STIE`.
- Each interrupt sets `stimecmp` to `time + timer_interval` (100,000 ticks at
  10 MHz), giving 100 interrupts per second per hart. Only `timer_boot_hart`
  atomically increments `timer_tick_count`, so `timer_ticks()` stays at 100 Hz
  regardless of the hart count.
- Writing `stimecmp` also clears the pending interrupt.

`timer_ticks()` returns the number of interrupts since boot, and the `ticks`
system call returns that number to user programs.

## VirtIO block device (`kernel/virtio_blk.c`)

Arblem uses a Modern VirtIO 1.0+ (MMIO v2) block device (`-global
virtio-mmio.force-legacy=false` in QEMU) backed by a raw disk image.

- **Probing and setup**: `virtio_blk_init(&info)` scans the `virtio,mmio` slots
  from the DTB for `MagicValue` `0x74726976` (`"virt"`), `Version` `2`, and
  `DeviceID` `2` (block device). Setup resets the device, sets `ACKNOWLEDGE` and
  `DRIVER`, negotiates `VIRTIO_F_VERSION_1` (feature word 1 bit 0, declining
  optional features) and checks `FEATURES_OK`, configures requestq (queue 0)
  with size `VIRTIO_QUEUE_SIZE` (8) and physical addresses for `blk_desc`,
  `blk_avail`, and `blk_used`, reads the 64-bit sector count from config space
  (`0x100`/`0x104`), and sets `DRIVER_OK`.
- **Virtqueue in `.bss`**: The split virtqueue tables (`blk_desc`,
  `blk_avail`, `blk_used`), request header (`blk_req`), and status byte
  (`blk_status`) sit in `.bss` with `_Alignas(16)`, `_Alignas(2)`, and
  `_Alignas(4)`. Because kernel memory is identity-mapped, their addresses can
  be given directly to DMA.
- **Sector I/O**: `virtio_blk_read(sector, buf)` and
  `virtio_blk_write(sector, buf)` transfer one 512-byte sector under `blk_lock`
  (`struct spinlock`). Calls serialize on `blk_busy` (sleeping with
  `thread_sleep(&blk_busy_wait, &blk_lock)`), build a 3-descriptor chain
  (16-byte `virtio_blk_req` header, 512-byte data buffer with
  `VIRTQ_DESC_F_WRITE` set for reads and clear for writes, and 1-byte status
  with `VIRTQ_DESC_F_WRITE`), publish it to `blk_avail` around `fence iorw,
  iorw` DMA barriers, ring `QueueNotify` (0), and sleep with
  `thread_sleep(&blk_done_wait, &blk_lock)` until `blk_used.idx` advances.
  Out-of-range sectors or device errors return `-ERR_IO`.
- **Interrupts**: `virtio_blk_interrupt()` acquires `blk_lock`, reads
  `InterruptStatus`, writes it back to `InterruptACK` to clear the
  level-triggered MMIO interrupt before `plic_complete`, and wakes
  `blk_done_wait`.

## SBI

The Supervisor Binary Interface is how S-mode software calls M-mode firmware.
An SBI call is an `ecall` from S-mode with:

| Register | Holds                                      |
|----------|--------------------------------------------|
| `a7`     | Extension ID (EID)                         |
| `a6`     | Function ID (FID)                          |
| `a0`-`a2`| Arguments                                  |
| `a0`, `a1` | Returned error code and value            |

`kernel/sbi.c` uses four extensions:

| Extension            | EID          | Function             | Used by                |
|----------------------|--------------|----------------------|------------------------|
| Base                 | `0x10`       | Get spec version     | Boot banner            |
| System Reset (SRST)  | `0x53525354` | System reset         | `poweroff`, `panic`    |
| Debug Console (DBCN) | `0x4442434e` | Console write        | `kprintf`              |
| Hart State Mgmt (HSM) | `0x48534d`  | Hart start           | `kernel_main` (`_start_secondary`) |

The EIDs of SRST, DBCN, and HSM are their names in ASCII. DBCN takes the
*physical* address of the buffer. The kernel can pass a pointer as is because
kernel memory is identity-mapped.

### `kprintf` and `panic`

`kprintf` (`kernel/kprintf.c`) understands only `%x` (a 64-bit value as
`0x` plus 16 hex digits) and `%s`. It acquires `kprintf_lock` so messages from
different harts do not interleave, collects output in a 64-byte buffer on the
stack, and sends it to the DBCN extension in chunks. Kernel messages therefore
go through OpenSBI, while user output goes through the UART driver; both end up
on the same serial port.

`panic` clears `sstatus.SIE`, disables `kprintf_lock` via
`kprintf_disable_lock()` (so it cannot deadlock if another hart died holding
`kprintf_lock`), and uses an atomic `panicking` flag so only the first hart to
panic prints `panic: <message>` and calls `sbi_shutdown`; any other hart halts
in `wfi`. The smoke test looks for the `panic: ` prefix.
