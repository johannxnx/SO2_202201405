#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/mm.h>
#include <linux/uaccess.h>

/* Estructura con la información del proceso */
struct process_info {
    pid_t pid;
    char name[16];
    unsigned long long exec_time_ms;
    unsigned long mem_kb;
};

SYSCALL_DEFINE2(get_process_info, pid_t, pid, struct process_info __user *, info)
{
    struct task_struct *task;
    struct process_info kinfo;

    printk(KERN_INFO "sys_get_process_info: llamada con PID=%d\n", pid);

    rcu_read_lock();
    task = pid_task(find_vpid(pid), PIDTYPE_PID);

    if (!task) {
        rcu_read_unlock();
        printk(KERN_WARNING "sys_get_process_info: PID %d no encontrado\n", pid);
        return -ESRCH;
    }

    kinfo.pid       = task->pid;
    strncpy(kinfo.name, task->comm, sizeof(kinfo.name) - 1);
    kinfo.name[sizeof(kinfo.name) - 1] = '\0';
    kinfo.exec_time_ms = (ktime_get_ns() - task->start_time) / 1000000ULL;
    kinfo.mem_kb    = task->mm ? (task->mm->total_vm * PAGE_SIZE) / 1024 : 0;

    rcu_read_unlock();

    if (copy_to_user(info, &kinfo, sizeof(struct process_info)))
        return -EFAULT;

    printk(KERN_INFO "sys_get_process_info: OK PID=%d name=%s\n",
           kinfo.pid, kinfo.name);
    return 0;
}