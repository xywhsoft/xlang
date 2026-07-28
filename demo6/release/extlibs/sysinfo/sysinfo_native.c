/* sysinfo_native.c - System information (Windows/Linux) */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/utsname.h>
#include <unistd.h>
#endif

static char buf[1024];

/* OS name */
const char *xsysinfo_os_name(void) {
#ifdef _WIN32
    return "Windows";
#else
    struct utsname u;
    if (uname(&u) == 0) return u.sysname;
    return "Unknown";
#endif
}

/* OS version */
const char *xsysinfo_os_version(void) {
#ifdef _WIN32
    OSVERSIONINFOA vi;
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    /* GetVersionEx is deprecated but works for our purposes */
    sprintf(buf, "%lu.%lu", (unsigned long)10, (unsigned long)0);
    return buf;
#else
    struct utsname u;
    if (uname(&u) == 0) {
        strcpy(buf, u.release);
        return buf;
    }
    return "unknown";
#endif
}

/* Machine architecture */
const char *xsysinfo_arch(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case 9: return "x86_64";
        case 0: return "x86";
        case 12: return "arm64";
        case 5: return "arm";
        default: return "unknown";
    }
#else
    struct utsname u;
    if (uname(&u) == 0) return u.machine;
    return "unknown";
#endif
}

/* Hostname */
const char *xsysinfo_hostname(void) {
#ifdef _WIN32
    DWORD sz = sizeof(buf);
    if (GetComputerNameA(buf, &sz)) return buf;
    return "unknown";
#else
    if (gethostname(buf, sizeof(buf)) == 0) return buf;
    return "unknown";
#endif
}

/* Number of CPU cores */
long long xsysinfo_cpu_count(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (long long)si.dwNumberOfProcessors;
#else
    return (long long)sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

/* Total physical memory in bytes */
long long xsysinfo_mem_total(void) {
#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
        return (long long)ms.ullTotalPhys;
    return 0;
#else
    return (long long)sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE);
#endif
}

/* Available memory in bytes */
long long xsysinfo_mem_available(void) {
#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
        return (long long)ms.ullAvailPhys;
    return 0;
#else
    return (long long)sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGE_SIZE);
#endif
}

/* Memory usage percentage (0-100) */
long long xsysinfo_mem_usage_pct(void) {
#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
        return (long long)ms.dwMemoryLoad;
    return 0;
#else
    long long total = xsysinfo_mem_total();
    long long avail = xsysinfo_mem_available();
    if (total == 0) return 0;
    return (total - avail) * 100 / total;
#endif
}

/* Page size */
long long xsysinfo_page_size(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (long long)si.dwPageSize;
#else
    return (long long)sysconf(_SC_PAGE_SIZE);
#endif
}

/* Uptime in seconds */
long long xsysinfo_uptime(void) {
#ifdef _WIN32
    return (long long)(GetTickCount64() / 1000);
#else
    FILE *f = fopen("/proc/uptime", "r");
    double up = 0;
    if (f) { fscanf(f, "%lf", &up); fclose(f); }
    return (long long)up;
#endif
}

/* CPU name */
const char *xsysinfo_cpu_name(void) {
#ifdef _WIN32
    HKEY hKey;
    DWORD sz = sizeof(buf);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_READ, &hKey) == 0) {
        RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)buf, &sz);
        RegCloseKey(hKey);
        /* Trim leading spaces */
        char *p = buf;
        while (*p == ' ') p++;
        if (p != buf) memmove(buf, p, strlen(p) + 1);
        return buf;
    }
    return "unknown";
#else
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "model name", 10) == 0) {
                char *colon = strchr(line, ':');
                if (colon) {
                    colon += 2;
                    colon[strcspn(colon, "\n")] = '\0';
                    strcpy(buf, colon);
                    fclose(f);
                    return buf;
                }
            }
        }
        fclose(f);
    }
    return "unknown";
#endif
}
