# Devices

Arblem drives four pieces of QEMU `virt` hardware directly (UART, PLIC, timer,
and a VirtIO block device) and asks OpenSBI to do two more things (print kernel
messages, power off). Device addresses, IRQ numbers, the RAM range, and the
timer frequency are read from the device tree by `kernel/fdt.c` rather than
hard-coded.

## Device tree (`kernel/fdt.c`)

`fdt_parse(dtb, &info)` walks the big-endian Flattened Device Tree (FDT) blob
passed by OpenSBI in `a1` before paging is turned on:

- Validates the FDT magic (`0xd00dfeed`) and header bounds (`totalsize`,
  structure block, and strings block).
- Scans `FDT_BEGIN_NODE`, `FDT_PROP`, and `FDT_END_NODE` tokens, buffering each
  node's `reg`, `interrupts`, `device_type`, and `compatible` properties until
  `FDT_END_NODE` (in QEMU's DTB, `reg` and `interrupts` appear before
  `compatible`).
- Extracts RAM (`memory`), the timer frequency (`timebase-frequency`), the
  PLIC (`riscv,plic0` / `sifive,plic-1.0.0`), the UART (`ns16550a`), and up to
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

- **Output**: `uart_putc` polls `LSR.THRE` until the transmitter holding
  register is empty, then writes the byte to `THR`.
- **Input**: `uart_interrupt` drains every waiting byte from `RBR` into a
  512-byte ring buffer (dropping excess bytes if full) and wakes
  `uart_rx_wait`, which clears the level-triggered receive interrupt even when
  no thread is reading. `uart_getc` sleeps on `uart_rx_wait` while the buffer
  is empty and pops the next byte.

`kernel/console.c` turns the UART into a `struct file`, so user programs reach
it through file descriptors 0, 1 and 2. Its `write` copies user data into a
64-byte kernel buffer one chunk at a time, and its `read` returns one byte.

## PLIC (`kernel/plic.c`)

The Platform-Level Interrupt Controller collects device interrupts and passes
them to harts. Each hart and privilege level pair is a *context*; on `virt`,
context 1 is hart 0 in S-mode, and that is the only context Arblem uses.

| Register                 | Address                  | Use                         |
|--------------------------|--------------------------|-----------------------------|
| Priority of IRQ *n*      | `PLIC + 4n`              | Set to 1; 0 would disable it |
| Enable bits, context 1   | `PLIC + 0x2080`          | One bit per IRQ             |
| Threshold, context 1     | `PLIC + 0x201000`        | Set to 0: pass any priority above 0 |
| Claim/complete, context 1 | `PLIC + 0x201004`       | Read to claim, write to complete |

Handling an external interrupt is always: read the claim register to get the
IRQ number, ignore 0 (no pending IRQ), service the matching device (`uart_irq()`
or `virtio_blk_irq()`), and write the same number back to complete it. The PLIC
does not send that IRQ again until it is completed.

`plic_init(base)` records the PLIC base address from the device tree, sets the
context 1 threshold to 0, and sets `sie.SEIE` so the hart accepts external
interrupts in S-mode.

## Timer (`kernel/timer.c`)

The timer uses the `stimecmp` CSR from the Sstc extension, which QEMU's default
CPU supports and OpenSBI enables. A supervisor timer interrupt is pending
whenever `time >= stimecmp`, so the kernel can program the timer without an SBI
call.

- `timer_init(timebase_freq)` computes `timer_interval = timebase_freq / 100`
  from the device tree's `timebase-frequency` (10 MHz on `virt`).
- Each interrupt sets `stimecmp` to `time + timer_interval` (100,000 ticks at
  10 MHz), giving 100 ticks per second.
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
  `virtio_blk_write(sector, buf)` transfer one 512-byte sector. Calls are
  serialized with `blk_busy` (sleeping on `blk_busy_wait`), build a
  3-descriptor chain (16-byte `virtio_blk_req` header, 512-byte data buffer with
  `VIRTQ_DESC_F_WRITE` set for reads and clear for writes, and 1-byte status
  with `VIRTQ_DESC_F_WRITE`), publish it to `blk_avail` around `fence iorw,
  iorw` DMA barriers, ring `QueueNotify` (0), and sleep on `blk_done_wait`
  until `blk_used.idx` advances. Out-of-range sectors or device errors return
  `-ERR_IO`.
- **Interrupts**: `virtio_blk_interrupt()` reads `InterruptStatus`, writes it
  back to `InterruptACK` to clear the level-triggered MMIO interrupt before
  `plic_complete`, and wakes `blk_done_wait`.

## SBI

The Supervisor Binary Interface is how S-mode software calls M-mode firmware.
An SBI call is an `ecall` from S-mode with:

| Register | Holds                                      |
|----------|--------------------------------------------|
| `a7`     | Extension ID (EID)                         |
| `a6`     | Function ID (FID)                          |
| `a0`-`a2`| Arguments                                  |
| `a0`, `a1` | Returned error code and value            |

`kernel/sbi.c` uses three extensions:

| Extension            | EID          | Function             | Used by                |
|----------------------|--------------|----------------------|------------------------|
| Base                 | `0x10`       | Get spec version     | Boot banner            |
| System Reset (SRST)  | `0x53525354` | System reset         | `poweroff`, `panic`    |
| Debug Console (DBCN) | `0x4442434e` | Console write        | `kprintf`              |

The EIDs of SRST and DBCN are their names in ASCII. DBCN takes the *physical*
address of the buffer. The kernel can pass a pointer as is because kernel memory
is identity-mapped.

### `kprintf` and `panic`

`kprintf` (`kernel/kprintf.c`) understands only `%x` (a 64-bit value as
`0x` plus 16 hex digits) and `%s`. It collects output in a 64-byte buffer on the
stack and sends it to the DBCN extension in chunks. Kernel messages therefore go
through OpenSBI, while user output goes through the UART driver; both end up on
the same serial port.

`panic` turns off interrupts, prints `panic: <message>`, and asks SBI to shut
down with the "system failure" reason. The smoke test looks for the
`panic: ` prefix.
