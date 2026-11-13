#include "error.h"
#include "str.h"
#include "sys.h"

#define SHELL_LINE_MAX 64

static void shell_puthex(unsigned long x) {
    char buf[19] = "0x";

    for (int i = 0; i < 16; i++) {
        buf[2 + i] = "0123456789abcdef"[(x >> (60 - 4 * i)) & 0xf];
    }

    buf[18] = '\n';

    sys_write(buf, sizeof(buf));
}

static void shell_readline(char *line) {
    int len = 0;

    for (;;) {
        char c = sys_getc();

        if (c == '\r' || c == '\n') {
            sys_putc('\n');
            line[len] = '\0';

            return;
        }

        if (c == 0x7f || c == '\b') {
            if (len > 0) {
                len--;
                sys_puts("\b \b");
            }

            continue;
        }

        if (len < SHELL_LINE_MAX - 1) {
            line[len++] = c;
            sys_putc(c);
        }
    }
}

static const char *shell_strerror(long err) {
    if (err == -ERR_NOENT) {
        return "Unknown command\n";
    }

    if (err == -ERR_NOMEM) {
        return "Out of memory\n";
    }

    if (err == -ERR_AGAIN) {
        return "Too many processes\n";
    }

    return "Error\n";
}

static void shell_exec(const char *line) {
    if (str_equal(line, "")) {
        return;
    }

    if (str_equal(line, "help")) {
        sys_puts("help: Show help message.\n");
        sys_puts("pages: Show free pages.\n");
        sys_puts("ticks: Show timer ticks.\n");
        sys_puts("hello, count: Run the program.\n");

        return;
    }

    if (str_equal(line, "pages")) {
        shell_puthex(sys_pages());

        return;
    }

    if (str_equal(line, "ticks")) {
        shell_puthex(sys_ticks());

        return;
    }

    long pid = sys_spawn(line, str_len(line));

    if (pid < 0) {
        sys_puts(shell_strerror(pid));

        return;
    }

    sys_wait(pid);
}

int main(void) {
    char line[SHELL_LINE_MAX];

    for (;;) {
        sys_puts("> ");
        shell_readline(line);
        shell_exec(line);
    }
}
