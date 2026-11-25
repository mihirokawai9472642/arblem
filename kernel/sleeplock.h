#pragma once

// Long-term mutual-exclusion lock that sleeps while contended instead of
// spinning. Safe to hold across disk I/O and other operations that sleep.

#include "spinlock.h"

struct sleeplock {
    struct spinlock lk;
    int locked;
};

#define SLEEPLOCK_INIT(lock_name) {.lk = SPINLOCK_INIT(lock_name), .locked = 0}

void sleeplock_init(struct sleeplock *sl, const char *name);
void sleeplock_acquire(struct sleeplock *sl);
void sleeplock_release(struct sleeplock *sl);
