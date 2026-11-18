#pragma once

// An open file, as seen through a file descriptor. Each operation takes a
// user buffer address, copies through vm_copy_*, and returns the number of
// bytes transferred or a negative -ERR_* code. A null operation means the
// file does not support it (-ERR_BADF).
//
// Files have no reference count; the only file today is the static console.
struct file {
    long (*read)(struct file *f, unsigned long buf, unsigned long len);
    long (*write)(struct file *f, unsigned long buf, unsigned long len);
};
