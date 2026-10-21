/* Windows/Linux 系统信息；字符串结果交给调用方用 xrtFree 释放。 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#include "xrt.h"
#include "sysinfo_native.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#ifndef PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64 12
#endif
#else
#include <sys/utsname.h>
#include <unistd.h>
#endif

static char *xsys_copy(const char *text)
{
    size_t size = strlen(text);
    /* Linux 内核与主机配置不保证返回 UTF-8；非法文本不进入语言字符串。 */
    if (!xrtUtf8Valid((xstrview){text, size}, NULL)) {
        text = "unknown";
        size = 7u;
    }
    char *copy = xrtMalloc(size + 1u);
    if (copy) memcpy(copy, text, size + 1u);
    return copy;
}

#ifdef _WIN32
static char *xsys_utf8(const WCHAR *text)
{
    int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    char *result;
    if (size <= 0) return NULL;
    result = xrtMalloc((size_t)size);
    if (!result) return NULL;
    if (WideCharToMultiByte(CP_UTF8, 0, text, -1, result, size, NULL, NULL) != size) {
        xrtFree(result);
        return NULL;
    }
    return result;
}
#endif

void *xsysinfo_os_name(void)
{
    xrtClearError();
#ifdef _WIN32
    return xsys_copy("Windows");
#else
    struct utsname info;
    return xsys_copy(uname(&info) == 0 ? info.sysname : "unknown");
#endif
}

void *xsysinfo_os_version(void)
{
    xrtClearError();
#ifdef _WIN32
    typedef LONG (WINAPI *rtl_get_version)(OSVERSIONINFOW *);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    FARPROC symbol = ntdll ? GetProcAddress(ntdll, "RtlGetVersion") : NULL;
    rtl_get_version get_version = NULL;
    OSVERSIONINFOW info;
    char text[64];
    memset(&info, 0, sizeof(info));
    info.dwOSVersionInfoSize = sizeof(info);
    if (symbol) memcpy(&get_version, &symbol, sizeof(get_version));
    if (!get_version || get_version(&info) != 0) return xsys_copy("unknown");
    snprintf(text, sizeof(text), "%lu.%lu.%lu", (unsigned long)info.dwMajorVersion,
        (unsigned long)info.dwMinorVersion, (unsigned long)info.dwBuildNumber);
    return xsys_copy(text);
#else
    struct utsname info;
    return xsys_copy(uname(&info) == 0 ? info.release : "unknown");
#endif
}

void *xsysinfo_arch(void)
{
    xrtClearError();
#ifdef _WIN32
    SYSTEM_INFO info;
    GetNativeSystemInfo(&info);
    switch (info.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: return xsys_copy("x86_64");
        case PROCESSOR_ARCHITECTURE_INTEL: return xsys_copy("x86");
        case PROCESSOR_ARCHITECTURE_ARM64: return xsys_copy("arm64");
        case PROCESSOR_ARCHITECTURE_ARM: return xsys_copy("arm");
        default: return xsys_copy("unknown");
    }
#else
    struct utsname info;
    return xsys_copy(uname(&info) == 0 ? info.machine : "unknown");
#endif
}

void *xsysinfo_hostname(void)
{
    xrtClearError();
#ifdef _WIN32
    WCHAR name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    char *result;
    if (!GetComputerNameW(name, &size)) return xsys_copy("unknown");
    result = xsys_utf8(name);
    if (result == NULL && xrtGetError() != NULL) return NULL;
    return result ? result : xsys_copy("unknown");
#else
    char name[256];
    if (gethostname(name, sizeof(name) - 1) != 0) return xsys_copy("unknown");
    name[sizeof(name) - 1] = '\0';
    return xsys_copy(name);
#endif
}

int64_t xsysinfo_cpu_count(void)
{
    xrtClearError();
#ifdef _WIN32
    typedef DWORD (WINAPI *active_processor_count)(WORD);
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    FARPROC symbol = kernel ? GetProcAddress(kernel, "GetActiveProcessorCount") : NULL;
    active_processor_count get_count = NULL;
    if (symbol) memcpy(&get_count, &symbol, sizeof(get_count));
    /* 0xffff 表示全部处理器组，避免只统计当前组。 */
    return get_count ? (long long)get_count((WORD)0xffffu) : 0;
#else
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? (long long)count : 0;
#endif
}

static long long xsysinfo_memory(int available)
{
#ifdef _WIN32
    MEMORYSTATUSEX info;
    unsigned long long bytes;
    memset(&info, 0, sizeof(info));
    info.dwLength = sizeof(info);
    if (!GlobalMemoryStatusEx(&info)) return -1;
    bytes = available ? info.ullAvailPhys : info.ullTotalPhys;
    return bytes <= LLONG_MAX ? (long long)bytes : LLONG_MAX;
#else
    long pages;
    long size;
    if (available) {
        FILE *file = fopen("/proc/meminfo", "r");
        char line[128];
        int64 kib;
        if (!file) return -1;
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "MemAvailable:", 13) == 0) {
                const char *digits = line + 13;
                const char *unit;
                size_t length;
                while (*digits == ' ' || *digits == '\t') ++digits;
                length = strcspn(digits, " \t\r\n");
                unit = digits + length;
                while (*unit == ' ' || *unit == '\t') ++unit;
                if (digits[0] < '0' || digits[0] > '9' ||
                    !xrtIntParse((xstrview){digits, length}, 10u, 0u, &kib) ||
                    (strcmp(unit, "kB") != 0 && strcmp(unit, "kB\n") != 0 &&
                     strcmp(unit, "kB\r\n") != 0)) {
                    xrtClearError();
                    fclose(file);
                    return -1;
                }
                fclose(file);
                return kib <= LLONG_MAX / 1024u ?
                    (long long)(kib * 1024u) : LLONG_MAX;
            }
        }
        fclose(file);
        return -1;
    }
    pages = sysconf(_SC_PHYS_PAGES);
    size = sysconf(_SC_PAGE_SIZE);
    if (pages <= 0 || size <= 0) return -1;
    if (pages > LLONG_MAX / size) return LLONG_MAX;
    return (long long)pages * size;
