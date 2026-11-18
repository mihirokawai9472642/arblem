#pragma once

// Print "panic: msg" and power off the machine, reporting a failure.
void panic(const char *msg) __attribute__((noreturn));
