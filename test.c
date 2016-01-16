#include "types.h"
#include "user.h"

#define N 5

int stdout = 1;

int
main(void) {
    int pid;

    printf(1, "pid: %d\n", getpid());
    pid = fork();
    if (pid < 0) {
        printf(stdout, "fork failed\n");
        exit();
    }

    if (pid == 0) {
        int n;
        for (n = 0; n < N; n++) {
            printf(stdout, "number %d\n", n);
        }

        printf(stdout, "save_process is about to call...\n");
        suspend_process("backup");
        printf(stdout, "save_process is called!\n");

        for (n = N; n > 0; n--) {
            printf(stdout, "number %d\n", n);
        }

        exit();
    }

//    pid = fork();
//    if (pid < 0) {
//        printf(stdout, "fork failed\n");
//        exit();
//    }
//
//    if (pid == 0) {
//        load_process();
//        exit();
//    }

    wait();
//    sleep(100);

    printf(stdout, "now it's time to load the process\n");

    resume_process("backup");

    printf(stdout, "I'm done!\n");

    wait();
    exit();
}