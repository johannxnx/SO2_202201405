#include <stdio.h>
#include <time.h>
#include "alerts.h"

void generar_alerta(FILE *log, const char *severidad, const char *evento, const char *archivo, const char *detalle)
{
    time_t ahora = time(NULL);

    fprintf(log, "    [ALERTA]\n");
    fprintf(log, "      Fecha: %s", ctime(&ahora));
    fprintf(log, "      Severidad: %s\n", severidad);
    fprintf(log, "      Evento: %s\n", evento);
    fprintf(log, "      Archivo: %s\n", archivo);

    if (detalle != NULL) {
        fprintf(log, "      Detalle: %s\n", detalle);
    }
}

void generar_alerta_json(FILE *json, const char *severidad, const char *tipo, const char *descripcion, const char *archivo, const char *detalle, int es_ultima)
{
    time_t ahora = time(NULL);

    if (json == NULL) return;

    fprintf(json, "    {\n");
    fprintf(json, "      \"timestamp\": %ld,\n", ahora);
    fprintf(json, "      \"severidad\": \"%s\",\n", severidad);
    fprintf(json, "      \"tipo\": \"%s\",\n", tipo);
    fprintf(json, "      \"descripcion\": \"%s\",\n", descripcion);
    fprintf(json, "      \"mensaje\": \"%s\",\n", detalle ? detalle : "");
    fprintf(json, "      \"archivo\": \"%s\"\n", archivo);
    fprintf(json, "    }%s\n", es_ultima ? "" : ",");
}
