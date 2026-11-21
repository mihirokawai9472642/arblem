// Check VirtIO-blk sector read/write, sector isolation, bounds and fault
// errors, and leave a signature in sector 0 for the smoke test's persistence
// check. Prints "blk: ok" when every check passes.

#include "error.h"
#include "sys.h"

#define SECTOR_SIZE 512

static int check_roundtrip(void) {
    unsigned char wbuf0[SECTOR_SIZE];
    unsigned char wbuf1[SECTOR_SIZE];
    unsigned char rbuf[SECTOR_SIZE];
    const char sig[] = "ARBLEM-BLK-OK";

    for (unsigned long i = 0; i < SECTOR_SIZE; i++) {
        wbuf0[i] = (unsigned char)((i * 37 + 11) & 0xff);
        wbuf1[i] = (unsigned char)((i * 53 + 97) & 0xff);
        rbuf[i] = 0;
    }

    // Place a recognizable signature at the start of sector 0 so the smoke
    // test can verify persistence in the disk image after QEMU exits.
    for (unsigned long i = 0; i < sizeof(sig); i++) {
        wbuf0[i] = (unsigned char)sig[i];
    }

    if (sys_blk_write(0, wbuf0) != 0 || sys_blk_write(1, wbuf1) != 0) {
        return -1;
    }

    if (sys_blk_read(1, rbuf) != 0) {
        return -1;
    }

    for (unsigned long i = 0; i < SECTOR_SIZE; i++) {
        if (rbuf[i] != wbuf1[i]) {
            return -1;
        }
    }

    if (sys_blk_read(0, rbuf) != 0) {
        return -1;
    }

    for (unsigned long i = 0; i < SECTOR_SIZE; i++) {
        if (rbuf[i] != wbuf0[i]) {
            return -1;
        }
    }

    return 0;
}

static int check_errors(void) {
    unsigned char buf[SECTOR_SIZE];

    for (unsigned long i = 0; i < SECTOR_SIZE; i++) {
        buf[i] = 0;
    }

    // Sector past the end of the 1 MiB (2048-sector) disk must fail.
    if (sys_blk_read(1000000, buf) != -ERR_IO ||
        sys_blk_write(1000000, buf) != -ERR_IO) {
        return -1;
    }

    // Unmapped user pointer must return -ERR_FAULT.
    if (sys_blk_read(0, 0) != -ERR_FAULT || sys_blk_write(0, 0) != -ERR_FAULT) {
        return -1;
    }

    return 0;
}

int main(void) {
    if (check_roundtrip() != 0) {
        sys_puts("blk: roundtrip failed\n");

        return 1;
    }

    if (check_errors() != 0) {
        sys_puts("blk: errors failed\n");

        return 1;
    }

    sys_puts("blk: ok\n");

    return 0;
}
