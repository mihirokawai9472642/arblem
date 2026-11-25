// SMP stress test: run parallel worker processes across harts exercising
// demand-paged memory, shared pipe I/O, filesystem create/read/write/unlink,
// per-process tp preservation, and timer ticks. Prints "smp: ok" when every
// check passes. The smoke test runs this.

#include "sys.h"

#define SMP_WORKERS     4
#define SMP_PIPE_ROUNDS 16
#define SMP_PIPE_CHUNK  16
#define SMP_PIPE_TOTAL  (SMP_PIPE_ROUNDS * SMP_PIPE_CHUNK)
#define SMP_MEM_PAGES   16
#define SMP_PAGE_SIZE   4096UL
#define SMP_FILE_BYTES  512

static int worker_mem(int id) {
    unsigned long bytes = SMP_MEM_PAGES * SMP_PAGE_SIZE;
    long base = sys_sbrk((long)bytes);

    if (base < 0) {
        return -1;
    }

    unsigned char *p = (unsigned char *)base;

    // Touch every demand-paged heap page with a worker-specific pattern.
    for (unsigned long i = 0; i < SMP_MEM_PAGES; i++) {
        p[i * SMP_PAGE_SIZE] = (unsigned char)(id * 37 + (int)i + 1);
        p[i * SMP_PAGE_SIZE + SMP_PAGE_SIZE - 1] =
            (unsigned char)(id * 53 + (int)i + 7);
    }

    for (unsigned long i = 0; i < SMP_MEM_PAGES; i++) {
        if (p[i * SMP_PAGE_SIZE] != (unsigned char)(id * 37 + (int)i + 1) ||
            p[i * SMP_PAGE_SIZE + SMP_PAGE_SIZE - 1] !=
                (unsigned char)(id * 53 + (int)i + 7)) {
            return -1;
        }
    }

    if (sys_sbrk(-(long)bytes) < 0) {
        return -1;
    }

    return 0;
}

static int worker_fs(int id) {
    char path[6];

    path[0] = '/';
    path[1] = 's';
    path[2] = 'm';
    path[3] = 'p';
    path[4] = (char)('0' + id);
    path[5] = '\0';

    char out[SMP_FILE_BYTES];
    char in[SMP_FILE_BYTES];

    for (int i = 0; i < SMP_FILE_BYTES; i++) {
        out[i] = (char)('a' + (id * 7 + i) % 26);
    }

    long fd = sys_open(path, O_RDWR | O_CREAT | O_TRUNC);

    if (fd < 0) {
        return -1;
    }

    if (sys_write((int)fd, out, SMP_FILE_BYTES) != SMP_FILE_BYTES) {
        sys_close((int)fd);
        sys_unlink(path);

        return -1;
    }

    if (sys_close((int)fd) != 0) {
        sys_unlink(path);

        return -1;
    }

    fd = sys_open(path, O_RDONLY);

    if (fd < 0) {
        sys_unlink(path);

        return -1;
    }

    long n = sys_read((int)fd, in, SMP_FILE_BYTES);

    sys_close((int)fd);

    if (sys_unlink(path) != 0 || n != SMP_FILE_BYTES) {
        return -1;
    }

    for (int i = 0; i < SMP_FILE_BYTES; i++) {
        if (in[i] != out[i]) {
            return -1;
        }
    }

    return 0;
}

static int worker_pipe(int id) {
    char chunk[SMP_PIPE_CHUNK];

    for (int i = 0; i < SMP_PIPE_CHUNK; i++) {
        chunk[i] = (char)('A' + id);
    }

    for (int r = 0; r < SMP_PIPE_ROUNDS; r++) {
        if (sys_write(FD_STDOUT, chunk, SMP_PIPE_CHUNK) != SMP_PIPE_CHUNK) {
            return -1;
        }
    }

    return 0;
}

static int run_worker(int id) {
    unsigned long want_tp = 0x3000UL + (unsigned long)id;
    unsigned long got_tp = 0;

    __asm__ volatile("mv tp, %0" : : "r"(want_tp));

    if (worker_mem(id) != 0 || worker_fs(id) != 0 || worker_pipe(id) != 0) {
        return 1;
    }

    __asm__ volatile("mv %0, tp" : "=r"(got_tp));

    return got_tp == want_tp ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc >= 2) {
        int id = argv[1][0] - '0';

        if (id < 0 || id >= SMP_WORKERS) {
            return 1;
        }

        return run_worker(id);
    }

    // Verify the boot hart advances timer ticks.
    unsigned long t0 = sys_ticks();

    while (sys_ticks() == t0) {
    }

    int fds[2] = {-1, -1};

    if (sys_pipe(fds) != 0) {
        sys_puts("smp: pipe failed\n");

        return 1;
    }

    long saved_out = sys_dup(FD_STDOUT);

    if (saved_out < 0 || sys_close(FD_STDOUT) != 0 ||
        sys_dup(fds[1]) != FD_STDOUT || sys_close(fds[1]) != 0) {
        sys_puts("smp: redirect failed\n");

        return 1;
    }

    long pids[SMP_WORKERS];

    for (int i = 0; i < SMP_WORKERS; i++) {
        char id_str[2];

        id_str[0] = (char)('0' + i);
        id_str[1] = '\0';

        const char *wargv[3] = {"smpcheck", id_str, 0};

        pids[i] = sys_spawnv("smpcheck", str_len("smpcheck"), wargv);
    }

    // Restore stdout so only the workers hold the pipe's write end.
    if (sys_close(FD_STDOUT) != 0 || sys_dup((int)saved_out) != FD_STDOUT ||
        sys_close((int)saved_out) != 0) {
        return 1;
    }

    for (int i = 0; i < SMP_WORKERS; i++) {
        if (pids[i] < 0) {
            sys_puts("smp: spawn failed\n");

            return 1;
        }
    }

    int counts[SMP_WORKERS] = {0, 0, 0, 0};
    char buf[64];

    for (;;) {
        long n = sys_read(fds[0], buf, sizeof(buf));

        if (n < 0) {
            sys_puts("smp: read failed\n");

            return 1;
        }

        if (n == 0) {
            break;
        }

        for (long i = 0; i < n; i++) {
            int idx = buf[i] - 'A';

            if (idx < 0 || idx >= SMP_WORKERS) {
                sys_puts("smp: bad byte\n");

                return 1;
            }

            counts[idx]++;
        }
    }

    sys_close(fds[0]);

    int ok = 1;

    for (int i = 0; i < SMP_WORKERS; i++) {
        int status = -1;

        if (sys_wait(pids[i], &status) != 0 || status != 0) {
            ok = 0;
        }

        if (counts[i] != SMP_PIPE_TOTAL) {
            ok = 0;
        }
    }

    if (!ok) {
        sys_puts("smp: worker failed\n");

        return 1;
    }

    sys_puts("smp: ok\n");

    return 0;
}
