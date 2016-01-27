#include "types.h"
#include "user.h"

#define N 5

int stdout = 1;

int
main(void) {
    printf(stdout, "now it's time to load the process\n");

    resume("backup");

    wait();
    printf(stdout, "I'm done!\n");
    exit();
}