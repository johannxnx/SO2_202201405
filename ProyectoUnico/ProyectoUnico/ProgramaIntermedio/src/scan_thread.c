#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>

#include "scan_thread.h"
#include "syscalls_wrapper.h"
#include "file_analyze.h"
#include "blacklist.h"
#include "file_state.h"
#include "alerts.h"
#include "process_scan.h"
#include "quarantine_persistent.h"

void convertir_hash_a_hex(unsigned char hash[], int size, char salida[])
{
    int i;
    for (i = 0; i < size; i++) {
        sprintf(salida + (i * 2), "%02x", hash[i]);
    }
    salida[size * 2] = '\0';
}

/* Estructura para almacenar alertas durante el escaneo */
struct alert_record {
    time_t timestamp;
    char severidad[16];
    char tipo[32];
    char evento[128];
    char archivo[256];
    char detalle[256];
};

#define MAX_ALERTS_PER_SCAN 500
static struct alert_record alertas_generadas[MAX_ALERTS_PER_SCAN];
static int total_alertas = 0;

static int escaneo_habilitado()
{
    FILE *f = fopen("/tmp/daemon_scan_state", "r");
    int enabled = 1;

    if (f == NULL) {
        return 1;
    }

    if (fscanf(f, "%d", &enabled) != 1) {
        enabled = 1;
    }

    fclose(f);
    return enabled ? 1 : 0;
}

void agregar_alerta_pendiente(const char *severidad, const char *tipo, const char *evento, const char *archivo, const char *detalle)
{
    if (total_alertas < MAX_ALERTS_PER_SCAN) {
        alertas_generadas[total_alertas].timestamp = time(NULL);
        strncpy(alertas_generadas[total_alertas].severidad, severidad, sizeof(alertas_generadas[total_alertas].severidad) - 1);
        strncpy(alertas_generadas[total_alertas].tipo, tipo, sizeof(alertas_generadas[total_alertas].tipo) - 1);
        strncpy(alertas_generadas[total_alertas].evento, evento, sizeof(alertas_generadas[total_alertas].evento) - 1);
        strncpy(alertas_generadas[total_alertas].archivo, archivo, sizeof(alertas_generadas[total_alertas].archivo) - 1);
        strncpy(alertas_generadas[total_alertas].detalle, detalle ? detalle : "", sizeof(alertas_generadas[total_alertas].detalle) - 1);
        total_alertas++;
    }
}

void registrar_procesos_sospechosos(FILE *log, FILE *json_proc)
{
    struct process_scan_result resultado_procesos;
    int res;
    int i;
    time_t ahora = time(NULL);

    memset(&resultado_procesos, 0, sizeof(resultado_procesos));

    res = scan_processes(&resultado_procesos);

    if (res != 0) {
        fprintf(log, "  Error al escanear procesos sospechosos\n");
        if (json_proc) {
            fprintf(json_proc, "{\"timestamp\": %ld, \"procesos\": [], \"error\": \"scan_processes failed\"}\n", ahora);
        }
        return;
    }

    if (json_proc) {
        fprintf(json_proc, "{\n");
        fprintf(json_proc, "  \"timestamp\": %ld,\n", ahora);
        fprintf(json_proc, "  \"procesos\": [\n");
    }

    if (resultado_procesos.count > 0) {
        fprintf(log, "  [PROCESOS SOSPECHOSOS]\n");

        for (i = 0; i < resultado_procesos.count; i++) {
            fprintf(log, "    PID: %d\n", resultado_procesos.processes[i].pid);
            fprintf(log, "    Nombre: %s\n", resultado_procesos.processes[i].name);
            fprintf(log, "    VM Size: %lu\n", resultado_procesos.processes[i].vm_size);
            fprintf(log, "    RSS: %lu\n", resultado_procesos.processes[i].rss);

            if (json_proc) {
                fprintf(json_proc, "    {\n");
                fprintf(json_proc, "      \"pid\": %d,\n", resultado_procesos.processes[i].pid);
                fprintf(json_proc, "      \"nombre\": \"%s\",\n", resultado_procesos.processes[i].name);
                fprintf(json_proc, "      \"rss\": %lu,\n", resultado_procesos.processes[i].rss);
                fprintf(json_proc, "      \"vm\": %lu\n", resultado_procesos.processes[i].vm_size);
                fprintf(json_proc, "    }%s\n", (i < resultado_procesos.count - 1) ? "," : "");
            }

            generar_alerta(
                log,
                "MEDIUM",
                "Proceso sospechoso detectado",
                resultado_procesos.processes[i].name,
                "Proceso reportado por sys_scan_processes"
            );
            
            agregar_alerta_pendiente(
                "MEDIUM",
                "proceso",
                "Proceso sospechoso detectado",
                resultado_procesos.processes[i].name,
                "Proceso reportado por sys_scan_processes"
            );
        }
    }

    if (json_proc) {
        fprintf(json_proc, "  ]\n");
        fprintf(json_proc, "}\n");
    }
}


