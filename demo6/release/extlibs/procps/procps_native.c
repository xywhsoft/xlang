/* 进程枚举与 RSS：Windows 使用进程快照，Linux 使用 /proc。 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#include "xrt.h"
#include "include/procps_native.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define XPROCPS_DOMAIN "xlang.extlib.procps"

static void xprocps_error(xerrkind kind, const char* message)
{
    xrtSetErrorInfo(kind, XPROCPS_DOMAIN, 0, message);
}

#ifdef _WIN32
#include <windows.h>
#ifndef WC_ERR_INVALID_CHARS
#define WC_ERR_INVALID_CHARS 0x00000080u
#endif

/* TCC 的精简 WinAPI 头不包含 tlhelp32.h；只声明本包使用的结构。 */
typedef struct xprocps_entry {
    DWORD size, usage, pid;
    ULONG_PTR heap;
    DWORD module, threads, parent;
    LONG priority;
    DWORD flags;
    WCHAR name[MAX_PATH];
} xprocps_entry;

typedef HANDLE (WINAPI *xprocps_snapshot_fn)(DWORD, DWORD);
typedef BOOL (WINAPI *xprocps_first_fn)(HANDLE, xprocps_entry*);
typedef BOOL (WINAPI *xprocps_next_fn)(HANDLE, xprocps_entry*);

/* WinAPI 返回的 FARPROC 原型不含参数，按指针位复制避免错误的函数调用强转。 */
#define XPROCPS_RESOLVE(target, module, symbol) do { \
    FARPROC address = GetProcAddress((module), (symbol)); \
    memcpy(&(target), &address, sizeof(target)); \
} while (0)

static int xprocps_walk(int (*visit)(const xprocps_entry*, void*), void* context)
{
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    xprocps_snapshot_fn snapshot_fn;
    xprocps_first_fn first_fn;
    xprocps_next_fn next_fn;
    HANDLE snapshot;
    xprocps_entry entry;
    int result = 1;
    int stopped = 0;
    if (kernel == NULL) goto unavailable;
    XPROCPS_RESOLVE(snapshot_fn, kernel, "CreateToolhelp32Snapshot");
    XPROCPS_RESOLVE(first_fn, kernel, "Process32FirstW");
    XPROCPS_RESOLVE(next_fn, kernel, "Process32NextW");
    if (snapshot_fn == NULL || first_fn == NULL || next_fn == NULL) goto unavailable;
    snapshot = snapshot_fn(2u, 0u); /* TH32CS_SNAPPROCESS */
    if (snapshot == INVALID_HANDLE_VALUE) goto unavailable;
    memset(&entry, 0, sizeof(entry));
    entry.size = sizeof(entry);
    if (first_fn(snapshot, &entry)) {
        do {
            int action = visit(&entry, context);
            if (action != 0) {
                result = action < 0 ? 0 : 1;
                stopped = 1;
                break;
            }
            entry.size = sizeof(entry);
        } while (next_fn(snapshot, &entry));
        if (!stopped && GetLastError() != ERROR_NO_MORE_FILES) result = 0;
    } else if (GetLastError() != ERROR_NO_MORE_FILES) {
        result = 0;
    }
    CloseHandle(snapshot);
    if (!result && xrtGetError() == NULL)
        xprocps_error(XERR_IO, "could not enumerate Windows processes");
    return result;
unavailable:
    xprocps_error(XERR_IO, "Windows process snapshot is unavailable");
    return 0;
}

typedef struct xprocps_lookup {
    DWORD pid;
    int found;
    DWORD parent;
    WCHAR name[MAX_PATH];
} xprocps_lookup;

static int xprocps_find_visit(const xprocps_entry* entry, void* context)
{
    xprocps_lookup* lookup = (xprocps_lookup*)context;
    if (entry->pid != lookup->pid) return 0;
    lookup->found = 1;
    lookup->parent = entry->parent;
    memcpy(lookup->name, entry->name, sizeof(lookup->name));
    lookup->name[MAX_PATH - 1] = L'\0';
    return 1;
}

static int xprocps_find(int64_t pid, xprocps_lookup* lookup)
{
    if (pid <= 0 || (uint64_t)pid > UINT32_MAX) return 0;
    memset(lookup, 0, sizeof(*lookup));
    lookup->pid = (DWORD)pid;
    return xprocps_walk(xprocps_find_visit, lookup) && lookup->found;
}

typedef struct xprocps_memory {
    DWORD size;
    DWORD page_faults;
    SIZE_T peak_working_set, working_set;
    SIZE_T peak_paged_pool, paged_pool;
    SIZE_T peak_nonpaged_pool, nonpaged_pool;
    SIZE_T pagefile, peak_pagefile;
} xprocps_memory;

