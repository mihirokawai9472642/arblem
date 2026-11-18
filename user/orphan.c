// Spawn hello and exit without waiting for it, which leaves hello an orphan.
// The smoke test runs this several times to check that orphans are freed.

#include "sys.h"

int main(void) {
    sys_spawn("hello", str_len("hello"));

    return 0;
}
