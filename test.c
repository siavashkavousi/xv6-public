#include "types.h"
#include "user.h"
#include "fcntl.h"

#define N 100

void
test(void)
{
    int n;

    for (n = 0; n < N; n++) {
        printf(1, "number %d\n", n);
    }
    save_process();
    load_process();
}

int
main(void)
{
    test();
    exit();
}
