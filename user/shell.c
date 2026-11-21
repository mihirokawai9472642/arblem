// The shell: read a line, run a built-in command or spawn the program with
// that name, and wait for it. The kernel starts it at boot with the console
// as stdin, stdout and stderr. See docs/user.md.

#include "error.h"
#include "str.h"
#include "sys.h"

// Longest line, including the NUL.
#define SHELL_LINE_MAX 64

// Maximum arguments per command, matching PROC_ARG_MAX in the kernel.
#define SHELL_ARG_MAX 16

// Print x as 0x plus 16 hex digits and a newline.
static void shell_puthex(unsigned long x) {
    char buf[19] = "0x";

    for (int i = 0; i < 16; i++) {
        buf[2 + i] = "0123456789abcdef"[(x >> (60 - 4 * i)) & 0xf];
    }

    buf[18] = '\n';

    sys_write(FD_STDOUT, buf, sizeof(buf));
}

// Print x in decimal and a newline. Digits are filled in from the end.
static void shell_putdec(int x) {
    char buf[12];
    int i = sizeof(buf);
    unsigned int u = x < 0 ? -(unsigned int)x : (unsigned int)x;

    buf[--i] = '\n';

    do {
        buf[--i] = '0' + u % 10;
        u /= 10;
    } while (u != 0);

    if (x < 0) {
        buf[--i] = '-';
    }

    sys_write(FD_STDOUT, buf + i, sizeof(buf) - i);
}

// Read one line into line, echoing as the user types. The kernel has no line
// discipline, so the shell handles echo and Backspace itself. Enter arrives
// as \r from a terminal.
static void shell_readline(char *line) {
    int len = 0;

    for (;;) {
        char c = sys_getc();

        if (c == '\r' || c == '\n') {
            sys_putc('\n');
            line[len] = '\0';

            return;
        }

        // Backspace (DEL or BS): move back, blank the character, move back.
        if (c == 0x7f || c == '\b') {
            if (len > 0) {
                len--;
                sys_puts("\b \b");
            }

            continue;
        }

        // Characters past the limit are dropped.
        if (len < SHELL_LINE_MAX - 1) {
            line[len++] = c;
            sys_putc(c);
        }
    }
}

// Message for a negative error code from sys_spawn or sys_poweroff.
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

    if (err == -ERR_NOEXEC) {
        return "Bad executable\n";
    }

    if (err == -ERR_MFILE) {
        return "Too many open files\n";
    }

    if (err == -ERR_2BIG) {
        return "Too many arguments\n";
    }

    return "Error\n";
}

// Split a command string in place on spaces into argv[0..argc-1], ending with
// a NULL entry. Returns argc, or -ERR_2BIG if there are more than max words.
static int shell_split(char *cmd, const char **argv, int max) {
    int argc = 0;
    char *p = cmd;

    for (;;) {
        while (*p == ' ') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        if (argc >= max) {
            return -ERR_2BIG;
        }

        argv[argc++] = p;

        while (*p != '\0' && *p != ' ') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        *p++ = '\0';
    }

    argv[argc] = 0;

    return argc;
}

// Replace target with newfd (closing newfd) and return a dup of the old target.
static long shell_swap_fd(int target, int newfd) {
    long saved = sys_dup(target);

    if (saved < 0) {
        return saved;
    }

    sys_close(target);
    sys_dup(newfd);
    sys_close(newfd);

    return saved;
}

// Restore target from saved (closing saved).
static void shell_restore_fd(int target, int saved) {
    sys_close(target);
    sys_dup(saved);
    sys_close(saved);
}

