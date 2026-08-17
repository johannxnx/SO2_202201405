#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include <sys/syscall.h>
#include "syscalls_wrapper.h"

#define QUARANTINE_STORAGE_DIR "config/quarantine_storage"

static int ensure_quarantine_directory(void)
{
    struct stat st;

    if (stat(QUARANTINE_STORAGE_DIR, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }

    return mkdir(QUARANTINE_STORAGE_DIR, 0755);
}

static void sanitize_path(const char *path, char *output, size_t output_size)
{
    size_t index = 0;

    if (output_size == 0) {
        return;
    }

    for (size_t i = 0; path != NULL && path[i] != '\0' && index + 1 < output_size; i++) {
        char current = path[i];
        if ((current >= 'a' && current <= 'z') ||
            (current >= 'A' && current <= 'Z') ||
            (current >= '0' && current <= '9') ||
            current == '.' || current == '-' || current == '_') {
            output[index++] = current;
        } else {
            output[index++] = '_';
        }
    }

    output[index] = '\0';
}

static void build_quarantine_path(const char *path, char *output, size_t output_size)
{
    char sanitized[256];
    sanitize_path(path, sanitized, sizeof(sanitized));
    snprintf(output, output_size, "%s/%s_%lu_%d", QUARANTINE_STORAGE_DIR, sanitized, (unsigned long)time(NULL), getpid());
}

static int find_quarantine_file_for_original(const char *original_path, char *output, size_t output_size)
{
    DIR *dir;
    struct dirent *entry;
    char sanitized[256];
    char prefix[300];
    char best_path[PATH_MAX] = {0};
    time_t best_mtime = 0;

    if (original_path == NULL || output == NULL || output_size == 0) {
        return -1;
    }

    sanitize_path(original_path, sanitized, sizeof(sanitized));
    snprintf(prefix, sizeof(prefix), "%s_", sanitized);

    dir = opendir(QUARANTINE_STORAGE_DIR);
    if (!dir) {
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        if (strncmp(entry->d_name, prefix, strlen(prefix)) != 0) {
            continue;
        }

        char candidate[PATH_MAX];
        struct stat st;
        snprintf(candidate, sizeof(candidate), "%s/%s", QUARANTINE_STORAGE_DIR, entry->d_name);

        if (stat(candidate, &st) != 0) {
            continue;
        }

        if (best_path[0] == '\0' || st.st_mtime >= best_mtime) {
            strncpy(best_path, candidate, sizeof(best_path) - 1);
            best_path[sizeof(best_path) - 1] = '\0';
            best_mtime = st.st_mtime;
        }
    }

    closedir(dir);

    if (best_path[0] == '\0') {
        return -1;
    }

    strncpy(output, best_path, output_size - 1);
    output[output_size - 1] = '\0';

    return 0;
}

#define __NR_get_process_info 463
#define __NR_get_system_monitor 464
#define __NR_file_analize 465
#define __NR_scan_processes 466
#define __NR_quarantine_file 467
#define __NR_restore_file 468
#define __NR_get_quarantine_list 469
#define __NR_simulate_panic 470

int obtener_metricas_sistema(struct system_monitor_info *info)
{
    return syscall(__NR_get_system_monitor, info);
}

int analizar_archivo(const char *path, struct file_info *info)
{
    return syscall(__NR_file_analize, path, info);
}

int simular_panic(const char *msg)
{
    return syscall(__NR_simulate_panic, msg);
}

int scan_processes(struct process_scan_result *result)
{
    return syscall(__NR_scan_processes, result);
}

int obtener_info_proceso(int pid, struct process_info *info)
{
    return syscall(__NR_get_process_info, pid, info);
}

int poner_en_cuarentena(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    if (ensure_quarantine_directory() != 0) {
        return -1;
    }

    if (access(path, F_OK) != 0) {
        return -1;
    }

    char quarantine_path[512];
    build_quarantine_path(path, quarantine_path, sizeof(quarantine_path));

    if (rename(path, quarantine_path) != 0) {
        return -1;
    }

    return 0;
}

int restaurar_archivo(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    char quarantine_path[PATH_MAX];
    if (find_quarantine_file_for_original(path, quarantine_path, sizeof(quarantine_path)) != 0) {
        return -1;
    }

    if (rename(quarantine_path, path) != 0) {
        return -1;
    }

    return 0;
}

int obtener_lista_cuarentena(struct quarantine_list *list)
{
    if (list == NULL) {
        return -1;
    }

    list->count = 0;

    return 0;
}
