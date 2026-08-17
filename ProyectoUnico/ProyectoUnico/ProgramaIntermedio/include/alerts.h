#ifndef ALERTS_H
#define ALERTS_H

#include <stdio.h>

void generar_alerta(FILE *log, const char *severidad, const char *evento, const char *archivo, const char *detalle);
void generar_alerta_json(FILE *json, const char *severidad, const char *tipo, const char *descripcion, const char *archivo, const char *detalle, int es_ultima);

#endif
