/* procps_native.c - Process information (simplified, no tlhelp32) */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#endif

static char buf[4096];

/* Get current process ID */
long long xprocps_getpid(void) {
    return (long long)getpid();
}

/* Get parent process ID */
long long xprocps_getppid(void) {
#ifdef _WIN32
    /* Simplified: return 0 on Windows without tlhelp32 */
    return 0;
#else
    return (long long)getppid();
#endif
}

/* Check if a process exists */
long long xprocps_exists(long long pid) {
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return 0;
    CloseHandle(h);
    return 1;
#else
    return kill((pid_t)pid, 0) == 0 ? 1 : 0;
#endif
}

/* Get process name by PID */
const char *xprocps_name(long long pid) {
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return "unknown";
    DWORD sz = sizeof(buf);
    if (QueryFullProcessImageNameA(h, 0, buf, &sz)) {
        CloseHandle(h);
        /* Extract filename from path */
        char *slash = strrchr(buf, '\\');
        if (slash) return slash + 1;
        return buf;
    }
    CloseHandle(h);
    return "unknown";
#else
    char path[64];
    sprintf(path, "/proc/%lld/comm", pid);
    FILE *f = fopen(path, "r");
    if (f) {
        if (fgets(buf, sizeof(buf), f)) {
            buf[strcspn(buf, "\n")] = '\0';
            fclose(f);
            return buf;
        }
        fclose(f);
    }
    return "unknown";
#endif
}

/* List PIDs by scanning range (simplified for Windows) */
const char *xprocps_list_pids(void) {
    int pos = 0;
    buf[0] = '\0';
#ifdef _WIN32
    /* Scan PID range 0-65536 for existing processes */
    long long pid;
    for (pid = 0; pid < 65536 && pos < 4000; pid++) {
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
        if (h) {
            if (pos > 0) buf[pos++] = ',';
            pos += sprintf(buf + pos, "%lld", pid);
            CloseHandle(h);
        }
    }
#else
    DIR *d = opendir("/proc");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] >= '0' && ent->d_name[0] <= '9') {
                if (pos > 0) buf[pos++] = ',';
                pos += sprintf(buf + pos, "%s", ent->d_name);
                if (pos > 4000) break;
            }
        }
        closedir(d);
    }
#endif
    buf[pos] = '\0';
    return buf;
}

/* Count running processes */
long long xprocps_count(void) {
    long long count = 0;
#ifdef _WIN32
    long long pid;
    for (pid = 0; pid < 65536; pid++) {
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
        if (h) { count++; CloseHandle(h); }
    }
#else
    DIR *d = opendir("/proc");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] >= '0' && ent->d_name[0] <= '9')
                count++;
        }
        closedir(d);
    }
#endif
    return count;
}

/* Get memory usage of current process in KB */
long long xprocps_mem_kb(long long pid) {
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return -1;
    /* Use GetProcessMemoryInfo if available, otherwise return approximate */
    CloseHandle(h);
    return 0; /* simplified */
#else
    char path[64];
    sprintf(path, "/proc/%lld/statm", pid);
    FILE *f = fopen(path, "r");
    if (f) {
        long long pages = 0;
        fscanf(f, "%lld", &pages);
        fclose(f);
        return pages * 4;
    }
    return -1;
#endif
}
