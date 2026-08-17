#ifndef PROCESS_SCAN_H
#define PROCESS_SCAN_H

#define MAX_SUSPICIOUS_PROCESSES 20
#define PROCESS_NAME_LEN 64

struct suspicious_process_info {
    int pid;
    char name[PROCESS_NAME_LEN];
    unsigned long vm_size;
    unsigned long rss;
};

struct process_scan_result {
    int count;
    struct suspicious_process_info processes[MAX_SUSPICIOUS_PROCESSES];
};

#endif
