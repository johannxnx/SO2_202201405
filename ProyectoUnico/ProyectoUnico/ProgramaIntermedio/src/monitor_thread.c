#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "monitor_thread.h"
#include "syscalls_wrapper.h"
#include "system_monitor.h"

void *hilo_monitoreo(void *arg)
{
    struct system_monitor_info info;
    int resultado;
    
    // Track previous faults to calculate deltas
    static unsigned long prev_fallos_menores = 0;
    static unsigned long prev_fallos_mayores = 0;
    static int first_iteration = 1;

    while (1) {
        memset(&info, 0, sizeof(info));

        resultado = obtener_metricas_sistema(&info);

        FILE *log = fopen("logs/daemon.log", "a");
        FILE *json = fopen("/tmp/daemon_monitor.json", "w");

        if (log != NULL) {
            time_t ahora = time(NULL);

            if (resultado == 0) {
                unsigned long delta_menores = (first_iteration) ? 0 : (info.fallos_menores - prev_fallos_menores);
                unsigned long delta_mayores = (first_iteration) ? 0 : (info.fallos_mayores - prev_fallos_mayores);
                
                fprintf(log, "[MONITOREO] %s", ctime(&ahora));
                fprintf(log, "  Memoria usada: %lu\n", info.memoria_usada);
                fprintf(log, "  Memoria libre: %lu\n", info.memoria_libre);
                fprintf(log, "  Memoria cache: %lu\n", info.memoria_cache);
                fprintf(log, "  Swap usada: %lu\n", info.swap_usada);
                fprintf(log, "  Fallos menores: %lu (delta: %lu)\n", info.fallos_menores, delta_menores);
                fprintf(log, "  Fallos mayores: %lu (delta: %lu)\n", info.fallos_mayores, delta_mayores);
                fprintf(log, "  Paginas activas: %lu\n", info.paginas_activas);
                fprintf(log, "  Paginas inactivas: %lu\n", info.paginas_inactivas);
            } else {
                fprintf(log, "[MONITOREO] Error al llamar get_system_monitor: %d\n", resultado);
            }

            fclose(log);
        }

        if (json != NULL) {
            time_t ahora = time(NULL);
            unsigned long delta_menores = (first_iteration) ? 0 : (info.fallos_menores - prev_fallos_menores);
            unsigned long delta_mayores = (first_iteration) ? 0 : (info.fallos_mayores - prev_fallos_mayores);
            
            // Each page is 4KB, convert to bytes then to GB
            double paginas_activas_gb = (info.paginas_activas * 4.0) / (1024.0 * 1024.0 / 1024.0);  // 4KB * pages / (1M/1G)
            double paginas_inactivas_gb = (info.paginas_inactivas * 4.0) / (1024.0 * 1024.0 / 1024.0);
            
            fprintf(json, "{\n");
            fprintf(json, "  \"timestamp\": %ld,\n", ahora);
            fprintf(json, "  \"memoria\": {\n");
            fprintf(json, "    \"usada\": %lu,\n", info.memoria_usada);
            fprintf(json, "    \"libre\": %lu,\n", info.memoria_libre);
            fprintf(json, "    \"cache\": %lu,\n", info.memoria_cache);
            fprintf(json, "    \"swap\": %lu\n", info.swap_usada);
            fprintf(json, "  },\n");
            fprintf(json, "  \"fallos\": {\n");
            fprintf(json, "    \"menores\": %lu,\n", info.fallos_menores);
            fprintf(json, "    \"mayores\": %lu,\n", info.fallos_mayores);
            fprintf(json, "    \"delta_menores\": %lu,\n", delta_menores);
            fprintf(json, "    \"delta_mayores\": %lu\n", delta_mayores);
            fprintf(json, "  },\n");
            fprintf(json, "  \"paginas\": {\n");
            fprintf(json, "    \"activas\": %lu,\n", info.paginas_activas);
            fprintf(json, "    \"inactivas\": %lu,\n", info.paginas_inactivas);
            fprintf(json, "    \"activas_gb\": %.2f,\n", paginas_activas_gb);
            fprintf(json, "    \"inactivas_gb\": %.2f\n", paginas_inactivas_gb);
            fprintf(json, "  }\n");
            fprintf(json, "}\n");
            
            fclose(json);
        }

        // Update previous values for next iteration
        prev_fallos_menores = info.fallos_menores;
        prev_fallos_mayores = info.fallos_mayores;
        first_iteration = 0;

        sleep(5);
    }

    return NULL;
}
