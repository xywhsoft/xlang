#include <windows.h>
#include <stdio.h>
#include <string.h>

#define XRT_MODULE_MEMORY_DEBUG

static int test_case;
static int close_calls;

static LSTATUS WINAPI test_open(HKEY root, LPCWSTR path, DWORD options,
    REGSAM access, PHKEY result);
static LSTATUS WINAPI test_query(HKEY key, LPCWSTR name, LPDWORD reserved,
    LPDWORD type, LPBYTE data, LPDWORD size);
static LSTATUS WINAPI test_close(HKEY key);

#define RegOpenKeyExW test_open
#define RegQueryValueExW test_query
#define RegCloseKey test_close
#include "../sysinfo_native.c"
#undef RegOpenKeyExW
#undef RegQueryValueExW
#undef RegCloseKey

static LSTATUS WINAPI test_open(HKEY root, LPCWSTR path, DWORD options,
    REGSAM access, PHKEY result)
{
    (void)root;
    (void)path;
    (void)options;
    (void)access;
    *result = (HKEY)1;
    return ERROR_SUCCESS;
}

static LSTATUS WINAPI test_query(HKEY key, LPCWSTR name, LPDWORD reserved,
    LPDWORD type, LPBYTE data, LPDWORD size)
{
    static const WCHAR unterminated[] = {L'C', L'P', L'U'};
    static const WCHAR terminated[] = {L'C', L'P', L'U', 0};
    static const WCHAR blank[] = {L' ', L'\t', 0};
    (void)key;
    (void)name;
    (void)reserved;
    *type = REG_SZ;
    if (test_case == 0) {
        memcpy(data, unterminated, sizeof(unterminated));
        *size = sizeof(unterminated);
    } else if (test_case == 1) {
        memcpy(data, terminated, sizeof(terminated));
        *size = sizeof(terminated);
    } else if (test_case == 2) {
        memcpy(data, unterminated, 3);
        *size = 3;
    } else {
        memcpy(data, blank, sizeof(blank));
        *size = sizeof(blank);
    }
    return ERROR_SUCCESS;
}

static LSTATUS WINAPI test_close(HKEY key)
{
    (void)key;
    close_calls++;
    return ERROR_SUCCESS;
}

int main(void)
{
    int i;
    for (i = 0; i < 4; i++) {
        char *name;
        test_case = i;
        name = xsysinfo_cpu_name();
        if (!name || strcmp(name, i >= 2 ? "unknown" : "CPU") != 0) {
            fprintf(stderr, "registry CPU name case %d failed: %s\n",
                i, name ? name : "null");
            xrtFree(name);
            return 1;
        }
        xrtFree(name);
    }
    if (close_calls != 4) return 2;
    {
        char *name;
        int failed_by_memory;
        test_case = 0;
        if (!xrtMemDebugFailAfter(0u)) return 3;
        name = xsysinfo_cpu_name();
        failed_by_memory = xrtMemDebugFailTriggered() &&
            xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
        xrtMemDebugFailClear();
        if (name != NULL || !failed_by_memory || close_calls != 5) {
            xrtFree(name);
            return 4;
        }
        name = xsysinfo_cpu_name();
        if (name == NULL || strcmp(name, "CPU") != 0 ||
            xrtGetError() != NULL || close_calls != 6) {
            xrtFree(name);
            return 5;
        }
        xrtFree(name);
    }
    return 0;
}
