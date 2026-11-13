#pragma once

#define THREAD_FRAME_SIZE 112

#ifndef __ASSEMBLER__
int thread_create(void (*entry)(unsigned long), unsigned long arg, unsigned long *vm);
void thread_start(void);
void thread_yield(void);
void thread_sleep(void *chan);
void thread_wakeup(void *chan);
void thread_exit(int code);
int thread_wait(int id, int *code);
#endif
