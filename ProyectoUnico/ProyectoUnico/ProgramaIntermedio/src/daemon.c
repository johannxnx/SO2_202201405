#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "daemon.h"

int iniciar_daemon(void)
{
    pid_t pid;

    pid = fork();

    if (pid < 0) {
        perror("Error al hacer fork");
        return -1;
    }

    if (pid > 0) {
        printf("Daemon iniciado con PID hijo: %d\n", pid);
        exit(0);
    }

    umask(0);

    if (setsid() < 0) {
        perror("Error al crear nueva sesion");
        return -1;
    }

    return 0;
}
