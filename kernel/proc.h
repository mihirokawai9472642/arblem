#pragma once

// Start the embedded program called name as a child of the current thread.
// Returns its thread ID (PID), or -ERR_NOENT, -ERR_NOEXEC, -ERR_NOMEM or
// -ERR_AGAIN. See docs/processes.md.
int proc_spawn(const char *name);
