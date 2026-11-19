#pragma once

// Unidirectional in-memory byte stream between processes.

struct file;

// Create a pipe and return its read and write ends through *rf and *wf, each
// with ref = 1. Returns 0, or -ERR_NOMEM if no page is available.
int pipe_create(struct file **rf, struct file **wf);
