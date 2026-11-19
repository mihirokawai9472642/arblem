// Check close, dup, and pipe: basic I/O, ring wrap-around, reference counting,
// error rollback, EOF, broken pipe, and reading a child's stdout. Prints
// "pipe: ok" when every check passes. The smoke test runs this.

#include "error.h"
#include "str.h"
#include "sys.h"

// Check invalid arguments and broken-pipe / error rollback paths.
static int check_errors(void) {
    if (sys_close(-1) != -ERR_BADF || sys_close(99) != -ERR_BADF) {
        return -1;
    }

    if (sys_dup(-1) != -ERR_BADF || sys_dup(99) != -ERR_BADF) {
        return -1;
    }

    // Invalid user pointer must fail with -ERR_FAULT without leaking the pipe.
    if (sys_pipe(0) != -ERR_FAULT) {
        return -1;
    }

    int fds[2] = {-1, -1};

    if (sys_pipe(fds) != 0) {
        return -1;
    }

    char c = 'x';

    // Read end is not writable; write end is not readable.
    if (sys_write(fds[0], &c, 1) != -ERR_BADF ||
        sys_read(fds[1], &c, 1) != -ERR_BADF) {
        return -1;
    }

    // Closing the read end makes writes fail with -ERR_PIPE.
    if (sys_close(fds[0]) != 0 || sys_write(fds[1], &c, 1) != -ERR_PIPE) {
        return -1;
    }

    // Closing an already-closed fd returns -ERR_BADF.
    if (sys_close(fds[0]) != -ERR_BADF || sys_close(fds[1]) != 0) {
        return -1;
    }

    return 0;
}

// Check dup reference counting, EOF once the last writer closes, and ring
// buffer wrap-around past PIPE_BUF_SIZE (512 bytes).
static int check_io_and_dup(void) {
    int fds[2] = {-1, -1};

    if (sys_pipe(fds) != 0) {
        return -1;
    }

    // Duplicate the write end and close the original. The pipe must stay open
    // until the duplicate closes too.
    long wdup = sys_dup(fds[1]);

    if (wdup < 0 || sys_close(fds[1]) != 0) {
        return -1;
    }

    if (sys_write((int)wdup, "hi", 2) != 2) {
        return -1;
    }

    // Exercise ring-buffer wrap-around: 3 * 200 = 600 bytes (> 512).
    char out[200];
    char in[200];

    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 200; i++) {
            out[i] = (char)('a' + (round * 200 + i) % 26);
        }

        if (sys_write((int)wdup, out, sizeof(out)) != (long)sizeof(out)) {
            return -1;
        }

        // First round also has the 2 bytes "hi" queued ahead of out.
        if (round == 0) {
            char head[2] = {0, 0};

            if (sys_read(fds[0], head, 2) != 2 || head[0] != 'h' ||
                head[1] != 'i') {
                return -1;
            }
        }

        if (sys_read(fds[0], in, sizeof(in)) != (long)sizeof(in)) {
            return -1;
        }

        for (int i = 0; i < 200; i++) {
            if (in[i] != out[i]) {
                return -1;
            }
        }
    }

    // Closing the last write end produces EOF (0) on read.
    if (sys_close((int)wdup) != 0 || sys_read(fds[0], in, 1) != 0 ||
        sys_close(fds[0]) != 0) {
        return -1;
    }

    return 0;
}

// Redirect stdout to a pipe, spawn "hello", restore stdout, and read until EOF
// before waiting. This checks cross-process I/O and closing files on exit.
static int check_child(void) {
    int fds[2] = {-1, -1};

    if (sys_pipe(fds) != 0) {
        return -1;
    }

    long saved_out = sys_dup(FD_STDOUT);

    if (saved_out < 0) {
        return -1;
    }

    if (sys_close(FD_STDOUT) != 0 || sys_dup(fds[1]) != FD_STDOUT ||
        sys_close(fds[1]) != 0) {
        return -1;
    }

    long pid = sys_spawn("hello", str_len("hello"));

    // Restore stdout before reading, so only the child holds the write end.
    if (sys_close(FD_STDOUT) != 0 || sys_dup((int)saved_out) != FD_STDOUT ||
        sys_close((int)saved_out) != 0) {
        return -1;
    }

    if (pid < 0) {
        sys_close(fds[0]);

        return -1;
    }

    char buf[64];
    unsigned long total = 0;

    for (;;) {
        long n = sys_read(fds[0], buf + total, sizeof(buf) - 1 - total);

        if (n < 0) {
            return -1;
        }

        if (n == 0) {
            break;
        }

        total += (unsigned long)n;
    }

    buf[total] = '\0';

    int status = -1;

    if (sys_close(fds[0]) != 0 || sys_wait(pid, &status) != 0 || status != 0) {
        return -1;
    }

    if (!str_equal(buf, "Hello World from U-mode!\n")) {
        return -1;
    }

    return 0;
}

int main(void) {
    if (check_errors() != 0) {
        sys_puts("pipe: errors failed\n");

        return 1;
    }

    if (check_io_and_dup() != 0) {
        sys_puts("pipe: io failed\n");

        return 1;
    }

    if (check_child() != 0) {
        sys_puts("pipe: child failed\n");

        return 1;
    }

    sys_puts("pipe: ok\n");

    return 0;
}
