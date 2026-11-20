// Program entry point, linked into every user program. The kernel enters
// U-mode here with sp on the user stack and argc/argv in a0/a1. Returning
// from main ends the program with main's return value as the exit status.

#include "sys.h"

int main(int argc, char **argv);

void _start(int argc, char **argv) {
    sys_exit(main(argc, argv));
}
