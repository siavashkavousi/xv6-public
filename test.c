#include "user.h"

#define N 100

void
test(void)
{
    int n;

    for (n = 0; n < N; n++) {
        printf(1, "number %d\n", n);
    }
}

int
main(void)
{
    test();
    exit();
}
