#pragma once

#define THREAD_FRAME_SIZE 112

#define THREAD_FILE_MAX 8

#ifndef __ASSEMBLER__
struct file;
int thread_create(void (*entry)(unsigned long), unsigned long arg, unsigned long *vm);
void thread_start(void);
void thread_yield(void);
void thread_sleep(void *chan);
void thread_wakeup(void *chan);
void thread_exit(int code);
int thread_wait(int id, int *code);
struct file *thread_file(unsigned long fd);
void thread_set_file(int id, unsigned long fd, struct file *f);
#endif
