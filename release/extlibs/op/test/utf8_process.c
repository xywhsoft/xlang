#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "xrt.h"

#include "../include/op.h"

static int same_text(void *result, const char *expected)
{
    int same = result && strcmp((const char *)result, expected) == 0;
    xrtFree(result);
    return same;
}

static int contains_text(void *result, const char *needle)
{
    int found = result && strstr((const char *)result, needle) != NULL;
    xrtFree(result);
    return found;
}

static DWORD WINAPI cold_toolhelp(void *argument)
{
    HANDLE start = (HANDLE)argument;
    if (WaitForSingleObject(start, 5000) != WAIT_OBJECT_0) return 1;
    xrtFree(xop_get_process_modules(0));
    return 0;
}

/* 固定前缀、最长 5 字符类别和 10 位 PID 总长小于 96 个码元。 */
static void event_name(WCHAR *output, const WCHAR *kind, DWORD parent)
{
    wsprintfW(output, L"Local\\op_utf8_%s_%lu", kind, (unsigned long)parent);
}

int main(void)
{
    static const char process_name[] =
        "\xe4\xb8\xad\xe6\x96\x87\xe8\xbf\x9b\xe7\xa8\x8b";
    static const char module_name[] =
        "\xe4\xb8\xad\xe6\x96\x87\xe6\xa8\xa1\xe5\x9d\x97.dll";
    WCHAR child_path[] = L"build/test/op/\x4E2D\x6587\x8FDB\x7A0B.exe";
    WCHAR ready_name[96];
    WCHAR stop_name[96];
    WCHAR parent_text[32];
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION child = {0};
    HANDLE ready = NULL;
    HANDLE stop = NULL;
    HANDLE start = NULL;
    HANDLE workers[12] = {0};
    char expected[128];
    long long window;
    int ok = 0;
    int i;

    start = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!start) goto cleanup;
    for (i = 0; i < 12; ++i) {
        workers[i] = CreateThread(NULL, 0, cold_toolhelp, start, 0, NULL);
        if (!workers[i]) goto cleanup;
    }
    SetEvent(start);
    if (WaitForMultipleObjects(12, workers, TRUE, 5000) != WAIT_OBJECT_0)
        goto cleanup;
    for (i = 0; i < 12; ++i) {
        DWORD code;
        if (!GetExitCodeThread(workers[i], &code) || code != 0) goto cleanup;
    }

    event_name(ready_name, L"ready", GetCurrentProcessId());
    event_name(stop_name, L"stop", GetCurrentProcessId());
    ready = CreateEventW(NULL, TRUE, FALSE, ready_name);
    stop = CreateEventW(NULL, TRUE, FALSE, stop_name);
    if (!ready || !stop) goto cleanup;
    wsprintfW(parent_text, L"%lu", (unsigned long)GetCurrentProcessId());
    if (!SetEnvironmentVariableW(L"OP_TEST_PARENT_PID", parent_text)) goto cleanup;
    startup.cb = sizeof(startup);
    if (!CreateProcessW(child_path, NULL, NULL, NULL, FALSE, 0,
                        NULL, NULL, &startup, &child)) goto cleanup;
    SetEnvironmentVariableW(L"OP_TEST_PARENT_PID", NULL);
    if (WaitForSingleObject(ready, 5000) != WAIT_OBJECT_0) goto cleanup;

    snprintf(expected, sizeof(expected), "%lu:%s.exe",
             (unsigned long)child.dwProcessId, process_name);
    {
        void *process_list = xop_enum_process(process_name);
        int modules_ok = contains_text(xop_get_process_modules(child.dwProcessId), module_name);
        int process_ok = process_list && strstr((const char *)process_list, expected);
        xrtFree(process_list);
        if (!modules_ok || !process_ok) goto cleanup;
    }
    window = xop_find_window("op_utf8_process_window", "\xe4\xbd\xa0\xe5\xa5\xbd OP");
    if (!window) goto cleanup;
    snprintf(expected, sizeof(expected), "%lld", window);
    if (!same_text(xop_enum_window_by_process(process_name,
                                              "\xe4\xbd\xa0\xe5\xa5\xbd OP",
                                              "op_utf8_process_window", 0), expected))
        goto cleanup;
    ok = 1;

cleanup:
    SetEnvironmentVariableW(L"OP_TEST_PARENT_PID", NULL);
    if (start) SetEvent(start);
    if (stop) SetEvent(stop);
    for (i = 0; i < 12; ++i) {
        if (workers[i]) {
            WaitForSingleObject(workers[i], 5000);
            CloseHandle(workers[i]);
        }
    }
    if (child.hProcess) {
        if (WaitForSingleObject(child.hProcess, 5000) != WAIT_OBJECT_0) ok = 0;
        CloseHandle(child.hProcess);
    }
    if (child.hThread) CloseHandle(child.hThread);
    if (start) CloseHandle(start);
    if (ready) CloseHandle(ready);
    if (stop) CloseHandle(stop);
    if (!ok) {
        fprintf(stderr, "op UTF-8 process/module test failed\n");
        return 1;
    }
    puts("op utf8 process and module: ok");
    return 0;
}
