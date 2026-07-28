/* sharedmem_native.c - Shared memory (Windows CreateFileMapping / POSIX shm) */
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

typedef struct {
    char name[256];
    long long size;
    char *data;
    int is_owner;
#ifdef _WIN32
    HANDLE hMap;
#endif
} SharedMem;

static char err_buf[256];
static char read_buf[4096];

const char *xshm_error(void) { return err_buf; }

void *xshm_create(const char *name, long long size) {
    SharedMem *sm = (SharedMem *)calloc(1, sizeof(SharedMem));
    strncpy(sm->name, name, 255);
    sm->size = size;

#ifdef _WIN32
    sm->hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                   (DWORD)(size >> 32), (DWORD)size, name);
    if (!sm->hMap) {
        sprintf(err_buf, "sharedmem: CreateFileMapping failed for '%s'", name);
        free(sm);
        return NULL;
    }
    sm->data = (char *)MapViewOfFile(sm->hMap, FILE_MAP_ALL_ACCESS, 0, 0, (SIZE_T)size);
    if (!sm->data) {
        sprintf(err_buf, "sharedmem: MapViewOfFile failed for '%s'", name);
        CloseHandle(sm->hMap);
        free(sm);
        return NULL;
    }
    sm->is_owner = 1;
#else
    /* Fallback: just allocate memory (no real sharing) */
    sm->data = (char *)calloc(1, (size_t)size);
    sm->is_owner = 1;
#endif
    return sm;
}

void *xshm_open(const char *name, long long size) {
    SharedMem *sm = (SharedMem *)calloc(1, sizeof(SharedMem));
    strncpy(sm->name, name, 255);
    sm->size = size;

#ifdef _WIN32
    sm->hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!sm->hMap) {
        sprintf(err_buf, "sharedmem: cannot open '%s'", name);
        free(sm);
        return NULL;
    }
    sm->data = (char *)MapViewOfFile(sm->hMap, FILE_MAP_ALL_ACCESS, 0, 0, (SIZE_T)size);
    if (!sm->data) {
        CloseHandle(sm->hMap);
        free(sm);
        return NULL;
    }
    sm->is_owner = 0;
#else
    sm->data = (char *)calloc(1, (size_t)size);
    sm->is_owner = 0;
#endif
    return sm;
}

void xshm_close(void *handle) {
    SharedMem *sm = (SharedMem *)handle;
    if (!sm) return;
#ifdef _WIN32
    if (sm->data) UnmapViewOfFile(sm->data);
    if (sm->hMap) CloseHandle(sm->hMap);
#else
    if (sm->data && sm->is_owner) free(sm->data);
#endif
    free(sm);
}

/* Write string at offset */
long long xshm_write(void *handle, long long offset, const char *data) {
    SharedMem *sm = (SharedMem *)handle;
    if (!sm || !sm->data) return 0;
    long long len = (long long)strlen(data);
    if (offset + len > sm->size) return 0;
    memcpy(sm->data + offset, data, (size_t)len);
    return len;
}

/* Read string from offset (up to max_len or null terminator) */
const char *xshm_read(void *handle, long long offset, long long max_len) {
    SharedMem *sm = (SharedMem *)handle;
    if (!sm || !sm->data || offset >= sm->size) { read_buf[0] = '\0'; return read_buf; }
    long long avail = sm->size - offset;
    long long n = avail < max_len ? avail : max_len;
    if (n > 4095) n = 4095;
    memcpy(read_buf, sm->data + offset, (size_t)n);
    read_buf[n] = '\0';
    return read_buf;
}

/* Write a long long value at offset */
long long xshm_write_int(void *handle, long long offset, long long value) {
    SharedMem *sm = (SharedMem *)handle;
    if (!sm || !sm->data) return 0;
    if (offset + 8 > sm->size) return 0;
    memcpy(sm->data + offset, &value, 8);
    return 1;
}

/* Read a long long value from offset */
long long xshm_read_int(void *handle, long long offset) {
    SharedMem *sm = (SharedMem *)handle;
    long long val = 0;
    if (!sm || !sm->data) return 0;
    if (offset + 8 > sm->size) return 0;
    memcpy(&val, sm->data + offset, 8);
    return val;
}

long long xshm_size(void *handle) {
    SharedMem *sm = (SharedMem *)handle;
    return sm ? sm->size : 0;
}

const char *xshm_name(void *handle) {
    SharedMem *sm = (SharedMem *)handle;
    return sm ? sm->name : "";
}

void xshm_zero(void *handle) {
    SharedMem *sm = (SharedMem *)handle;
    if (sm && sm->data)
        memset(sm->data, 0, (size_t)sm->size);
}
