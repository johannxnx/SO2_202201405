#include <stdio.h>
#include <string.h>
#include "blacklist.h"

int cargar_blacklist(const char *ruta, struct blacklist_entry entries[], int max_entries)
{
    FILE *file;
    char linea[512];
    int count = 0;

    file = fopen(ruta, "r");
    if (file == NULL) {
        return -1;
    }

    while (fgets(linea, sizeof(linea), file) != NULL && count < max_entries) {
        if (strstr(linea, "\"hash\"")) {
            sscanf(linea, " %*[^:]: \"%64[^\"]\"", entries[count].hash);
        }

        if (strstr(linea, "\"nombre\"")) {
            sscanf(linea, " %*[^:]: \"%99[^\"]\"", entries[count].nombre);
        }

        if (strstr(linea, "\"severidad\"")) {
            sscanf(linea, " %*[^:]: \"%19[^\"]\"", entries[count].severidad);
        }

        if (strstr(linea, "\"descripcion\"")) {
            sscanf(linea, " %*[^:]: \"%255[^\"]\"", entries[count].descripcion);
            count++;
        }
    }

    fclose(file);
    return count;
}

int buscar_hash_en_blacklist(const char *hash_hex, struct blacklist_entry entries[], int count)
{
    int i;

    for (i = 0; i < count; i++) {
        if (strcmp(hash_hex, entries[i].hash) == 0) {
            return i;
        }
    }

    return -1;
}
