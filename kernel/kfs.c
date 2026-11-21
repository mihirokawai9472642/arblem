// Simple block-based filesystem on top of kernel/bcache.c.
// All disk writes are write-through via bcache_write.

#include "kfs.h"
#include "bcache.h"
#include "error.h"
#include "str.h"

static struct fs_super fs_sb;
static int fs_ready;

static void fs_zero(void *dst, unsigned long len) {
    unsigned char *p = dst;

    for (unsigned long i = 0; i < len; i++) {
        p[i] = 0;
    }
}

static void fs_copy(void *dst, const void *src, unsigned long len) {
    unsigned char *d = dst;
    const unsigned char *s = src;

    for (unsigned long i = 0; i < len; i++) {
        d[i] = s[i];
    }
}

int fs_init(void) {
    struct bcache_buf *b = bcache_get(FS_SUPER_BLOCK);

    if (b == 0) {
        return -1;
    }

    fs_copy(&fs_sb, b->data, sizeof(fs_sb));
    bcache_put(b);

    if (fs_sb.magic != FS_MAGIC || fs_sb.total_blocks == 0 ||
        fs_sb.inode_blocks == 0 || fs_sb.num_inodes <= FS_ROOT_INUM ||
        fs_sb.data_start >= fs_sb.total_blocks ||
        fs_sb.bitmap_block >= fs_sb.total_blocks ||
        fs_sb.inode_start + fs_sb.inode_blocks > fs_sb.total_blocks) {
        return -1;
    }

    fs_ready = 1;

    return 0;
}

int fs_mounted(void) {
    return fs_ready;
}

int fs_super_info(struct fs_super *out) {
    if (!fs_ready) {
        return -ERR_IO;
    }

    fs_copy(out, &fs_sb, sizeof(fs_sb));

    return 0;
}

static int fs_inode_read(unsigned int inum, struct fs_dinode *out) {
    if (inum == 0 || inum >= fs_sb.num_inodes) {
        return -ERR_NOENT;
    }

    unsigned int blk = fs_sb.inode_start + inum / FS_INODES_PER_BLOCK;
    unsigned int idx = inum % FS_INODES_PER_BLOCK;
    struct bcache_buf *b = bcache_get(blk);

    if (b == 0) {
        return -ERR_IO;
    }

    fs_copy(out, b->data + idx * sizeof(struct fs_dinode), sizeof(*out));
    bcache_put(b);

    return 0;
}

static int fs_inode_write(unsigned int inum, const struct fs_dinode *ino) {
    if (inum == 0 || inum >= fs_sb.num_inodes) {
        return -ERR_INVAL;
    }

    unsigned int blk = fs_sb.inode_start + inum / FS_INODES_PER_BLOCK;
    unsigned int idx = inum % FS_INODES_PER_BLOCK;
    struct bcache_buf *b = bcache_get(blk);

    if (b == 0) {
        return -ERR_IO;
    }

    fs_copy(b->data + idx * sizeof(struct fs_dinode), ino, sizeof(*ino));

    int err = bcache_write(b);

    bcache_put(b);

    return err;
}

// Allocate one free data block from the bitmap and zero it on disk.
static int fs_balloc(unsigned int *out_blk) {
    struct bcache_buf *bm = bcache_get(fs_sb.bitmap_block);

    if (bm == 0) {
        return -ERR_IO;
    }

    for (unsigned int b = fs_sb.data_start; b < fs_sb.total_blocks; b++) {
        unsigned int byte = b / 8;
        unsigned char mask = (unsigned char)(1U << (b % 8));

        if (byte >= FS_BLOCK_SIZE) {
            break;
        }

        if ((bm->data[byte] & mask) != 0) {
            continue;
        }

        bm->data[byte] |= mask;

        int err = bcache_write(bm);

        bcache_put(bm);

        if (err < 0) {
            return err;
        }

        struct bcache_buf *db = bcache_get(b);

        if (db == 0) {
            return -ERR_IO;
        }

        fs_zero(db->data, FS_BLOCK_SIZE);
        err = bcache_write(db);
        bcache_put(db);

        if (err < 0) {
            return err;
        }

        *out_blk = b;

        return 0;
    }

    bcache_put(bm);

    return -ERR_NOSPC;
}

static void fs_bfree(unsigned int blk) {
    if (blk < fs_sb.data_start || blk >= fs_sb.total_blocks) {
        return;
    }

    struct bcache_buf *bm = bcache_get(fs_sb.bitmap_block);

    if (bm == 0) {
        return;
    }

    unsigned int byte = blk / 8;
    unsigned char mask = (unsigned char)(1U << (blk % 8));

    if (byte < FS_BLOCK_SIZE) {
        bm->data[byte] &= (unsigned char)~mask;
        bcache_write(bm);
    }

    bcache_put(bm);
}

