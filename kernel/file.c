// Reference-counted open files. See kernel/file.h.

#include "file.h"
#include "panic.h"

struct file *file_ref(struct file *f) {
    f->ref++;

    return f;
}

void file_close(struct file *f) {
    if (f->ref <= 0) {
        panic("file_close: bad refcount");
    }

    f->ref--;

    if (f->ref == 0 && f->close != 0) {
        f->close(f);
    }
}
