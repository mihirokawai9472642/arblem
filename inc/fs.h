#pragma once

// Shared on-disk filesystem layout and open flags.
// Used by the kernel (kernel/fs.c), user programs (ls, fscheck, ...), and the
// host image builder (tools/mkfs.c).

#define FS_MAGIC      0x41524246U // "ARBF"
#define FS_BLOCK_SIZE 4096U
#define FS_NAME_MAX   28U // Max filename length including NUL
#define FS_NDIRECT    12U // Direct block pointers per inode
#define FS_NINDIRECT  (FS_BLOCK_SIZE / sizeof(unsigned int)) // 1024
#define FS_MAX_BLOCKS (FS_NDIRECT + FS_NINDIRECT)
#define FS_ROOT_INUM  1U

// Disk layout (4 KiB blocks):
//   block 0:                        reserved / boot block (untouched by FS)
//   block 1:                        superblock (struct fs_super)
//   blocks [inode_start .. +count): inode table (64-byte struct fs_dinode)
//   block bitmap_block:             allocation bitmap (1 bit per block)
//   blocks [data_start .. total):   file data, directories, indirect blocks
#define FS_SUPER_BLOCK 1U

// Inode types.
#define FS_TYPE_FREE 0U
#define FS_TYPE_FILE 1U
#define FS_TYPE_DIR  2U

// Flags for the open() system call.
#define O_RDONLY 0x0U
#define O_WRONLY 0x1U
#define O_RDWR   0x2U
#define O_CREAT  0x4U
#define O_TRUNC  0x8U

struct fs_super {
    unsigned int magic;
    unsigned int total_blocks;
    unsigned int inode_start;
    unsigned int inode_blocks;
    unsigned int num_inodes;
    unsigned int bitmap_block;
    unsigned int data_start;
};

struct fs_dinode {
    unsigned short type;
    unsigned short nlink;
    unsigned int size;
    unsigned int direct[FS_NDIRECT];
    unsigned int indirect;
    unsigned int reserved;
};

struct fs_dirent {
    unsigned int inum; // 0 = unused slot
    char name[FS_NAME_MAX];
};

#define FS_INODES_PER_BLOCK  (FS_BLOCK_SIZE / sizeof(struct fs_dinode))
#define FS_DIRENTS_PER_BLOCK (FS_BLOCK_SIZE / sizeof(struct fs_dirent))

_Static_assert(sizeof(struct fs_dinode) == 64, "fs_dinode must be 64 bytes");
_Static_assert(sizeof(struct fs_dirent) == 32, "fs_dirent must be 32 bytes");
