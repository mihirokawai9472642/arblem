#pragma once

// Kernel filesystem interface, built on kernel/bcache.c and inc/fs.h.

#include "fs.h"

struct file;

// Mount the filesystem by reading and validating the superblock at block 1.
// Returns 0 on success, -1 if no valid filesystem is found.
int fs_init(void);

// Return 1 if a valid filesystem is mounted, 0 otherwise.
int fs_mounted(void);

// Copy the mounted superblock into *out. Returns 0, or -ERR_IO if unmounted.
int fs_super_info(struct fs_super *out);

// Read type and byte size of inode inum into *type and *size (either may be 0).
// Returns 0, or a negative -ERR_* code.
int fs_stat(unsigned int inum, unsigned short *type, unsigned int *size);

// Resolve path (absolute "/a/b" or relative to root "a/b") to an inode number.
// Returns the inode number (> 0), or a negative -ERR_* code.
int fs_lookup(const char *path);

// Free all data blocks of inode inum and reset its size to 0.
// Returns 0, or a negative -ERR_* code.
int fs_trunc(unsigned int inum);

// Read up to len bytes from inode inum starting at offset off into kernel
// buffer dst. Returns the number of bytes read (>= 0), or a negative -ERR_*.
long fs_read(unsigned int inum, unsigned int off, void *dst, unsigned int len);

// Write len bytes from kernel buffer src into inode inum starting at offset
// off, growing the file if needed. Returns the number of bytes written (>= 0),
// or a negative -ERR_*.
long fs_write(unsigned int inum, unsigned int off, const void *src,
              unsigned int len);

// Create a new file (FS_TYPE_FILE) or directory (FS_TYPE_DIR) at path.
// Returns the new inode number (> 0), or a negative -ERR_* code.
int fs_create(const char *path, unsigned short type);

// Remove the file or empty directory at path and free its inode and blocks.
// Returns 0, or a negative -ERR_* code.
int fs_unlink(const char *path);

// Open path with flags (O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_TRUNC) and
// return a struct file with ref = 1 in *out. Returns 0, or a negative -ERR_*.
int fs_open(const char *path, unsigned int flags, struct file **out);
