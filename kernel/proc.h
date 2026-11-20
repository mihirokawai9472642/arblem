#pragma once

// Maximum arguments and total string bytes (including NULs) passed to spawn.
#define PROC_ARG_MAX   16
#define PROC_ARG_BYTES 256

// Start the embedded program called name as a child of the current thread,
// passing argc strings from argv (each NUL-terminated). If argc is 0, name
// alone is passed as argv[0]. Returns the thread ID (PID), or a negative
// -ERR_* code. See docs/processes.md.
int proc_spawn(const char *name, int argc, const char *const *argv);