#endif
}

int64_t xsysinfo_mem_total(void)
{
    xrtClearError();
    long long value = xsysinfo_memory(0);
    return value < 0 ? 0 : value;
}

int64_t xsysinfo_mem_available(void)
{
    xrtClearError();
    long long value = xsysinfo_memory(1);
    return value < 0 ? 0 : value;
}

int64_t xsysinfo_mem_usage_pct(void)
{
    xrtClearError();
    long long total = xsysinfo_memory(0);
    long long available = xsysinfo_memory(1);
    double used;
    if (total <= 0 || available < 0) return 0;
    used = (double)(total - available) * 100.0 / (double)total;
    if (used < 0) return 0;
    if (used > 100) return 100;
    return (long long)used;
}

int64_t xsysinfo_page_size(void)
{
    xrtClearError();
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (long long)info.dwPageSize;
#else
    long size = sysconf(_SC_PAGE_SIZE);
    return size > 0 ? (long long)size : 0;
#endif
}

int64_t xsysinfo_uptime(void)
{
    xrtClearError();
#ifdef _WIN32
    return (long long)(GetTickCount64() / 1000);
#else
    FILE *file = fopen("/proc/uptime", "r");
    double seconds = 0;
    if (!file) return 0;
    if (fscanf(file, "%lf", &seconds) != 1) seconds = 0;
    fclose(file);
    /* double 会把 LLONG_MAX 舍入为 2^63；该值不能转成 int64。 */
    return seconds >= 0 && seconds < 0x1p63 ? (long long)seconds : 0;
#endif
}

void *xsysinfo_cpu_name(void)
{
    xrtClearError();
#ifdef _WIN32
    HKEY key;
    WCHAR name[256];
    DWORD size = sizeof(name) - sizeof(name[0]);
    DWORD type = 0;
    char *result = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            0, KEY_READ, &key) != ERROR_SUCCESS) return xsys_copy("unknown");
    if (RegQueryValueExW(key, L"ProcessorNameString", NULL, &type,
            (LPBYTE)name, &size) == ERROR_SUCCESS && type == REG_SZ &&
        size >= sizeof(WCHAR) && size <= sizeof(name) - sizeof(name[0]) &&
        size % sizeof(WCHAR) == 0) {
        WCHAR *start = name;
        name[size / sizeof(WCHAR)] = L'\0';
        while (*start == L' ' || *start == L'\t') start++;
        if (*start != L'\0') result = xsys_utf8(start);
    }
    RegCloseKey(key);
    if (result == NULL && xrtGetError() != NULL) return NULL;
    return result ? result : xsys_copy("unknown");
#else
    FILE *file = fopen("/proc/cpuinfo", "r");
    char stack[512];
    if (!file) return xsys_copy("unknown");
    while (fgets(stack, sizeof(stack), file)) {
        size_t length = strlen(stack);
        size_t capacity = sizeof(stack);
        char *line = stack;
        char *colon;
        if (strncmp(stack, "model name", 10) != 0 ||
            (stack[10] != ':' && stack[10] != ' ' && stack[10] != '\t')) {
            /* 长行的后续片段不是新字段。 */
            if (length && stack[length - 1] != '\n') {
                int ch;
                while ((ch = fgetc(file)) != '\n' && ch != EOF) {}
            }
            continue;
        }

        /* 型号极长时只扩容这一行，不截断公开返回值。 */
        while (length && line[length - 1] != '\n' && !feof(file)) {
            char *grown;
            if (capacity > (size_t)INT_MAX / 2u) {
                if (line != stack) xrtFree(line);
                fclose(file);
                return xsys_copy("unknown");
            }
            capacity *= 2u;
            grown = line == stack ? xrtMalloc(capacity) : xrtRealloc(line, capacity);
            if (!grown) {
                if (line != stack) xrtFree(line);
                fclose(file);
                return NULL;
            }
            if (line == stack) memcpy(grown, stack, length + 1u);
            line = grown;
            if (!fgets(line + length, (int)(capacity - length), file)) break;
            length += strlen(line + length);
        }
        if (ferror(file)) {
            if (line != stack) xrtFree(line);
            fclose(file);
            return xsys_copy("unknown");
        }
        colon = line + 10;
        while (*colon == ' ' || *colon == '\t') colon++;
        if (*colon == ':') {
            char *result;
            colon++;
            while (*colon == ' ' || *colon == '\t') colon++;
            colon[strcspn(colon, "\r\n")] = '\0';
            result = xsys_copy(*colon != '\0' ? colon : "unknown");
            if (line != stack) xrtFree(line);
            fclose(file);
            return result;
        }
        if (line != stack) xrtFree(line);
    }
    fclose(file);
    return xsys_copy("unknown");
#endif
}
