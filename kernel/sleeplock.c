// Sleeplock: built on a spinlock plus thread_sleep/thread_wakeup.

#include "sleeplock.h"
#include "panic.h"
#include "thread.h"

void sleeplock_init(struct sleeplock *sl, const char *name) {
    spinlock_init(&sl->lk, name);
    sl->locked = 0;
}

void sleeplock_acquire(struct sleeplock *sl) {
    spinlock_acquire(&sl->lk);

    while (sl->locked) {
        thread_sleep(sl, &sl->lk);
    }

    sl->locked = 1;
    spinlock_release(&sl->lk);
}

void sleeplock_release(struct sleeplock *sl) {
    spinlock_acquire(&sl->lk);

    if (!sl->locked) {
        spinlock_release(&sl->lk);
        panic("sleeplock_release: not held");
    }

    sl->locked = 0;
    thread_wakeup(sl);
    spinlock_release(&sl->lk);
}
