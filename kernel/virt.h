#pragma once

// Fixed hardware facts of the QEMU virt machine. These would normally come
// from the device tree, which the kernel does not parse yet.

#define VIRT_PLIC          0x0c000000UL
#define VIRT_PLIC_SIZE     0x600000UL
#define VIRT_UART0         0x10000000UL
#define VIRT_UART0_SIZE    0x1000UL
#define VIRT_UART0_IRQ     10
#define VIRT_TIMEBASE_FREQ 10000000UL  // Frequency of the time CSR, in Hz
