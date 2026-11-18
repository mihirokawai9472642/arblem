#pragma once

// Bytes thread_switch pushes: ra and s0-s11 (13 registers), rounded up to keep
// sp 16-byte aligned. Must match thread.S and the frame built by
// thread_create.
#define THREAD_FRAME_SIZE 112

// File descriptors per thread.
#define THREAD_FILE_MAX 8

#ifndef __ASSEMBLER__
struct file;

// Create a thread that will run entry(arg) on its own kernel stack, using the
// page table vm (0 = the kernel's). It inherits the current thread's files.
// Returns the thread ID, or -ERR_AGAIN or -ERR_NOMEM.
int thread_create(void (*entry)(unsigned long), unsigned long arg, unsigned long *vm);

// Create the idle thread and start running thread 0. Does not return.
void thread_start(void);

// Switch to the next runnable thread, round robin. Returns when this thread
// is scheduled again.
void thread_yield(void);

// Block until thread_wakeup(chan). chan is any address, used as a name.
void thread_sleep(void *chan);

// Make every thread sleeping on chan runnable.
void thread_wakeup(void *chan);

// End the current thread with exit status code. Does not return. The thread
// stays allocated until its parent calls thread_wait.
void thread_exit(int code);

// Wait for child thread id to exit, store its exit status in *code, and free
// it. Returns 0, or -ERR_CHILD if id is not a child of the current thread.
int thread_wait(int id, int *code);

// The current thread's file for fd, or 0 if fd is out of range or unused.
struct file *thread_file(unsigned long fd);

// Install file f as fd in thread id. fd must be below THREAD_FILE_MAX.
void thread_set_file(int id, unsigned long fd, struct file *f);
#endif
