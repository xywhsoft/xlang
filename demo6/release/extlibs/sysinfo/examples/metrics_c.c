#include "../sysinfo_native.c"
#include <inttypes.h>
#include <stdio.h>

int main(void)
{
    int64_t available = xsysinfo_mem_available();
    int64_t usage = xsysinfo_mem_usage_pct();
    int64_t uptime = xsysinfo_uptime();
    if (available < 0 || usage < 0 || usage > 100 || uptime < 0) return 1;
    printf("available: %" PRId64 "\nusage: %" PRId64
           "\nuptime: %" PRId64 "\n", available, usage, uptime);
    return 0;
}
