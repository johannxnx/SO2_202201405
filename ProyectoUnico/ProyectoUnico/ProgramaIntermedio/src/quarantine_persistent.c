#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "quarantine_persistent.h"

/* Estructura extendida para almacenar más información en cuarentena */
struct quarantine_entry_extended {
    char path[256];
    unsigned long timestamp;
    char hash[65];
    char severidad[16];
};

static struct quarantine_entry_extended quarantine_list[MAX_QUARANTINE_ENTRIES];
static int quarantine_count = 0;
static pthread_mutex_t quarantine_mutex = PTHREAD_MUTEX_INITIALIZER;

void agregar_a_cuarentena_persistente(const char *path, const char *hash, const char *severidad)
{
    pthread_mutex_lock(&quarantine_mutex);

    if (quarantine_count < MAX_QUARANTINE_ENTRIES) {
        strncpy(quarantine_list[quarantine_count].path, path, sizeof(quarantine_list[quarantine_count].path) - 1);
        quarantine_list[quarantine_count].path[sizeof(quarantine_list[quarantine_count].path) - 1] = '\0';
        strncpy(quarantine_list[quarantine_count].hash, hash, sizeof(quarantine_list[quarantine_count].hash) - 1);
        quarantine_list[quarantine_count].hash[sizeof(quarantine_list[quarantine_count].hash) - 1] = '\0';
        strncpy(quarantine_list[quarantine_count].severidad, severidad, sizeof(quarantine_list[quarantine_count].severidad) - 1);
        quarantine_list[quarantine_count].severidad[sizeof(quarantine_list[quarantine_count].severidad) - 1] = '\0';
        quarantine_list[quarantine_count].timestamp = time(NULL);
        quarantine_count++;
    }

    pthread_mutex_unlock(&quarantine_mutex);
}

void eliminar_de_cuarentena_persistente(const char *path)
{
    if (path == NULL) {
        return;
    }

    pthread_mutex_lock(&quarantine_mutex);

    for (int i = 0; i < quarantine_count; i++) {
        if (strcmp(quarantine_list[i].path, path) == 0) {
            for (int j = i; j < quarantine_count - 1; j++) {
                quarantine_list[j] = quarantine_list[j + 1];
            }
            quarantine_count--;
            break;
        }
    }

    pthread_mutex_unlock(&quarantine_mutex);
}

void generar_json_cuarentena()
{
    FILE *json = fopen("/tmp/daemon_quarantine.json", "w");
    if (json == NULL) return;

    pthread_mutex_lock(&quarantine_mutex);

    fprintf(json, "{\n");
    fprintf(json, "  \"timestamp\": %ld,\n", time(NULL));
    fprintf(json, "  \"total\": %d,\n", quarantine_count);
    fprintf(json, "  \"archivos\": [\n");

    for (int i = 0; i < quarantine_count; i++) {
        fprintf(json, "    {\n");
        fprintf(json, "      \"ruta\": \"%s\",\n", quarantine_list[i].path);
        fprintf(json, "      \"timestamp\": %ld,\n", quarantine_list[i].timestamp);
        fprintf(json, "      \"timestamp_cuarentena\": %ld,\n", quarantine_list[i].timestamp);
        fprintf(json, "      \"hash\": \"%s\",\n", quarantine_list[i].hash);
        fprintf(json, "      \"severidad\": \"%s\"\n", quarantine_list[i].severidad);
        fprintf(json, "    }%s\n", (i < quarantine_count - 1) ? "," : "");
    }

    fprintf(json, "  ]\n");
    fprintf(json, "}\n");

    pthread_mutex_unlock(&quarantine_mutex);
    fclose(json);
}

void cargar_cuarentena_desde_archivo()
{
    FILE *archivo = fopen(QUARANTINE_FILE, "r");
    if (archivo == NULL) return;

    /* Crear archivo si no existe */
    archivo = fopen(QUARANTINE_FILE, "w");
    if (archivo) {
        fprintf(archivo, "{\n  \"archivos\": []\n}\n");
        fclose(archivo);
    }
}