#else
#include <dirent.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

static int xprocps_linux_walk(int (*visit)(int64_t, void*), void* context)
{
    DIR* directory = opendir("/proc");
    struct dirent* entry;
    int result = 1;
    int stopped = 0;
    if (directory == NULL) {
        xprocps_error(XERR_IO, "could not open /proc");
        return 0;
    }
    errno = 0;
    while ((entry = readdir(directory)) != NULL) {
        const unsigned char* text = (const unsigned char*)entry->d_name;
        int64_t pid = 0;
        int action;
        if (*text < '0' || *text > '9') continue;
        for (; *text >= '0' && *text <= '9'; ++text) {
            if (pid > (INT64_MAX - 9) / 10) break;
            pid = pid * 10 + (*text - '0');
        }
        if (*text != '\0' || pid == 0) continue;
        action = visit(pid, context);
        if (action != 0) {
            if (action < 0) result = 0;
            stopped = 1;
            break;
        }
        errno = 0;
    }
    if (!stopped && errno != 0) result = 0;
    closedir(directory);
    if (!result && xrtGetError() == NULL)
        xprocps_error(XERR_IO, "could not enumerate Linux processes");
    return result;
}
#endif

static int xprocps_append(int64_t pid, void* context)
{
    xvalue* item = xrtValueInt(pid);
    if (item == NULL || !xrtValueArrayAppendTake((xvalue*)context, &item)) {
        xrtValueRelease(item);
        return -1;
    }
    return 0;
}

#ifdef _WIN32
static int xprocps_append_windows(const xprocps_entry* entry, void* context)
{
    return xprocps_append((int64_t)entry->pid, context);
}

static int xprocps_count_windows(const xprocps_entry* entry, void* context)
{
    (void)entry;
    ++*(int64_t*)context;
    return 0;
}
#else
static int xprocps_count_linux(int64_t pid, void* context)
{
    (void)pid;
    ++*(int64_t*)context;
    return 0;
}
#endif

int64_t xprocps_pid(void)
{
    xrtClearError();
#ifdef _WIN32
    return (int64_t)GetCurrentProcessId();
#else
    return (int64_t)getpid();
#endif
}

int64_t xprocps_ppid(void)
{
    xrtClearError();
#ifdef _WIN32
    xprocps_lookup lookup;
    return xprocps_find(xprocps_pid(), &lookup) ? (int64_t)lookup.parent : -1;
#else
    return (int64_t)getppid();
#endif
}

int xprocps_exists(int64_t pid)
{
    xrtClearError();
    if (pid <= 0) return 0;
#ifdef _WIN32
    {
        xprocps_lookup lookup;
        return xprocps_find(pid, &lookup);
    }
#else
    if (pid > INT_MAX) return 0;
    return kill((pid_t)pid, 0) == 0 || errno == EPERM;
#endif
}

xvalue* xprocps_pids(void)
{
    xvalue* result;
    xrtClearError();
    result = xrtValueArray();
    if (result == NULL) return NULL;
#ifdef _WIN32
    if (!xprocps_walk(xprocps_append_windows, result)) {
#else
    if (!xprocps_linux_walk(xprocps_append, result)) {
#endif
        xrtValueRelease(result);
        return NULL;
    }
    return result;
}

int64_t xprocps_count(void)
{
    int64_t count = 0;
    xrtClearError();
#ifdef _WIN32
    return xprocps_walk(xprocps_count_windows, &count) ? count : -1;
#else
    return xprocps_linux_walk(xprocps_count_linux, &count) ? count : -1;
#endif
}

char* xprocps_name(int64_t pid)
{
    xrtClearError();
#ifdef _WIN32
    xprocps_lookup lookup;
    int size;
    char* result;
    if (!xprocps_find(pid, &lookup)) return NULL;
    size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        lookup.name, -1, NULL, 0, NULL, NULL);
    if (size <= 0) return NULL;
    result = (char*)xrtMalloc((size_t)size);
    if (result == NULL) return NULL;
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        lookup.name, -1, result, size, NULL, NULL)) {
        xrtFree(result);
        return NULL;
    }
    return result;
