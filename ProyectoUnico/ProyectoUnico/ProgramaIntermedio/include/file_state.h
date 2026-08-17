#ifndef FILE_STATE_H
#define FILE_STATE_H

#define MAX_FILES 100
#define PATH_SIZE 256
#define HASH_SIZE 65

struct file_state {
    char path[PATH_SIZE];
    char hash[HASH_SIZE];
    unsigned long timestamp;
};

int buscar_archivo(struct file_state estados[], int total, const char *path);
void actualizar_estado(struct file_state estados[], int *total, const char *path, const char *hash, unsigned long timestamp);

#endif
