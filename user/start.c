#include "sys.h"

int main(void);

__attribute__((section(".text.start")))
void _start(void) {
    sys_exit(main());
}
