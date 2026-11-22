// Host tool to build a 2 MiB Arblem filesystem image populated with the given
// files in the root directory. Usage: mkfs <output.img> [files...]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs.h"

#define MKFS_BLOCKS       512U // 512 * 4 KiB = 2 MiB
#define MKFS_INODE_START  2U
#define MKFS_INODE_BLOCKS 2U
#define MKFS_BITMAP_BLOCK 4U
#define MKFS_DATA_START   5U
#define MKFS_NUM_INODES   (MKFS_INODE_BLOCKS * FS_INODES_PER_BLOCK) // 128

// Whole 2 MiB image held in memory, page-aligned so block casts are aligned.
static _Alignas(FS_BLOCK_SIZE) unsigned char img[MKFS_BLOCKS][FS_BLOCK_SIZE];
static unsigned int next_block = MKFS_DATA_START;
static unsigned int next_inum = FS_ROOT_INUM + 1;

static void bitmap_mark(unsigned int b) {
    img[MKFS_BITMAP_BLOCK][b / 8] |= (unsigned char)(1U << (b % 8));
}

static unsigned int alloc_block(void) {
    if (next_block >= MKFS_BLOCKS) {
        fprintf(stderr, "mkfs: out of disk space\n");
        exit(1);
    }

    unsigned int b = next_block++;

    bitmap_mark(b);

    return b;
}

static struct fs_dinode *get_inode(unsigned int inum) {
    unsigned int blk = MKFS_INODE_START + inum / FS_INODES_PER_BLOCK;
    unsigned int idx = inum % FS_INODES_PER_BLOCK;
    struct fs_dinode *table = (struct fs_dinode *)img[blk];

    return &table[idx];
}

// Return the block number for logical block idx of ino, allocating as needed.
static unsigned int inode_block(struct fs_dinode *ino, unsigned int idx) {
    if (idx < FS_NDIRECT) {
        if (ino->direct[idx] == 0) {
            ino->direct[idx] = alloc_block();
        }

        return ino->direct[idx];
    }

    unsigned int ind_idx = idx - FS_NDIRECT;

    if (ind_idx >= FS_NINDIRECT) {
        fprintf(stderr, "mkfs: file too large\n");
        exit(1);
    }

    if (ino->indirect == 0) {
        ino->indirect = alloc_block();
    }

    unsigned int *entries = (unsigned int *)img[ino->indirect];

    if (entries[ind_idx] == 0) {
        entries[ind_idx] = alloc_block();
    }

    return entries[ind_idx];
}

static void inode_append(struct fs_dinode *ino, const void *src,
                         unsigned int len) {
    const unsigned char *p = src;
    unsigned int done = 0;

    while (done < len) {
        unsigned int off = ino->size;
        unsigned int blk = inode_block(ino, off / FS_BLOCK_SIZE);
        unsigned int boff = off % FS_BLOCK_SIZE;
        unsigned int n = FS_BLOCK_SIZE - boff;

        if (n > len - done) {
            n = len - done;
        }

        memcpy(img[blk] + boff, p + done, n);
        ino->size += n;
        done += n;
    }
}

static void dir_add(struct fs_dinode *dir, const char *name,
                    unsigned int inum) {
    size_t len = strlen(name);

    if (len == 0 || len >= FS_NAME_MAX) {
        fprintf(stderr, "mkfs: invalid name: %s\n", name);
        exit(1);
    }

    struct fs_dirent de;

    memset(&de, 0, sizeof(de));
    de.inum = inum;
    memcpy(de.name, name, len);
    inode_append(dir, &de, sizeof(de));
}

// Derive the in-FS filename from a host path: take the basename and strip a
// trailing ".elf" so "build/user/hello.elf" becomes "hello".
static void extract_name(const char *path, char *out, size_t out_size) {
    const char *slash = strrchr(path, '/');
    const char *base = slash != NULL ? slash + 1 : path;
    size_t len = strlen(base);

    if (len > 4 && strcmp(base + len - 4, ".elf") == 0) {
        len -= 4;
    }

    if (len == 0 || len >= out_size) {
        fprintf(stderr, "mkfs: invalid file name: %s\n", path);
        exit(1);
    }

    memcpy(out, base, len);
    out[len] = '\0';
}

static void add_file(struct fs_dinode *root, const char *path) {
    if (next_inum >= MKFS_NUM_INODES) {
        fprintf(stderr, "mkfs: out of inodes\n");
        exit(1);
    }

    char name[FS_NAME_MAX];

    extract_name(path, name, sizeof(name));

    FILE *fp = fopen(path, "rb");

    if (fp == NULL) {
        perror(path);
        exit(1);
    }

    unsigned int inum = next_inum++;
    struct fs_dinode *ino = get_inode(inum);

    ino->type = FS_TYPE_FILE;
    ino->nlink = 1;

    unsigned char buf[FS_BLOCK_SIZE];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        inode_append(ino, buf, (unsigned int)n);
    }

    if (ferror(fp)) {
        perror(path);
        fclose(fp);
        exit(1);
    }

    fclose(fp);
    dir_add(root, name, inum);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <output.img> [files...]\n", argv[0]);
        return 1;
    }

    // Write superblock at block 1 (block 0 stays zeroed as reserved space).
    struct fs_super *sb = (struct fs_super *)img[FS_SUPER_BLOCK];

    sb->magic = FS_MAGIC;
    sb->total_blocks = MKFS_BLOCKS;
    sb->inode_start = MKFS_INODE_START;
    sb->inode_blocks = MKFS_INODE_BLOCKS;
    sb->num_inodes = MKFS_NUM_INODES;
    sb->bitmap_block = MKFS_BITMAP_BLOCK;
    sb->data_start = MKFS_DATA_START;

    // Mark block 0 and all metadata blocks as allocated in the bitmap.
    for (unsigned int b = 0; b < MKFS_DATA_START; b++) {
        bitmap_mark(b);
    }

    // Create the root directory at inode 1.
    struct fs_dinode *root = get_inode(FS_ROOT_INUM);

    root->type = FS_TYPE_DIR;
    root->nlink = 1;
    dir_add(root, ".", FS_ROOT_INUM);
    dir_add(root, "..", FS_ROOT_INUM);

    for (int i = 2; i < argc; i++) {
        add_file(root, argv[i]);
    }

    FILE *out = fopen(argv[1], "wb");

    if (out == NULL) {
        perror(argv[1]);
        return 1;
    }

    if (fwrite(img, 1, sizeof(img), out) != sizeof(img) || fclose(out) != 0) {
        perror(argv[1]);
        return 1;
    }

    return 0;
}