// Map logical block index idx of ino to an on-disk block number in *out_blk.
// If alloc is non-zero, missing direct or indirect blocks are allocated and
// *dirty is set to 1 whenever ino itself changes.
static int fs_bmap(struct fs_dinode *ino, unsigned int idx, int alloc,
                   unsigned int *out_blk, int *dirty) {
    if (idx < FS_NDIRECT) {
        if (ino->direct[idx] == 0 && alloc) {
            int err = fs_balloc(&ino->direct[idx]);

            if (err < 0) {
                return err;
            }

            *dirty = 1;
        }

        *out_blk = ino->direct[idx];

        return 0;
    }

    unsigned int ind_idx = idx - FS_NDIRECT;

    if (ind_idx >= FS_NINDIRECT) {
        return -ERR_NOSPC;
    }

    if (ino->indirect == 0) {
        if (!alloc) {
            *out_blk = 0;

            return 0;
        }

        int err = fs_balloc(&ino->indirect);

        if (err < 0) {
            return err;
        }

        *dirty = 1;
    }

    struct bcache_buf *ib = bcache_get(ino->indirect);

    if (ib == 0) {
        return -ERR_IO;
    }

    unsigned long entry_off = (unsigned long)ind_idx * sizeof(unsigned int);
    unsigned int blk = 0;

    fs_copy(&blk, ib->data + entry_off, sizeof(blk));

    if (blk == 0 && alloc) {
        int err = fs_balloc(&blk);

        if (err < 0) {
            bcache_put(ib);

            return err;
        }

        fs_copy(ib->data + entry_off, &blk, sizeof(blk));
        err = bcache_write(ib);
        bcache_put(ib);

        if (err < 0) {
            return err;
        }

        *out_blk = blk;

        return 0;
    }

    bcache_put(ib);
    *out_blk = blk;

    return 0;
}

int fs_stat(unsigned int inum, unsigned short *type, unsigned int *size) {
    if (!fs_ready) {
        return -ERR_IO;
    }

    struct fs_dinode ino;
    int err = fs_inode_read(inum, &ino);

    if (err < 0) {
        return err;
    }

    if (ino.type == FS_TYPE_FREE) {
        return -ERR_NOENT;
    }

    if (type != 0) {
        *type = ino.type;
    }

    if (size != 0) {
        *size = ino.size;
    }

    return 0;
}

int fs_trunc(unsigned int inum) {
    if (!fs_ready) {
        return -ERR_IO;
    }

    struct fs_dinode ino;
    int err = fs_inode_read(inum, &ino);

    if (err < 0) {
        return err;
    }

    for (unsigned int i = 0; i < FS_NDIRECT; i++) {
        if (ino.direct[i] != 0) {
            fs_bfree(ino.direct[i]);
            ino.direct[i] = 0;
        }
    }

    if (ino.indirect != 0) {
        struct bcache_buf *ib = bcache_get(ino.indirect);

        if (ib == 0) {
            return -ERR_IO;
        }

        for (unsigned long i = 0; i < FS_NINDIRECT; i++) {
            unsigned int blk = 0;

            fs_copy(&blk, ib->data + i * sizeof(unsigned int), sizeof(blk));

            if (blk != 0) {
                fs_bfree(blk);
            }
        }

        bcache_put(ib);
        fs_bfree(ino.indirect);
        ino.indirect = 0;
    }

    ino.size = 0;

    return fs_inode_write(inum, &ino);
}

long fs_read(unsigned int inum, unsigned int off, void *dst, unsigned int len) {
    if (!fs_ready) {
        return -ERR_IO;
    }

    struct fs_dinode ino;
    int err = fs_inode_read(inum, &ino);

    if (err < 0) {
        return err;
    }

    if (ino.type == FS_TYPE_FREE) {
        return -ERR_NOENT;
    }

    if (off >= ino.size || len == 0) {
        return 0;
    }

    if (len > ino.size - off) {
        len = ino.size - off;
    }

    unsigned char *out = dst;
    unsigned int done = 0;

    while (done < len) {
        unsigned int cur = off + done;
        unsigned int blk = 0;
        int dirty = 0;

        err = fs_bmap(&ino, cur / FS_BLOCK_SIZE, 0, &blk, &dirty);

        if (err < 0) {
            return err;
        }

        unsigned int boff = cur % FS_BLOCK_SIZE;
        unsigned int n = FS_BLOCK_SIZE - boff;

        if (n > len - done) {
            n = len - done;
        }

        if (blk == 0) {
            fs_zero(out + done, n);
        } else {
            struct bcache_buf *b = bcache_get(blk);

            if (b == 0) {
                return -ERR_IO;
            }

            fs_copy(out + done, b->data + boff, n);
            bcache_put(b);
        }

        done += n;
    }

    return (long)done;
}

