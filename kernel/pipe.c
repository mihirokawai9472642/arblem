// In-memory pipe: a ring buffer shared by a read end and a write end, both
// embedded in one physical page.

#include "pipe.h"
#include "error.h"
#include "file.h"
#include "page.h"
#include "spinlock.h"
#include "thread.h"
#include "vm.h"

// Buffer capacity (power of two so unsigned wrap-around stays aligned).
#define PIPE_BUF_SIZE 512

// Bytes copied to or from user memory per chunk.
#define PIPE_CHUNK 64

struct pipe {
    struct spinlock lock;
    struct file read_end;
    struct file write_end;
    int read_open;
    int write_open;
    unsigned long rpos;
    unsigned long wpos;
    char buf[PIPE_BUF_SIZE];
};

_Static_assert(sizeof(struct pipe) <= PAGE_SIZE,
               "struct pipe must fit in one page");

static long pipe_read(struct file *f, unsigned long buf, unsigned long len) {
    struct pipe *p = f->priv;
    char kbuf[PIPE_CHUNK];

    if (len == 0) {
        return 0;
    }

    spinlock_acquire(&p->lock);

    // Block while the buffer is empty and a writer can still produce data.
    while (p->rpos == p->wpos && p->write_open) {
        thread_sleep(&p->read_end, &p->lock);
    }

    // Buffer is empty and all write ends are closed: EOF.
    if (p->rpos == p->wpos) {
        spinlock_release(&p->lock);

        return 0;
    }

    unsigned long done = 0;

    for (;;) {
        unsigned long avail = p->wpos - p->rpos;

        if (avail == 0 || done == len) {
            spinlock_release(&p->lock);

            return (long)done;
        }

        unsigned long n = len - done;

        if (n > avail) {
            n = avail;
        }

        if (n > PIPE_CHUNK) {
            n = PIPE_CHUNK;
        }

        for (unsigned long i = 0; i < n; i++) {
            kbuf[i] = p->buf[(p->rpos + i) % PIPE_BUF_SIZE];
        }

        p->rpos += n;
        thread_wakeup(&p->write_end);
        spinlock_release(&p->lock);

        if (vm_copy_to_user(buf + done, kbuf, n) != 0) {
            return -ERR_FAULT;
        }

        done += n;

        if (done == len) {
            return (long)done;
        }

        spinlock_acquire(&p->lock);
    }
}

static long pipe_write(struct file *f, unsigned long buf, unsigned long len) {
    struct pipe *p = f->priv;
    char kbuf[PIPE_CHUNK];
    unsigned long done = 0;

    while (done < len) {
        unsigned long chunk = len - done;

        if (chunk > PIPE_CHUNK) {
            chunk = PIPE_CHUNK;
        }

        if (vm_copy_from_user(kbuf, buf + done, chunk) != 0) {
            return -ERR_FAULT;
        }

        unsigned long off = 0;

        spinlock_acquire(&p->lock);

        while (off < chunk) {
            // Block while the buffer is full and a reader is still attached.
            while (p->wpos - p->rpos == PIPE_BUF_SIZE && p->read_open) {
                thread_sleep(&p->write_end, &p->lock);
            }

            if (!p->read_open) {
                spinlock_release(&p->lock);

                return -ERR_PIPE;
            }

            unsigned long space = PIPE_BUF_SIZE - (p->wpos - p->rpos);
            unsigned long n = chunk - off;

            if (n > space) {
                n = space;
            }

            for (unsigned long i = 0; i < n; i++) {
                p->buf[(p->wpos + i) % PIPE_BUF_SIZE] = kbuf[off + i];
            }

            p->wpos += n;
            off += n;
            thread_wakeup(&p->read_end);
        }

        spinlock_release(&p->lock);
        done += chunk;
    }

    return (long)done;
}

static void pipe_close_read(struct file *f) {
    struct pipe *p = f->priv;

    spinlock_acquire(&p->lock);
    p->read_open = 0;
    thread_wakeup(&p->write_end);
    int free_pipe = !p->write_open;
    spinlock_release(&p->lock);

    if (free_pipe) {
        page_free(p);
    }
}

static void pipe_close_write(struct file *f) {
    struct pipe *p = f->priv;

    spinlock_acquire(&p->lock);
    p->write_open = 0;
    thread_wakeup(&p->read_end);
    int free_pipe = !p->read_open;
    spinlock_release(&p->lock);

    if (free_pipe) {
        page_free(p);
    }
}

int pipe_create(struct file **rf, struct file **wf) {
    struct pipe *p = page_alloc();

    if (p == 0) {
        return -ERR_NOMEM;
    }

    spinlock_init(&p->lock, "pipe");

    p->read_end.ref = 1;
    p->read_end.priv = p;
    p->read_end.read = pipe_read;
    p->read_end.write = 0;
    p->read_end.close = pipe_close_read;

    p->write_end.ref = 1;
    p->write_end.priv = p;
    p->write_end.read = 0;
    p->write_end.write = pipe_write;
    p->write_end.close = pipe_close_write;

    p->read_open = 1;
    p->write_open = 1;
    p->rpos = 0;
    p->wpos = 0;

    *rf = &p->read_end;
    *wf = &p->write_end;

    return 0;
}
