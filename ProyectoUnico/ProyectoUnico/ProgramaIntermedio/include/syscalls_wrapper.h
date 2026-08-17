#ifndef SYSCALLS_WRAPPER_H
#define SYSCALLS_WRAPPER_H

#include "system_monitor.h"
#include "file_analyze.h"
#include "process_scan.h"
#include "process_info.h"

/* Estructura para cuarentena */
struct quarantine_entry {
    char path[256];
    unsigned long timestamp;
};

struct quarantine_list {
    int count;
    struct quarantine_entry files[100];
};

/* Syscalls principales */
int obtener_metricas_sistema(struct system_monitor_info *info);
int analizar_archivo(const char *path, struct file_info *info);
int simular_panic(const char *msg);
int scan_processes(struct process_scan_result *result);
int obtener_info_proceso(int pid, struct process_info *info);

/* Syscalls de cuarentena */
int poner_en_cuarentena(const char *path);
int restaurar_archivo(const char *path);
int obtener_lista_cuarentena(struct quarantine_list *list);

#endif