#else
    char path[64], stack[4096];
    char* target = stack;
    size_t capacity = sizeof(stack);
    char* fallback;
    ssize_t size;
    const char* base;
    char* result;
    if (pid <= 0 || pid > INT_MAX) return NULL;
    snprintf(path, sizeof(path), "/proc/%lld/exe", (long long)pid);
    for (;;) {
        char* grown;
        size = readlink(path, target, capacity - 1u);
        if (size < 0) break;
        if ((size_t)size < capacity - 1u) {
            target[size] = '\0';
            base = strrchr(target, '/');
            base = base != NULL ? base + 1 : target;
            if (!xrtUtf8Valid(xrtStrView(base), NULL)) {
                if (target != stack) xrtFree(target);
                xprocps_error(XERR_VALUE, "process name is not valid UTF-8");
                return NULL;
            }
            result = xrtStrDup(base);
            if (target != stack) xrtFree(target);
            return result;
        }
        /* readlink 不终止字符串；恰好填满时必须重读，不能回退到短 comm。 */
        if (capacity >= 1024u * 1024u) {
            if (target != stack) xrtFree(target);
            xprocps_error(XERR_RANGE, "process executable path is too long");
            return NULL;
        }
        grown = target == stack ? xrtMalloc(capacity * 2u) :
            xrtRealloc(target, capacity * 2u);
        if (grown == NULL) {
            if (target != stack) xrtFree(target);
            return NULL;
        }
        target = grown;
        capacity *= 2u;
    }
    if (target != stack) xrtFree(target);
    snprintf(path, sizeof(path), "/proc/%lld/comm", (long long)pid);
    fallback = (char*)xrtFileReadAllLimit(path, 256u, NULL);
    if (fallback != NULL) {
        fallback[strcspn(fallback, "\r\n")] = '\0';
        if (!xrtUtf8Valid(xrtStrView(fallback), NULL)) {
            xrtFree(fallback);
            xprocps_error(XERR_VALUE, "process name is not valid UTF-8");
            return NULL;
        }
    } else {
        const xerror* error = xrtGetError();
        /* 进程消失或名称不可读由语言层报告为不可用；内存失败仍须传播。 */
        if (error == NULL || xrtErrorKind(error) != XERR_MEMORY)
            xrtClearError();
    }
    return fallback;
#endif
}

void* xprocps_name_abi(int64_t pid)
{
    return xprocps_name(pid);
}

#ifndef _WIN32
static bool xprocps_statm_number(const char** cursor, int64* value)
{
    const char* start = *cursor;
    const char* end;
    while (*start == ' ' || *start == '\t') ++start;
    end = start;
    while (*end >= '0' && *end <= '9') ++end;
    if (end == start || (*end != ' ' && *end != '\t' &&
                         *end != '\n' && *end != '\0') ||
        !xrtIntParse((xstrview){start, (size_t)(end - start)}, 10u, 0u, value))
        return false;
    *cursor = end;
    return true;
}
#endif

int64_t xprocps_mem_kb(int64_t pid)
{
    xrtClearError();
    if (pid <= 0) return -1;
#ifdef _WIN32
    {
        HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
        BOOL (WINAPI *query)(HANDLE, xprocps_memory*, DWORD);
        HANDLE process;
        xprocps_memory memory = {0};
        if (kernel == NULL || (uint64_t)pid > UINT32_MAX) return -1;
        XPROCPS_RESOLVE(query, kernel, "K32GetProcessMemoryInfo");
        if (query == NULL) return -1;
        process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION |
            PROCESS_VM_READ, FALSE, (DWORD)pid);
        if (process == NULL) return -1;
        memory.size = sizeof(memory);
        if (!query(process, &memory, sizeof(memory))) {
            CloseHandle(process);
            return -1;
        }
        CloseHandle(process);
        return (int64_t)(memory.working_set / 1024u);
    }
#else
    {
        char path[64];
        char* text;
        const char* cursor;
        int64 virtual_pages, resident;
        long page_size;
        size_t size;
        if (pid > INT_MAX) return -1;
        snprintf(path, sizeof(path), "/proc/%lld/statm", (long long)pid);
        text = (char*)xrtFileReadAllLimit(path, 128u, &size);
        if (text == NULL) {
            const xerror* error = xrtGetError();
            /* 进程消失或 statm 不可读是正常查询结果，内存失败仍须传播。 */
            if (error == NULL || xrtErrorKind(error) != XERR_MEMORY)
                xrtClearError();
            return -1;
        }
        cursor = text;
        /* 第一列是虚拟页，第二列才是 RSS；两列均须是完整的非负整数。 */
        if (memchr(text, '\0', size) != NULL ||
            !xprocps_statm_number(&cursor, &virtual_pages) ||
            !xprocps_statm_number(&cursor, &resident)) {
            xrtFree(text);
            xrtClearError();
            return -1;
        }
        xrtFree(text);
        page_size = sysconf(_SC_PAGESIZE);
        if (page_size <= 0 || resident > INT64_MAX / page_size)
            return -1;
        return (int64_t)(resident * page_size / 1024u);
    }
#endif
}
