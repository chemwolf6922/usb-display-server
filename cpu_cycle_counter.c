#define _GNU_SOURCE

#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include "cpu_cycle_counter.h"

int cpu_cycle_counter_open()
{
    /** @link{https://stackoverflow.com/questions/13772567/how-to-get-the-cpu-cycle-count-in-x86-64-from-c/64898073#64898073} */
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    return syscall(SYS_perf_event_open, &pe, 0, -1, -1, 0);
}

int cpu_cycle_counter_reset(int fd)
{
    if(fd < 0)
        return -1;
    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    return 0;
}   

long long cpu_cycle_counter_get_result(int fd)
{
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    long long count;
    if(read(fd, &count, sizeof(long long)) == sizeof(long long))
        return count;
    else
        return -1;
}
