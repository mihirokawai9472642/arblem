#pragma once

#include "page.h"

// User address space layout in the 1 GiB user slot [PROC_BASE, PROC_STACK_TOP):
//   [PROC_BASE, heap_base)                        ELF segments
//   [heap_base, heap_end)                         demand-paged heap (sbrk)
//   [PROC_STACK_MIN - PAGE_SIZE, PROC_STACK_MIN)  unmapped stack guard page
//   [PROC_STACK_MIN, PROC_STACK_TOP)              demand-paged stack (64 KiB)
#define PROC_BASE       0x40000000UL
#define PROC_STACK_TOP  0x80000000UL
#define PROC_STACK_MAX  (16UL * PAGE_SIZE)
#define PROC_STACK_MIN  (PROC_STACK_TOP - PROC_STACK_MAX)
#define PROC_HEAP_LIMIT (PROC_STACK_MIN - PAGE_SIZE)

// Maximum arguments and total string bytes (including NULs) passed to spawn.
#define PROC_ARG_MAX   16
#define PROC_ARG_BYTES 256

// Start the embedded program called name as a child of the current thread,
// passing argc strings from argv (each NUL-terminated). If argc is 0, name
// alone is passed as argv[0]. Returns the thread ID (PID), or a negative
// -ERR_* code. See docs/processes.md.
int proc_spawn(const char *name, int argc, const char *const *argv);
