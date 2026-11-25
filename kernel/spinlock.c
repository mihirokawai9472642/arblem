// Spinlock implementation using RISC-V atomic instructions and per-hart
// nested interrupt disabling.

#include "spinlock.h"
#include "cpu.h"
#include "kprintf.h"
#include "panic.h"
#include "riscv.h"

void spinlock_init(struct spinlock *lk, const char *name) {
    lk->locked = 0;
    lk->owner = 0;
    lk->name = name;
}

void spinlock_push_off(void) {
    // Disable interrupts before reading cpu_current() so this thread cannot
    // migrate to another hart between the two steps.
    unsigned long old = csr_read(sstatus);

    csr_clear(sstatus, SSTATUS_SIE);

    struct cpu *c = cpu_current();

    if (c->lock_depth == 0) {
        c->int_was_on = (old & SSTATUS_SIE) != 0;
    }

    c->lock_depth++;
}

void spinlock_pop_off(void) {
    if ((csr_read(sstatus) & SSTATUS_SIE) != 0) {
        panic("spinlock_pop_off: interrupts enabled");
    }

    struct cpu *c = cpu_current();

    if (c->lock_depth < 1) {
        panic("spinlock_pop_off: negative depth");
    }

    c->lock_depth--;

    if (c->lock_depth == 0 && c->int_was_on) {
        csr_set(sstatus, SSTATUS_SIE);
    }
}

int spinlock_holding(struct spinlock *lk) {
    return __atomic_load_n(&lk->locked, __ATOMIC_RELAXED) != 0 &&
           __atomic_load_n(&lk->owner, __ATOMIC_RELAXED) == cpu_current();
}

void spinlock_acquire(struct spinlock *lk) {
    spinlock_push_off();

    if (spinlock_holding(lk)) {
        kprintf("spinlock: %s\n", lk->name ? lk->name : "?");
        panic("spinlock_acquire: already held");
    }

    // Atomically swap 1 into lk->locked until we observe the previous value 0.
    // Acquire ordering prevents memory accesses in the critical section from
    // moving before the lock is taken.
    while (__atomic_exchange_n(&lk->locked, 1U, __ATOMIC_ACQUIRE) != 0) {
    }

    __atomic_store_n(&lk->owner, cpu_current(), __ATOMIC_RELAXED);
}

void spinlock_release(struct spinlock *lk) {
    if (!spinlock_holding(lk)) {
        kprintf("spinlock: %s\n", lk->name ? lk->name : "?");
        panic("spinlock_release: not held");
    }

    // Clear owner before releasing locked, and use release ordering so every
    // write in the critical section becomes visible before another hart can
    // acquire the lock.
    __atomic_store_n(&lk->owner, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&lk->locked, 0U, __ATOMIC_RELEASE);

    spinlock_pop_off();
}
