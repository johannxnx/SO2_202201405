#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

#define __NR_simulate_panic 470

int main() {
    long res = syscall(__NR_simulate_panic, "TEST DESDE USER SPACE");

    printf("Resultado syscall: %ld\n", res);
    printf("errno actual: %d\n", errno);
    perror("syscall");

    return 0;
}