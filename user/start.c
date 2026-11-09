#include "sys.h"

int main(void);

__attribute__((section(".text.start")))
void _start(void) {
    main();
    sys_exit();
}
