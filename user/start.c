#include "sys.h"

int main(void);

void _start(void) {
    sys_exit(main());
}
