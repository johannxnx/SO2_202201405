#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>

#define SYS_GET_PROCESS_INFO 463

struct process_info {
    int pid;
    char name[16];
    unsigned long long exec_time_ms;
    unsigned long mem_kb;
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <PID>\n", argv[0]);
        return 1;
    }

    int pid = atoi(argv[1]);
    struct process_info info;
    memset(&info, 0, sizeof(info));

    long ret = syscall(SYS_GET_PROCESS_INFO, pid, &info);

    if (ret != 0) {
        perror("syscall falló");
        printf("errno: %d\n", errno);
        return 1;
    }

    printf("=== Información del Proceso ===\n");
    printf("PID           : %d\n", info.pid);
    printf("Nombre        : %s\n", info.name);
    printf("Tiempo ejec.  : %llu ms\n", info.exec_time_ms);
    printf("Memoria aprox.: %lu KB\n", info.mem_kb);

    return 0;
}
