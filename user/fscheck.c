// Check file creation, read/write, truncate, multi-block I/O, directories,
// unlink, and error codes, and leave /persist on disk for the smoke test's
// persistence check. Prints "fs: ok" when every check passes.

#include "error.h"
#include "str.h"
#include "sys.h"

static int check_file_rw(void) {
    const char msg[] = "hello filesystem\n";
    unsigned long msg_len = sizeof(msg) - 1;
    long fd = sys_open("/fstest", O_WRONLY | O_CREAT | O_TRUNC);

    if (fd < 0) {
        return -1;
    }

    char tmp[8] = {0};

    // Reading a write-only descriptor must fail with -ERR_BADF.
    if (sys_read((int)fd, tmp, sizeof(tmp)) != -ERR_BADF) {
        sys_close((int)fd);

        return -1;
    }

    if (sys_write((int)fd, msg, msg_len) != (long)msg_len) {
        sys_close((int)fd);

        return -1;
    }

    sys_close((int)fd);

    fd = sys_open("/fstest", O_RDONLY);

    if (fd < 0) {
        return -1;
    }

    // Writing a read-only descriptor must fail with -ERR_BADF.
    if (sys_write((int)fd, "x", 1) != -ERR_BADF) {
        sys_close((int)fd);

        return -1;
    }

    char rbuf[32] = {0};
    long n = sys_read((int)fd, rbuf, sizeof(rbuf) - 1);

    if (n != (long)msg_len || !str_equal(rbuf, msg) ||
        sys_read((int)fd, tmp, 1) != 0) {
        sys_close((int)fd);

        return -1;
    }

    sys_close((int)fd);

    // O_TRUNC must shrink the file back to 0 before writing "hi".
    fd = sys_open("/fstest", O_RDWR | O_TRUNC);

    if (fd < 0 || sys_write((int)fd, "hi", 2) != 2) {
        if (fd >= 0) {
            sys_close((int)fd);
        }

        return -1;
    }

    sys_close((int)fd);

    fd = sys_open("/fstest", O_RDONLY);

    if (fd < 0) {
        return -1;
    }

    char short_buf[8] = {0};

    n = sys_read((int)fd, short_buf, sizeof(short_buf) - 1);
    sys_close((int)fd);

    if (n != 2 || !str_equal(short_buf, "hi")) {
        return -1;
    }

    if (sys_unlink("/fstest") != 0 ||
        sys_open("/fstest", O_RDONLY) != -ERR_NOENT) {
        return -1;
    }

    return 0;
}

// Write and verify 5000 bytes (crossing a 4 KiB block boundary) to test
// multi-block fs_write, fs_read, and block freeing on unlink.
static int check_multiblock(void) {
    long fd = sys_open("/fsbig", O_WRONLY | O_CREAT | O_TRUNC);

    if (fd < 0) {
        return -1;
    }

    char chunk[64] = {0};
    unsigned long total = 5000;

    for (unsigned long off = 0; off < total; off += sizeof(chunk)) {
        unsigned long n = total - off;

        if (n > sizeof(chunk)) {
            n = sizeof(chunk);
        }

        for (unsigned long i = 0; i < n; i++) {
            chunk[i] = (char)(((off + i) * 31 + 7) & 0x7f);
        }

        if (sys_write((int)fd, chunk, n) != (long)n) {
            sys_close((int)fd);

            return -1;
        }
    }

    sys_close((int)fd);

    fd = sys_open("/fsbig", O_RDONLY);

    if (fd < 0) {
        return -1;
    }

    for (unsigned long off = 0; off < total; off += sizeof(chunk)) {
        unsigned long n = total - off;

        if (n > sizeof(chunk)) {
            n = sizeof(chunk);
        }

        long r = sys_read((int)fd, chunk, n);

        if (r != (long)n) {
            sys_close((int)fd);

            return -1;
        }

        for (unsigned long i = 0; i < n; i++) {
            if (chunk[i] != (char)(((off + i) * 31 + 7) & 0x7f)) {
                sys_close((int)fd);

                return -1;
            }
        }
    }

    sys_close((int)fd);

    return sys_unlink("/fsbig") == 0 ? 0 : -1;
}

static int check_dirs_and_errors(void) {
    if (sys_mkdir("/fsdir") != 0 || sys_mkdir("/fsdir") != -ERR_EXIST) {
        return -1;
    }

    if (sys_open("/fsdir", O_WRONLY) != -ERR_ISDIR ||
        sys_open(0, O_RDONLY) != -ERR_FAULT || sys_unlink("/") != -ERR_INVAL) {
        return -1;
    }

    long fd = sys_open("/fsdir/note", O_WRONLY | O_CREAT | O_TRUNC);

    if (fd < 0 || sys_write((int)fd, "nested", 6) != 6) {
        if (fd >= 0) {
            sys_close((int)fd);
        }

        return -1;
    }

    sys_close((int)fd);

    // Non-empty directory must refuse unlink.
    if (sys_unlink("/fsdir") != -ERR_NOTEMPTY) {
        return -1;
    }

    fd = sys_open("/fsdir/note", O_RDONLY);

    if (fd < 0) {
        return -1;
    }

    char rbuf[16] = {0};
    long n = sys_read((int)fd, rbuf, sizeof(rbuf) - 1);

    sys_close((int)fd);

    if (n != 6 || !str_equal(rbuf, "nested")) {
        return -1;
    }

    if (sys_unlink("/fsdir/note") != 0 || sys_unlink("/fsdir") != 0) {
        return -1;
    }

    return 0;
}

static int check_persist(void) {
    const char sig[] = "ARBLEM-FS-OK\n";
    unsigned long len = sizeof(sig) - 1;
    long fd = sys_open("/persist", O_WRONLY | O_CREAT | O_TRUNC);

    if (fd < 0) {
        return -1;
    }

    long w = sys_write((int)fd, sig, len);

    sys_close((int)fd);

    return w == (long)len ? 0 : -1;
}

int main(void) {
    if (check_file_rw() != 0) {
        sys_puts("fs: rw failed\n");

        return 1;
    }

    if (check_multiblock() != 0) {
        sys_puts("fs: multiblock failed\n");

        return 1;
    }

    if (check_dirs_and_errors() != 0) {
        sys_puts("fs: dirs failed\n");

        return 1;
    }

    if (check_persist() != 0) {
        sys_puts("fs: persist failed\n");

        return 1;
    }

    sys_puts("fs: ok\n");

    return 0;
}
