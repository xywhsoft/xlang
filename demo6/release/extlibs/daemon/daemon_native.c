/* daemon_native.c - Daemon/service utilities (PID file, detach, status) */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#endif

typedef struct {
    int daemonized;
    int running;
    long long pid;
    char pidfile[512];
    char name[256];
} DaemonState;

static char err_buf[256];
static char info_buf[512];

const char *xdaemon_error(void) { return err_buf; }

void *xdaemon_create(const char *name) {
    DaemonState *ds = (DaemonState *)calloc(1, sizeof(DaemonState));
    strncpy(ds->name, name, 255);
    ds->pid = (long long)getpid();
    ds->running = 1;
    ds->daemonized = 0;
    return ds;
}

void xdaemon_destroy(void *handle) {
    DaemonState *ds = (DaemonState *)handle;
    if (!ds) return;
    /* Remove pidfile if set */
    if (ds->pidfile[0]) remove(ds->pidfile);
    free(ds);
}

/* Detach from console (Windows: FreeConsole, Unix: fork+setsid) */
long long xdaemon_detach(void *handle) {
    DaemonState *ds = (DaemonState *)handle;
    if (!ds) return 0;
#ifdef _WIN32
    FreeConsole();
    ds->daemonized = 1;
    return 1;
#else
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid > 0) _exit(0); /* parent exits */
    setsid();
    ds->daemonized = 1;
    ds->pid = (long long)getpid();
    return 1;
#endif
}

/* Write PID file */
long long xdaemon_write_pidfile(void *handle, const char *path) {
    DaemonState *ds = (DaemonState *)handle;
    if (!ds) return 0;
    FILE *f = fopen(path, "w");
    if (!f) {
        sprintf(err_buf, "daemon: cannot write pidfile '%s'", path);
        return 0;
    }
    fprintf(f, "%lld\n", ds->pid);
    fclose(f);
    strncpy(ds->pidfile, path, 511);
    return 1;
}

/* Read PID from file, returns -1 if not found */
long long xdaemon_read_pidfile(const char *path) {
    FILE *f = fopen(path, "r");
    long long pid = -1;
    if (!f) return -1;
    if (fscanf(f, "%lld", &pid) != 1) pid = -1;
    fclose(f);
    return pid;
}

/* Check if process with given PID is alive */
long long xdaemon_is_alive(long long pid) {
    if (pid <= 0) return 0;
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return 0;
    DWORD code;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return code == STILL_ACTIVE ? 1 : 0;
#else
    return kill((pid_t)pid, 0) == 0 ? 1 : 0;
#endif
}

long long xdaemon_get_pid(void *handle) {
    DaemonState *ds = (DaemonState *)handle;
    return ds ? ds->pid : -1;
}

long long xdaemon_is_daemonized(void *handle) {
    DaemonState *ds = (DaemonState *)handle;
    return ds ? ds->daemonized : 0;
}

void xdaemon_set_running(void *handle, long long val) {
    DaemonState *ds = (DaemonState *)handle;
    if (ds) ds->running = (int)val;
}

long long xdaemon_is_running(void *handle) {
    DaemonState *ds = (DaemonState *)handle;
    return ds ? ds->running : 0;
}

const char *xdaemon_name(void *handle) {
    DaemonState *ds = (DaemonState *)handle;
    return ds ? ds->name : "";
}

/* Get status string */
const char *xdaemon_status(void *handle) {
    DaemonState *ds = (DaemonState *)handle;
    if (!ds) return "invalid";
    sprintf(info_buf, "%s: pid=%lld running=%d daemonized=%d",
            ds->name, ds->pid, ds->running, ds->daemonized);
    return info_buf;
}
