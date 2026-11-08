#include "shell.h"
#include "kprintf.h"
#include "str.h"
#include "timer.h"
#include "uart.h"

#define SHELL_LINE_MAX 64

static void shell_readline(char *line) {
    int len = 0;

    for (;;) {
        char c = uart_getc();

        if (c == '\r' || c == '\n') {
            kprintf("\n");
            line[len] = '\0';

            return;
        }

        if (c == 0x7f || c == '\b') {
            if (len > 0) {
                len--;
                kprintf("\b \b");
            }

            continue;
        }

        if (len < SHELL_LINE_MAX - 1) {
            line[len++] = c;
            uart_putc(c);
        }
    }
}

static void shell_exec(const char *line) {
    if (str_equal(line, "")) {
        return;
    }

    if (str_equal(line, "help")) {
        kprintf("help: Show help message.\n");
        kprintf("ticks: Show timer ticks.\n");

        return;
    }

    if (str_equal(line, "ticks")) {
        kprintf("%x\n", timer_ticks());

        return;
    }

    kprintf("Unknown command\n");
}

void shell_main(void) {
    char line[SHELL_LINE_MAX];

    for (;;) {
        kprintf("> ");
        shell_readline(line);
        shell_exec(line);
    }
}
