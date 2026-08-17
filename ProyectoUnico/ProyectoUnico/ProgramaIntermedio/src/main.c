#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "daemon.h"
#include "monitor_thread.h"
#include "scan_thread.h"

int main()
{
    pthread_t thread_monitoreo;
    pthread_t thread_escaneo;

    if (iniciar_daemon() != 0) {
        return 1;
    }

    if (pthread_create(&thread_monitoreo, NULL, hilo_monitoreo, NULL) != 0) {
        return 1;
    }

    if (pthread_create(&thread_escaneo, NULL, hilo_escaneo, NULL) != 0) {
        return 1;
    }

    pthread_join(thread_monitoreo, NULL);
    pthread_join(thread_escaneo, NULL);

    return 0;
}