void *hilo_escaneo(void *arg)
{
    DIR *dir;
    struct dirent *entry;
    char ruta[PATH_MAX];
    char hash_hex[65];
    struct file_info info;
    int resultado;

    struct blacklist_entry blacklist[MAX_BLACKLIST_ENTRIES];
    int total_firmas;
    int indice_hash;

    static struct file_state estados[MAX_FILES];
    static int total_estados = 0;
    int indice_estado;
    const char *blacklist_path;
    
    int archivo_idx = 0;

    blacklist_path = getenv("BLACKLIST_PATH");
    if (blacklist_path == NULL || blacklist_path[0] == '\0') {
        blacklist_path = "config/hash_blacklist.json";
    }

    while (1) {
        FILE *log = fopen("logs/daemon.log", "a");
        FILE *json_files = fopen("/tmp/daemon_files.json", "w");
        FILE *json_alerts = fopen("/tmp/daemon_alerts.json", "w");
        FILE *json_proc = fopen("/tmp/daemon_processes.json", "w");
        time_t ahora = time(NULL);

        if (log != NULL) {
            fprintf(log, "[ESCANEO] %s", ctime(&ahora));

            if (!escaneo_habilitado()) {
                fprintf(log, "  Estado: escaneo desactivado por administrador\n");

                if (json_files) {
                    fprintf(json_files, "{\n");
                    fprintf(json_files, "  \"timestamp\": %ld,\n", ahora);
                    fprintf(json_files, "  \"archivos\": []\n");
                    fprintf(json_files, "}\n");
                    fclose(json_files);
                }

                if (json_alerts) {
                    fprintf(json_alerts, "{\n");
                    fprintf(json_alerts, "  \"timestamp\": %ld,\n", ahora);
                    fprintf(json_alerts, "  \"alertas\": []\n");
                    fprintf(json_alerts, "}\n");
                    fclose(json_alerts);
                }

                if (json_proc) {
                    fprintf(json_proc, "{\"timestamp\": %ld, \"procesos\": []}\n", ahora);
                    fclose(json_proc);
                }

                fclose(log);
                total_alertas = 0;
                sleep(7);
                continue;
            }

            total_firmas = cargar_blacklist(blacklist_path, blacklist, MAX_BLACKLIST_ENTRIES);

            if (total_firmas < 0) {
                fprintf(log, "  Error: no se pudo cargar %s\n", blacklist_path);
                fclose(log);
                if (json_files) fclose(json_files);
                if (json_alerts) fclose(json_alerts);
                if (json_proc) fclose(json_proc);
                sleep(7);
                continue;
            }

            dir = opendir("config/monitor");

            if (dir == NULL) {
                fprintf(log, "  Error: no se pudo abrir config/monitor\n");
                fclose(log);
                if (json_files) fclose(json_files);
                if (json_alerts) fclose(json_alerts);
                if (json_proc) fclose(json_proc);
                sleep(7);
                continue;
            }

            /* Iniciar JSON de archivos */
            if (json_files) {
                fprintf(json_files, "{\n");
                fprintf(json_files, "  \"timestamp\": %ld,\n", ahora);
                fprintf(json_files, "  \"archivos\": [\n");
            }

            /* Iniciar JSON de alertas */
            if (json_alerts) {
                fprintf(json_alerts, "{\n");
                fprintf(json_alerts, "  \"timestamp\": %ld,\n", ahora);
                fprintf(json_alerts, "  \"alertas\": [\n");
            }

            archivo_idx = 0;
            
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }

                snprintf(ruta, sizeof(ruta), "config/monitor/%s", entry->d_name);

                memset(&info, 0, sizeof(info));
                resultado = analizar_archivo(ruta, &info);

                if (resultado == 0) {
                    convertir_hash_a_hex(info.hash, SHA256_DIGEST_SIZE, hash_hex);
                    int es_nuevo = 0;
                    int es_modificado = 0;
                    int tiene_malware = 0;

                    fprintf(log, "  Archivo: %s\n", ruta);
                    fprintf(log, "    Size: %lu\n", info.size);
                    fprintf(log, "    Timestamp: %lu\n", info.timestamp);
                    fprintf(log, "    SHA-256: %s\n", hash_hex);

                    indice_estado = buscar_archivo(estados, total_estados, ruta);

                    if (indice_estado < 0) {
                        es_nuevo = 1;
                        fprintf(log, "    Estado archivo: NUEVO\n");
                        generar_alerta(
                            log,
                            "LOW",
                            "Archivo nuevo detectado",
                            ruta,
                            "El archivo no existia en el registro previo"
                        );
                        agregar_alerta_pendiente(
                            "LOW",
                            "archivo",
                            "Archivo nuevo detectado",
                            ruta,
                            "El archivo no existia en el registro previo"
                        );
                    } else {
                        if (strcmp(estados[indice_estado].hash, hash_hex) != 0 ||
                            estados[indice_estado].timestamp != info.timestamp) {
                            es_modificado = 1;
                            fprintf(log, "    Estado archivo: MODIFICADO\n");
                            generar_alerta(
                                log,
                                "MEDIUM",
                                "Archivo modificado",
                                ruta,
                                "Cambio detectado en hash o timestamp"
                            );
                            agregar_alerta_pendiente(
                                "MEDIUM",
                                "archivo",
                                "Archivo modificado",
                                ruta,
                                "Cambio detectado en hash o timestamp"
                            );
                        } else {
                            fprintf(log, "    Estado archivo: SIN CAMBIOS\n");
                        }
                    }

                    indice_hash = buscar_hash_en_blacklist(hash_hex, blacklist, total_firmas);

                    if (indice_hash >= 0) {
                        tiene_malware = 1;
                        fprintf(log, "    [AMENAZA DETECTADA]\n");
                        fprintf(log, "      Nombre: %s\n", blacklist[indice_hash].nombre);
                        fprintf(log, "      Severidad: %s\n", blacklist[indice_hash].severidad);
                        fprintf(log, "      Descripcion: %s\n", blacklist[indice_hash].descripcion);

                        generar_alerta(
                            log,
                            blacklist[indice_hash].severidad,
                            "Hash malicioso detectado",
                            ruta,
                            blacklist[indice_hash].descripcion
                        );
                        
                        agregar_alerta_pendiente(
                            blacklist[indice_hash].severidad,
                            "archivo",
                            "Hash malicioso detectado",
                            ruta,
                            blacklist[indice_hash].descripcion
                        );

                        if (strcmp(blacklist[indice_hash].severidad, "HIGH") == 0) {
                            int res_quarantine = poner_en_cuarentena(ruta);
                            
                            if (res_quarantine == 0) {
                                fprintf(log, "      [ACCIÓN] Archivo puesto en cuarentena exitosamente\n");
                                agregar_a_cuarentena_persistente(ruta, hash_hex, blacklist[indice_hash].severidad);
                            } else {
                                fprintf(log, "      [ERROR] No se pudo poner archivo en cuarentena: code=%d\n", res_quarantine);
                            }
                            
                            simular_panic("Amenaza HIGH detectada por hash blacklist");
                        }

                    } else {
                        fprintf(log, "    Estado malware: limpio\n");
                    }

                    /* Escribir JSON del archivo */
                    if (json_files) {
                        if (archivo_idx > 0) {
                            fprintf(json_files, ",\n");
                        }
                        fprintf(json_files, "    {\n");
                        fprintf(json_files, "      \"ruta\": \"%s\",\n", ruta);
                        fprintf(json_files, "      \"size\": %lu,\n", info.size);
                        fprintf(json_files, "      \"timestamp\": %lu,\n", info.timestamp);
                        fprintf(json_files, "      \"hash\": \"%s\",\n", hash_hex);
                        fprintf(json_files, "      \"estado\": \"%s\",\n", 
                            es_nuevo ? "NUEVO" : (es_modificado ? "MODIFICADO" : "SIN CAMBIOS"));
                        fprintf(json_files, "      \"malware\": {\n");
                        fprintf(json_files, "        \"detectado\": %s", tiene_malware ? "true" : "false");
                        if (tiene_malware) {
                            fprintf(json_files, ",\n");
                            fprintf(json_files, "        \"nombre\": \"%s\",\n", blacklist[indice_hash].nombre);
                            fprintf(json_files, "        \"severidad\": \"%s\",\n", blacklist[indice_hash].severidad);
                            fprintf(json_files, "        \"descripcion\": \"%s\"\n", blacklist[indice_hash].descripcion);
                        } else {
                            fprintf(json_files, "\n");
                        }
                        fprintf(json_files, "      }\n");
                        fprintf(json_files, "    }");
                    }

                    actualizar_estado(estados, &total_estados, ruta, hash_hex, info.timestamp);
                    archivo_idx++;

                } else {
                    fprintf(log, "  Error analizando archivo: %s\n", ruta);
                }
            }

            closedir(dir);

            /* Cerrar JSON de archivos */
            if (json_files) {
                fprintf(json_files, "\n  ]\n");
                fprintf(json_files, "}\n");
                fclose(json_files);
            }

            /* Escribir JSON de alertas con todas las alertas generadas */
            if (json_alerts) {
                for (int i = 0; i < total_alertas; i++) {
                    generar_alerta_json(
                        json_alerts,
                        alertas_generadas[i].severidad,
                        alertas_generadas[i].tipo,
                        alertas_generadas[i].evento,
                        alertas_generadas[i].archivo,
                        alertas_generadas[i].detalle,
                        (i == total_alertas - 1) ? 1 : 0
                    );
                }
                fprintf(json_alerts, "  ]\n");
                fprintf(json_alerts, "}\n");
                fclose(json_alerts);
            }

            /* Generar JSON de cuarentena persistente */
            generar_json_cuarentena();

            registrar_procesos_sospechosos(log, json_proc);

            if (json_proc) {
                fclose(json_proc);
            }

            fclose(log);
            
            /* Resetear contador de alertas para el próximo ciclo */
            total_alertas = 0;
        }

        sleep(7);
    }

    return NULL;
}