#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

#define __NR_get_system_monitor 464
#define TOP_PROCESSES 5
#define PROC_NAME_LEN 64
#define OUTPUT_FILE "/tmp/system_monitor.json"

struct top_process_info {
    int pid;
    char name[PROC_NAME_LEN];
    unsigned long mem_kb;
    unsigned long mem_percent;
};

struct system_monitor_info {
    unsigned long memoria_usada;
    unsigned long memoria_libre;
    unsigned long memoria_cache;
    unsigned long swap_usada;
    unsigned long fallos_menores;
    unsigned long fallos_mayores;
    unsigned long paginas_activas;
    unsigned long paginas_inactivas;

    int num_top_processes;
    struct top_process_info procesos_top[TOP_PROCESSES];
};

static int write_json_file(const char *path, const struct system_monitor_info *info) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    time_t now = time(NULL);

    fprintf(fp, "{\n");
    fprintf(fp, "  \"timestamp\": %ld,\n", now);
    fprintf(fp, "  \"memoria_usada\": %lu,\n", info->memoria_usada);
    fprintf(fp, "  \"memoria_libre\": %lu,\n", info->memoria_libre);
    fprintf(fp, "  \"memoria_cache\": %lu,\n", info->memoria_cache);
    fprintf(fp, "  \"swap_usada\": %lu,\n", info->swap_usada);
    fprintf(fp, "  \"fallos_menores\": %lu,\n", info->fallos_menores);
    fprintf(fp, "  \"fallos_mayores\": %lu,\n", info->fallos_mayores);
    fprintf(fp, "  \"paginas_activas\": %lu,\n", info->paginas_activas);
    fprintf(fp, "  \"paginas_inactivas\": %lu,\n", info->paginas_inactivas);
    fprintf(fp, "  \"procesos_top\": [\n");

    for (int i = 0; i < info->num_top_processes; i++) {
        fprintf(fp,
                "    {\"pid\": %d, \"nombre\": \"%s\", \"mem_kb\": %lu, \"mem_percent\": %lu}%s\n",
                info->procesos_top[i].pid,
                info->procesos_top[i].name,
                info->procesos_top[i].mem_kb,
                info->procesos_top[i].mem_percent,
                (i == info->num_top_processes - 1) ? "" : ",");
    }

    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");

    fclose(fp);
    return 0;
}

static int fetch_system_monitor(struct system_monitor_info *info) {
    memset(info, 0, sizeof(*info));

    long ret = syscall(__NR_get_system_monitor, info);
    if (ret < 0) {
        errno = -ret;
        perror("syscall get_system_monitor");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    struct system_monitor_info info;
    int interval = 3;

    if (argc > 1) {
        interval = atoi(argv[1]);
        if (interval <= 0) {
            fprintf(stderr, "Intervalo inválido. Usa un entero mayor que 0.\n");
            return 1;
        }
    }

    printf("Monitor iniciado. Escribiendo JSON en %s cada %d segundos...\n",
           OUTPUT_FILE, interval);

    while (1) {
        if (fetch_system_monitor(&info) == 0) {
            if (write_json_file(OUTPUT_FILE, &info) == 0) {
                printf("JSON actualizado correctamente.\n");
            }
        }

        sleep(interval);
    }

    return 0;
}