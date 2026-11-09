#pragma once

void thread_create(void (*entry)(void), unsigned long *vm);
void thread_start(void);
void thread_yield(void);
void thread_sleep(void *chan);
void thread_wakeup(void *chan);
void thread_exit(void);
