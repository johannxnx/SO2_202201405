#ifndef BLACKLIST_H
#define BLACKLIST_H

#define MAX_BLACKLIST_ENTRIES 20
#define HASH_HEX_SIZE 65
#define NAME_SIZE 100
#define SEVERITY_SIZE 20
#define DESCRIPTION_SIZE 256

struct blacklist_entry {
    char hash[HASH_HEX_SIZE];
    char nombre[NAME_SIZE];
    char severidad[SEVERITY_SIZE];
    char descripcion[DESCRIPTION_SIZE];
};

int cargar_blacklist(const char *ruta, struct blacklist_entry entries[], int max_entries);
int buscar_hash_en_blacklist(const char *hash_hex, struct blacklist_entry entries[], int count);

#endif
