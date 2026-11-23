#pragma once

// Bytes thread_switch pushes: ra and s0-s11 (13 registers), rounded up to keep
// sp 16-byte aligned. Must match thread.S and the frame built by
// thread_create.
#define THREAD_FRAME_SIZE 112

// File descriptors per thread.
#define THREAD_FILE_MAX 8

#ifndef __ASSEMBLER__
struct file;

// Create a thread in THREAD_INIT state that will run entry(arg) on its own
// kernel stack, using the page table vm (0 = the kernel's). It inherits the
// current thread's files. Call thread_ready() once setup is complete.
// Returns the thread ID, or -ERR_AGAIN or -ERR_NOMEM.
int thread_create(void (*entry)(unsigned long), unsigned long arg,
                  unsigned long *vm);

// Mark a newly created thread runnable so any hart can schedule it.
void thread_ready(int id);

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
// stays allocated until its parent calls thread_wait, or until the scheduler
// frees it if it has no parent.
void thread_exit(int code);

// Wait for child thread id to exit, store its exit status in *code, and free
// it. Returns 0, or -ERR_CHILD if id is not a child of the current thread.
int thread_wait(unsigned long id, int *code);

// The current thread's file for fd, or 0 if fd is out of range or unused.
struct file *thread_file(unsigned long fd);

// Install file f in the lowest free fd of the current thread. Returns the
// fd, or -ERR_MFILE if the table is full.
int thread_alloc_fd(struct file *f);

// Close fd in the current thread and drop its file reference. Returns 0, or
// -ERR_BADF if fd is out of range or unused.
int thread_close_fd(unsigned long fd);

// Record the initial U-mode stack pointer and argc/argv for thread id.
void thread_set_start(int id, unsigned long sp, unsigned long argc,
                      unsigned long argv);

// Read the current thread's initial U-mode stack pointer and argc/argv.
void thread_get_start(unsigned long *sp, unsigned long *argc,
                      unsigned long *argv);

// Set the initial heap base (and break) for thread id.
void thread_set_heap(int id, unsigned long base);

// Move the current thread's program break by delta bytes. Growing only moves
// the break (pages are allocated on demand); shrinking unmaps and frees pages
// above the new break. Returns the previous break, or -ERR_NOMEM on failure.
long thread_sbrk(long delta);

// Handle a demand page fault at virtual address va for the current thread
// (heap or stack growth). Returns 0 if a page was mapped, -1 otherwise.
int thread_fault(unsigned long va);
#endif
