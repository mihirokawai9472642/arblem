#pragma once

// String helpers from lib/str.c, linked into the kernel and user programs.

// Return non-zero if the NUL-terminated strings a and b are equal.
int str_equal(const char *a, const char *b);

// Length of NUL-terminated s, not counting the NUL.
unsigned long str_len(const char *s);
