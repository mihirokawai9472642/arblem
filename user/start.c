// Program entry point, linked into every user program. The kernel enters
// U-mode here with sp at the top of the stack page. Returning from main ends
// the program with main's return value as the exit status.

#include "sys.h"

int main(void);

void _start(void) {
    sys_exit(main());
}
