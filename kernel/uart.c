// Driver for the NS16550-compatible UART of the QEMU virt machine.
// Output polls LSR.THRE; input is buffered in a ring buffer by the receive
// interrupt. See docs/devices.md.

#include "uart.h"
#include "spinlock.h"
#include "thread.h"

static unsigned long uart_base;
static unsigned int uart_irq_num;

// Registers, at byte offsets from the base address. RBR and THR share an
// offset: reads come from RBR, writes go to THR.
#define UART_RBR      ((volatile unsigned char *)(uart_base + 0)) // Receive buffer
#define UART_THR      ((volatile unsigned char *)(uart_base + 0)) // Transmit holding
#define UART_IER      ((volatile unsigned char *)(uart_base + 1)) // Interrupt enable
#define UART_LSR      ((volatile unsigned char *)(uart_base + 5)) // Line status
#define UART_IER_RDI  (1 << 0) // Interrupt when received data is available
#define UART_LSR_DR   (1 << 0) // A received byte is waiting in RBR
#define UART_LSR_THRE (1 << 5) // THR is empty and ready for the next byte

// Receive ring buffer capacity (power of two so wrap-around stays aligned).
#define UART_RX_BUF_SIZE 512

static struct spinlock uart_lock = SPINLOCK_INIT("uart");
static char uart_rx_buf[UART_RX_BUF_SIZE];
static unsigned long uart_rx_rpos;
static unsigned long uart_rx_wpos;

// Wait channel for threads in uart_getc.
static char uart_rx_wait;

void uart_init(unsigned long base, unsigned int irq) {
    uart_base = base;
    uart_irq_num = irq;
    *UART_IER = UART_IER_RDI;
}

unsigned int uart_irq(void) {
    return uart_irq_num;
}

void uart_putc(char c) {
    spinlock_acquire(&uart_lock);

    while ((*UART_LSR & UART_LSR_THRE) == 0) {
    }

    *UART_THR = (unsigned char)c;
    spinlock_release(&uart_lock);
}

char uart_getc(void) {
    spinlock_acquire(&uart_lock);

    while (uart_rx_rpos == uart_rx_wpos) {
        thread_sleep(&uart_rx_wait, &uart_lock);
    }

    char c = uart_rx_buf[uart_rx_rpos % UART_RX_BUF_SIZE];

    uart_rx_rpos++;
    spinlock_release(&uart_lock);

    return c;
}

// Drain every waiting byte from RBR so the level-triggered receive interrupt
// clears even when no thread is reading. Excess bytes are dropped if full.
void uart_interrupt(void) {
    spinlock_acquire(&uart_lock);

    unsigned long before = uart_rx_wpos;

    while ((*UART_LSR & UART_LSR_DR) != 0) {
        unsigned char c = *UART_RBR;

        if (uart_rx_wpos - uart_rx_rpos < UART_RX_BUF_SIZE) {
            uart_rx_buf[uart_rx_wpos % UART_RX_BUF_SIZE] = (char)c;
            uart_rx_wpos++;
        }
    }

    if (uart_rx_wpos != before) {
        thread_wakeup(&uart_rx_wait);
    }

    spinlock_release(&uart_lock);
}