long fs_write(unsigned int inum, unsigned int off, const void *src,
              unsigned int len) {
    if (!fs_ready) {
        return -ERR_IO;
    }

    struct fs_dinode ino;
    int err = fs_inode_read(inum, &ino);

    if (err < 0) {
        return err;
    }

    if (ino.type == FS_TYPE_FREE) {
        return -ERR_NOENT;
    }

    unsigned long max_bytes = (unsigned long)FS_MAX_BLOCKS * FS_BLOCK_SIZE;

    if ((unsigned long)off >= max_bytes) {
        return -ERR_NOSPC;
    }

    if ((unsigned long)len > max_bytes - off) {
        len = (unsigned int)(max_bytes - off);
    }

    const unsigned char *in = src;
    unsigned int done = 0;
    int dirty = 0;

    while (done < len) {
        unsigned int cur = off + done;
        unsigned int blk = 0;

        err = fs_bmap(&ino, cur / FS_BLOCK_SIZE, 1, &blk, &dirty);

        if (err < 0) {
            if (dirty) {
                fs_inode_write(inum, &ino);
            }

            return done > 0 ? (long)done : err;
        }

        unsigned int boff = cur % FS_BLOCK_SIZE;
        unsigned int n = FS_BLOCK_SIZE - boff;

        if (n > len - done) {
            n = len - done;
        }

        struct bcache_buf *b = bcache_get(blk);

        if (b == 0) {
            if (dirty) {
                fs_inode_write(inum, &ino);
            }

            return done > 0 ? (long)done : -ERR_IO;
        }

        fs_copy(b->data + boff, in + done, n);
        err = bcache_write(b);
        bcache_put(b);

        if (err < 0) {
            if (dirty) {
                fs_inode_write(inum, &ino);
            }

            return done > 0 ? (long)done : err;
        }

        done += n;
    }

    if (off + done > ino.size) {
        ino.size = off + done;
        dirty = 1;
    }

    if (dirty) {
        err = fs_inode_write(inum, &ino);

        if (err < 0) {
            return err;
        }
    }

    return (long)done;
}

// Extract the next path component from *pp into name. Returns 1 if a component
// was extracted, 0 at end of path, or -ERR_INVAL if the component is too long.
static int fs_path_next(const char **pp, char *name) {
    const char *p = *pp;

    while (*p == '/') {
        p++;
    }

    if (*p == '\0') {
        *pp = p;

        return 0;
    }

    unsigned int len = 0;

    while (*p != '\0' && *p != '/') {
        if (len >= FS_NAME_MAX - 1) {
            return -ERR_INVAL;
        }

        name[len++] = *p++;
    }

    name[len] = '\0';
    *pp = p;

    return 1;
}

static int fs_dir_lookup(unsigned int dir_inum, const char *name,
                         unsigned int *out_off) {
    struct fs_dinode ino;
    int err = fs_inode_read(dir_inum, &ino);

    if (err < 0) {
        return err;
    }

    if (ino.type != FS_TYPE_DIR) {
        return -ERR_NOTDIR;
    }

    struct fs_dirent de;

    for (unsigned int off = 0; off + sizeof(de) <= ino.size;
         off += sizeof(de)) {
        fs_zero(&de, sizeof(de));

        long n = fs_read(dir_inum, off, &de, sizeof(de));

        if (n < 0) {
            return (int)n;
        }

        if (n != (long)sizeof(de)) {
            break;
        }

        if (de.inum != 0 && str_equal(de.name, name)) {
            if (out_off != 0) {
                *out_off = off;
            }

            return (int)de.inum;
        }
    }

    return -ERR_NOENT;
}

int fs_lookup(const char *path) {
    if (!fs_ready) {
        return -ERR_IO;
    }

    if (path[0] == '\0') {
        return -ERR_NOENT;
    }

    unsigned int cur = FS_ROOT_INUM;
    const char *p = path;
    char name[FS_NAME_MAX];

    for (;;) {
        int r = fs_path_next(&p, name);

        if (r < 0) {
            return r;
        }

        if (r == 0) {
            return (int)cur;
        }

        int next = fs_dir_lookup(cur, name, 0);

        if (next < 0) {
            return next;
        }

        cur = (unsigned int)next;
    }
}
