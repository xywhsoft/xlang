/*
 * singleinstance_native.c - Single instance mutex (Windows: named mutex, POSIX: lock file)
 */
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

static HANDLE si_mutex = NULL;

int xsi_acquire(const char *name) {
    char mname[512];
    HANDLE h;
    if (!name || !name[0]) return 0;
    snprintf(mname, sizeof(mname), "Local\\xlang_si_%s", name);
    h = CreateMutexA(NULL, TRUE, mname);
    if (!h) return 0;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(h);
        return 0;
    }
    si_mutex = h;
    return 1;
}

void xsi_release(void) {
    if (si_mutex) {
        ReleaseMutex(si_mutex);
        CloseHandle(si_mutex);
        si_mutex = NULL;
    }
}

int xsi_is_acquired(void) {
    return si_mutex != NULL;
}

#else
/* POSIX: use lock file in /tmp */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

static int si_fd = -1;
static char si_path[512];

int xsi_acquire(const char *name) {
    if (!name || !name[0]) return 0;
    snprintf(si_path, sizeof(si_path), "/tmp/.xlang_si_%s.lock", name);
    si_fd = open(si_path, O_CREAT | O_RDWR, 0666);
    if (si_fd < 0) return 0;
    if (flock(si_fd, LOCK_EX | LOCK_NB) != 0) {
        close(si_fd);
        si_fd = -1;
        return 0;
    }
    return 1;
}

void xsi_release(void) {
    if (si_fd >= 0) {
        flock(si_fd, LOCK_UN);
        close(si_fd);
        unlink(si_path);
        si_fd = -1;
    }
}

int xsi_is_acquired(void) {
    return si_fd >= 0;
}
#endif
