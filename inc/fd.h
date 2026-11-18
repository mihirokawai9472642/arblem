#pragma once

// Standard file descriptors. The shell gets the console on all three, and
// every program it spawns inherits them.

#define FD_STDIN  0
#define FD_STDOUT 1
#define FD_STDERR 2
