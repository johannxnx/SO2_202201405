#ifndef ALERTS_JSON_H
#define ALERTS_JSON_H

#include <stdio.h>
#include <time.h>

typedef struct {
    FILE *file;
    int count;
    time_t timestamp;
} alerts_json_t;

alerts_json_t* alerts_json_begin(const char *path) {
    alerts_json_t *aj = malloc(sizeof(alerts_json_t));
    if (!aj) return NULL;
    
    aj->file = fopen(path, "w");
    if (!aj->file) {
        free(aj);
        return NULL;
    }
    
    aj->count = 0;
    aj->timestamp = time(NULL);
    
    fprintf(aj->file, "{\n");
    fprintf(aj->file, "  \"timestamp\": %ld,\n", aj->timestamp);
    fprintf(aj->file, "  \"alertas\": [\n");
    
    return aj;
}

void alerts_json_add(alerts_json_t *aj, const char *tipo, const char *severidad, 
                     const char *mensaje, const char *archivo) {
    if (!aj || !aj->file) return;
    
    if (aj->count > 0) {
        fprintf(aj->file, ",\n");
    }
    
    fprintf(aj->file, "    {\n");
    fprintf(aj->file, "      \"timestamp\": %ld,\n", time(NULL));
    fprintf(aj->file, "      \"tipo\": \"%s\",\n", tipo);
    fprintf(aj->file, "      \"severidad\": \"%s\",\n", severidad);
    fprintf(aj->file, "      \"mensaje\": \"%s\",\n", mensaje);
    fprintf(aj->file, "      \"archivo\": \"%s\"\n", archivo ? archivo : "");
    fprintf(aj->file, "    }");
    
    aj->count++;
}

void alerts_json_end(alerts_json_t *aj) {
    if (!aj || !aj->file) return;
    
    fprintf(aj->file, "\n  ]\n");
    fprintf(aj->file, "}\n");
    fclose(aj->file);
    free(aj);
}

#endif
