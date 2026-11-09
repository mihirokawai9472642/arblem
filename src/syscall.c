#include "syscall.h"
#include "thread.h"
#include "uart.h"

unsigned long syscall_handle(unsigned long num, unsigned long arg) {
    if (num == SYSCALL_PUTC) {
        uart_putc(arg);
        return 0;
    }

    if (num == SYSCALL_EXIT) {
        thread_exit();
    }

    return -1;
}
