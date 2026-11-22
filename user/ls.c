// List directory entries (default "/"). Reads struct fs_dirent records from
// the open directory descriptor.

#include "sys.h"

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "/";
    long fd = sys_open(path, O_RDONLY);

    if (fd < 0) {
        sys_puts("ls: cannot open\n");

        return 1;
    }

    struct fs_dirent de;

    for (;;) {
        // Zero before sys_read so static analysis sees de as initialized.
        de.inum = 0;
        de.name[0] = '\0';

        long n = sys_read((int)fd, (char *)&de, sizeof(de));

        if (n < 0) {
            sys_puts("ls: read failed\n");
            sys_close((int)fd);

            return 1;
        }

        if (n < (long)sizeof(de)) {
            break;
        }

        if (de.inum != 0) {
            de.name[FS_NAME_MAX - 1] = '\0';
            sys_puts(de.name);
            sys_putc('\n');
        }
    }

    sys_close((int)fd);

    return 0;
}
