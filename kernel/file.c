// Reference-counted open files. See kernel/file.h.

#include "file.h"
#include "panic.h"
#include "spinlock.h"

static struct spinlock file_lock = SPINLOCK_INIT("file");

struct file *file_ref(struct file *f) {
    spinlock_acquire(&file_lock);
    f->ref++;
    spinlock_release(&file_lock);

    return f;
}

void file_close(struct file *f) {
    spinlock_acquire(&file_lock);

    if (f->ref <= 0) {
        spinlock_release(&file_lock);
        panic("file_close: bad refcount");
    }

    f->ref--;
    int last = (f->ref == 0);

    spinlock_release(&file_lock);

    // Run the close callback outside file_lock because it may acquire another
    // lock (for example, a pipe lock or filesystem lock) or wake threads.
    if (last && f->close != 0) {
        f->close(f);
    }
}