// Run "left | right": connect left's stdout to right's stdin with a pipe.
static void shell_exec_pipe(char *left, char *right) {
    const char *largv[SHELL_ARG_MAX + 1];
    const char *rargv[SHELL_ARG_MAX + 1];
    int largc = shell_split(left, largv, SHELL_ARG_MAX);

    if (largc < 0) {
        sys_puts(shell_strerror(largc));

        return;
    }

    int rargc = shell_split(right, rargv, SHELL_ARG_MAX);

    if (rargc < 0) {
        sys_puts(shell_strerror(rargc));

        return;
    }

    if (largc == 0 || rargc == 0) {
        sys_puts("Bad pipeline\n");

        return;
    }

    int fds[2] = {-1, -1};
    long err = sys_pipe(fds);

    if (err < 0) {
        sys_puts(shell_strerror(err));

        return;
    }

    long saved_out = shell_swap_fd(FD_STDOUT, fds[1]);

    if (saved_out < 0) {
        sys_close(fds[0]);
        sys_close(fds[1]);
        sys_puts(shell_strerror(saved_out));

        return;
    }

    long pid1 = sys_spawnv(largv[0], str_len(largv[0]), largv);

    // Restore stdout right away so only left holds the write end of the pipe.
    shell_restore_fd(FD_STDOUT, (int)saved_out);

    if (pid1 < 0) {
        sys_close(fds[0]);
        sys_puts(shell_strerror(pid1));

        return;
    }

    long saved_in = shell_swap_fd(FD_STDIN, fds[0]);

    if (saved_in < 0) {
        sys_close(fds[0]);
        sys_puts(shell_strerror(saved_in));
        sys_wait(pid1, 0);

        return;
    }

    long pid2 = sys_spawnv(rargv[0], str_len(rargv[0]), rargv);

    // Restore stdin so the shell holds neither end of the pipe.
    shell_restore_fd(FD_STDIN, (int)saved_in);

    if (pid2 < 0) {
        sys_puts(shell_strerror(pid2));
        sys_wait(pid1, 0);

        return;
    }

    int status1 = 0;
    int status2 = 0;

    sys_wait(pid1, &status1);
    sys_wait(pid2, &status2);

    if (status2 != 0) {
        sys_puts("exit: ");
        shell_putdec(status2);
    } else if (status1 != 0) {
        sys_puts("exit: ");
        shell_putdec(status1);
    }
}

static void shell_exec(char *line) {
    // Look for a single pipe: "left | right".
    for (unsigned long i = 0; line[i] != '\0'; i++) {
        if (line[i] == '|') {
            line[i] = '\0';
            shell_exec_pipe(line, line + i + 1);

            return;
        }
    }

    const char *argv[SHELL_ARG_MAX + 1];
    int argc = shell_split(line, argv, SHELL_ARG_MAX);

    if (argc < 0) {
        sys_puts(shell_strerror(argc));

        return;
    }

    if (argc == 0) {
        return;
    }

    if (str_equal(argv[0], "help")) {
        sys_puts("help: Show help message.\n");
        sys_puts("pages: Show free pages.\n");
        sys_puts("ticks: Show timer ticks.\n");
        sys_puts("poweroff: Power off the machine.\n");
        sys_puts("hello, count, fault, orphan, regcheck, pipecheck, upper, "
                 "echo, memcheck, blkcheck: Run the program.\n");
        sys_puts("cmd1 | cmd2: Connect cmd1's stdout to cmd2's stdin.\n");

        return;
    }

    if (str_equal(argv[0], "pages")) {
        shell_puthex(sys_pages());

        return;
    }

    if (str_equal(argv[0], "ticks")) {
        shell_puthex(sys_ticks());

        return;
    }

    // Returns only if the shutdown fails.
    if (str_equal(argv[0], "poweroff")) {
        sys_puts(shell_strerror(sys_poweroff()));

        return;
    }

    // Not a built-in: run it as a program and wait for it to finish.
    long pid = sys_spawnv(argv[0], str_len(argv[0]), argv);

    if (pid < 0) {
        sys_puts(shell_strerror(pid));

        return;
    }

    int status = 0;

    if (sys_wait(pid, &status) == 0 && status != 0) {
        sys_puts("exit: ");
        shell_putdec(status);
    }
}

int main(void) {
    char line[SHELL_LINE_MAX];

    for (;;) {
        sys_puts("> ");
        shell_readline(line);
        shell_exec(line);
    }
}
