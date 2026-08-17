#include <string.h>
#include "file_state.h"

int buscar_archivo(struct file_state estados[], int total, const char *path)
{
    int i;
    for (i = 0; i < total; i++) {
        if (strcmp(estados[i].path, path) == 0) {
            return i;
        }
    }
    return -1;
}

void actualizar_estado(struct file_state estados[], int *total, const char *path, const char *hash, unsigned long timestamp)
{
    int idx = buscar_archivo(estados, *total, path);

    if (idx >= 0) {
        strcpy(estados[idx].hash, hash);
        estados[idx].timestamp = timestamp;
    } else {
        strcpy(estados[*total].path, path);
        strcpy(estados[*total].hash, hash);
        estados[*total].timestamp = timestamp;
        (*total)++;
    }
}
