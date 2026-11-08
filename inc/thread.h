#pragma once

void thread_create(void (*entry)(void));
void thread_start(void);
void thread_yield(void);
