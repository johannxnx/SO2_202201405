#ifndef QUARANTINE_PERSISTENT_H
#define QUARANTINE_PERSISTENT_H

#include "syscalls_wrapper.h"

#define QUARANTINE_FILE "config/quarantine_list.json"
#define MAX_QUARANTINE_ENTRIES 1000

void generar_json_cuarentena();
void agregar_a_cuarentena_persistente(const char *path, const char *hash, const char *severidad);
void eliminar_de_cuarentena_persistente(const char *path);
void cargar_cuarentena_desde_archivo();

#endif
