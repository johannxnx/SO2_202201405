#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#define MAX_TOP_PROCESSES 10
#define PROCESS_NAME_LEN 64

struct top_process_info {
    int pid;
    char name[PROCESS_NAME_LEN];
    unsigned long memory_usage;
};

struct system_monitor_info {
    unsigned long memoria_usada;
    unsigned long memoria_libre;
    unsigned long memoria_cache;
    unsigned long swap_usada;
    unsigned long fallos_menores;
    unsigned long fallos_mayores;
    unsigned long paginas_activas;
    unsigned long paginas_inactivas;

    struct top_process_info top_processes[MAX_TOP_PROCESSES];
};

#endif
