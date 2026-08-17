#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include <stdint.h>

struct process_info {
    int pid;
    char name[16];
    unsigned long long exec_time_ms;
    unsigned long mem_kb;
};

#endif
