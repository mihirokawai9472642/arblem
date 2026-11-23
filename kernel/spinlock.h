#pragma once

// Mutual-exclusion spinlock with nested interrupt disabling on the local hart.
// Never sleep or call functions that may sleep while holding a spinlock.

struct cpu;

struct spinlock {
    unsigned int locked;
    struct cpu *owner;
    const char *name;
};

#define SPINLOCK_INIT(lock_name) {.locked = 0, .owner = 0, .name = (lock_name)}

void spinlock_init(struct spinlock *lk, const char *name);
void spinlock_acquire(struct spinlock *lk);
void spinlock_release(struct spinlock *lk);

// Return non-zero if the current hart holds lk. Interrupts must be off.
int spinlock_holding(struct spinlock *lk);

// Disable S-mode interrupts on the current hart, tracking nesting depth in
// cpu_current()->lock_depth. Each push_off must pair with one pop_off.
void spinlock_push_off(void);
void spinlock_pop_off(void);
