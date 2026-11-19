#pragma once

// An open file, as seen through a file descriptor. Each operation takes a
// user buffer address, copies through vm_copy_*, and returns the number of
// bytes transferred or a negative -ERR_* code. A null operation means the
// file does not support it (-ERR_BADF).
//
// Files are reference-counted. When the last reference is dropped with
// file_close, close is called if non-null so the implementation can release
// its state.
struct file {
    int ref;
    long (*read)(struct file *f, unsigned long buf, unsigned long len);
    long (*write)(struct file *f, unsigned long buf, unsigned long len);
    void (*close)(struct file *f);
};

// Take a new reference to f and return f.
struct file *file_ref(struct file *f);

// Drop a reference to f, calling f->close(f) when the count reaches zero.
void file_close(struct file *f);
