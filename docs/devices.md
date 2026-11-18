# Devices

Arblem drives three pieces of QEMU `virt` hardware directly (UART, PLIC, timer)
and asks OpenSBI to do two more things (print kernel messages, power off).
Device addresses and the IRQ number are in `kernel/virt.h`. They are fixed
for the `virt` machine; the kernel does not read them from the device tree.

## UART (`kernel/uart.c`)

The console is an NS16550-compatible UART at `0x10000000`, on PLIC IRQ 10. The
driver uses three registers:

| Offset | Register | Use                                          |
|--------|----------|----------------------------------------------|
| 0      | `RBR` / `THR` | Read a received byte / write a byte to send |
| 1      | `IER`    | Bit 0 enables the "data received" interrupt  |
| 5      | `LSR`    | Bit 0 (`DR`) is set when a byte is waiting   |

- **Output**: `uart_putc` writes straight to `THR`. It does not wait for the
  transmitter to be ready, which works on QEMU but would drop characters on
  real hardware.
- **Input**: `uart_getc` checks `LSR.DR`. If no byte is waiting, it sleeps on
  `uart_rx_wait`. The receive interrupt calls `uart_interrupt`, which wakes the
  sleeping thread, and the loop checks again.

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
IRQ number, service the device, and write the same number back to complete it.
The PLIC does not send that IRQ again until it is completed.

`plic_init` also sets `sie.SEIE`, so the hart accepts external interrupts in
S-mode.

## Timer (`kernel/timer.c`)

The timer uses the `stimecmp` CSR from the Sstc extension, which QEMU's default
CPU supports and OpenSBI enables. A supervisor timer interrupt is pending
whenever `time >= stimecmp`, so the kernel can program the timer without an SBI
call.

- `time` counts at 10 MHz on `virt` (`VIRT_TIMEBASE_FREQ`).
- Each interrupt sets `stimecmp` to `time + 100000`, which gives 100 ticks per
  second.
- Writing `stimecmp` also clears the pending interrupt.

`timer_ticks()` returns the number of interrupts since boot, and the `ticks`
system call returns that number to user programs.

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
