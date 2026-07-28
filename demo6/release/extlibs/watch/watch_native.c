/* watch_native.c - File/directory watching (polling-based, cross-platform) */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#define MAX_WATCH 32

typedef struct {
    char path[512];
    long long last_mtime;
    long long last_size;
    int active;
} WatchEntry;

typedef struct {
    WatchEntry entries[MAX_WATCH];
    int count;
} Watcher;

static char err_buf[256];
static char event_buf[1024];

const char *xwatch_error(void) { return err_buf; }

static long long get_mtime(const char *path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) {
        ULARGE_INTEGER uli;
        uli.LowPart = fad.ftLastWriteTime.dwLowDateTime;
        uli.HighPart = fad.ftLastWriteTime.dwHighDateTime;
        return (long long)(uli.QuadPart / 10000000);
    }
    return -1;
#else
    struct stat st;
    if (stat(path, &st) == 0) return (long long)st.st_mtime;
    return -1;
#endif
}

static long long get_size(const char *path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) {
        ULARGE_INTEGER uli;
        uli.LowPart = fad.nFileSizeLow;
        uli.HighPart = fad.nFileSizeHigh;
        return (long long)uli.QuadPart;
    }
    return -1;
#else
    struct stat st;
    if (stat(path, &st) == 0) return (long long)st.st_size;
    return -1;
#endif
}

void *xwatch_create(void) {
    Watcher *w = (Watcher *)calloc(1, sizeof(Watcher));
    return w;
}

void xwatch_destroy(void *handle) {
    Watcher *w = (Watcher *)handle;
    if (w) free(w);
}

/* Add a path to watch, returns watch index or -1 */
long long xwatch_add(void *handle, const char *path) {
    Watcher *w = (Watcher *)handle;
    if (!w || w->count >= MAX_WATCH) return -1;
    int idx = w->count;
    strncpy(w->entries[idx].path, path, 511);
    w->entries[idx].last_mtime = get_mtime(path);
    w->entries[idx].last_size = get_size(path);
    w->entries[idx].active = 1;
    w->count++;
    return idx;
}

/* Remove a watch by index */
long long xwatch_remove(void *handle, long long idx) {
    Watcher *w = (Watcher *)handle;
    if (!w || idx < 0 || idx >= w->count) return 0;
    w->entries[idx].active = 0;
    return 1;
}

/*
 * Poll for changes. Returns event string:
 * "MODIFIED:<path>" or "CREATED:<path>" or "DELETED:<path>" or "" if no change
 */
const char *xwatch_poll(void *handle) {
    Watcher *w = (Watcher *)handle;
    if (!w) return "";
    int i;
    for (i = 0; i < w->count; i++) {
        if (!w->entries[i].active) continue;
        char *path = w->entries[i].path;
        long long mtime = get_mtime(path);
        long long size = get_size(path);

        if (w->entries[i].last_mtime == -1 && mtime >= 0) {
            /* File created */
            w->entries[i].last_mtime = mtime;
            w->entries[i].last_size = size;
            sprintf(event_buf, "CREATED:%s", path);
            return event_buf;
        }
        if (w->entries[i].last_mtime >= 0 && mtime == -1) {
            /* File deleted */
            w->entries[i].last_mtime = -1;
            w->entries[i].last_size = -1;
            sprintf(event_buf, "DELETED:%s", path);
            return event_buf;
        }
        if (mtime != w->entries[i].last_mtime || size != w->entries[i].last_size) {
            w->entries[i].last_mtime = mtime;
            w->entries[i].last_size = size;
            sprintf(event_buf, "MODIFIED:%s", path);
            return event_buf;
        }
    }
    return "";
}

/* Get number of active watches */
long long xwatch_count(void *handle) {
    Watcher *w = (Watcher *)handle;
    if (!w) return 0;
    int i, c = 0;
    for (i = 0; i < w->count; i++)
        if (w->entries[i].active) c++;
    return c;
}

/* Get path by index */
const char *xwatch_path(void *handle, long long idx) {
    Watcher *w = (Watcher *)handle;
    if (!w || idx < 0 || idx >= w->count) return "";
    return w->entries[idx].path;
}

/* Check if file exists */
long long xwatch_exists(const char *path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES ? 1 : 0;
#else
    struct stat st;
    return stat(path, &st) == 0 ? 1 : 0;
#endif
}
