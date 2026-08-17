#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/swap.h>
#include <linux/sched/signal.h>
#include <linux/uaccess.h>
#include <linux/sysinfo.h>
#include <linux/system_monitor.h>
#include <linux/vmstat.h>

SYSCALL_DEFINE1(get_system_monitor, struct system_monitor_info __user *, info)
{
    struct system_monitor_info kinfo;
    struct sysinfo si;
    struct task_struct *task;
    unsigned long total_ram_kb, free_ram_kb, buffer_ram_kb, total_swap_kb, free_swap_kb;

    int i;

    memset(&kinfo, 0, sizeof(kinfo));

    si_meminfo(&si);
    si_swapinfo(&si);

    total_ram_kb   = (si.totalram * si.mem_unit) / 1024;
    free_ram_kb    = (si.freeram * si.mem_unit) / 1024;
    buffer_ram_kb  = (si.bufferram * si.mem_unit) / 1024;
    total_swap_kb  = (si.totalswap * si.mem_unit) / 1024;
    free_swap_kb   = (si.freeswap * si.mem_unit) / 1024;

    kinfo.memoria_usada  = total_ram_kb - free_ram_kb;
    kinfo.memoria_libre  = free_ram_kb;
    kinfo.memoria_cache  = buffer_ram_kb;
    kinfo.swap_usada     = total_swap_kb - free_swap_kb;

    kinfo.fallos_menores = 0;
    kinfo.fallos_mayores = 0;
    kinfo.paginas_activas = global_node_page_state(NR_ACTIVE_FILE) +
                        global_node_page_state(NR_ACTIVE_ANON);
    kinfo.paginas_inactivas = global_node_page_state(NR_INACTIVE_FILE) +
                          global_node_page_state(NR_INACTIVE_ANON);

    kinfo.num_top_processes = 0;

    rcu_read_lock();

    for_each_process(task) {
        unsigned long mem_kb = 0;
        unsigned long mem_percent = 0;
        int pos = -1;

        if (task->mm)
            mem_kb = (get_mm_rss(task->mm) * PAGE_SIZE) / 1024;

        kinfo.fallos_menores += task->min_flt;
        kinfo.fallos_mayores += task->maj_flt;

        if (total_ram_kb > 0)
            mem_percent = (mem_kb * 100) / total_ram_kb;

        if (kinfo.num_top_processes < TOP_PROCESSES) {
            pos = kinfo.num_top_processes;
            kinfo.num_top_processes++;
        } else {
            int min_idx = 0;
            for (i = 1; i < TOP_PROCESSES; i++) {
                if (kinfo.procesos_top[i].mem_kb < kinfo.procesos_top[min_idx].mem_kb)
                    min_idx = i;
            }

            if (mem_kb > kinfo.procesos_top[min_idx].mem_kb)
                pos = min_idx;
        }

        if (pos >= 0) {
            kinfo.procesos_top[pos].pid = task->pid;
            strncpy(kinfo.procesos_top[pos].name, task->comm, PROC_NAME_LEN - 1);
            kinfo.procesos_top[pos].name[PROC_NAME_LEN - 1] = '\0';
            kinfo.procesos_top[pos].mem_kb = mem_kb;
            kinfo.procesos_top[pos].mem_percent = mem_percent;
        }
    }

    rcu_read_unlock();

    if (copy_to_user(info, &kinfo, sizeof(kinfo)))
        return -EFAULT;

    printk(KERN_INFO "sys_get_system_monitor: OK\n");
    return 0;
}
