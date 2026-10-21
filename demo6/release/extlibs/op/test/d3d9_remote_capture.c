#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "xrt.h"

#ifdef REMOTE_WGL
#define TARGET_EXE L"wgl_remote_target.exe"
#define TARGET_TITLE L"op wgl remote %lu"
#define DISPLAY_MODE "wgl"
#define TEST_LABEL "op WGL remote capture: PASS"
#else
#define TARGET_EXE L"d3d9_remote_target.exe"
#define TARGET_TITLE L"op d3d9 remote %lu"
#define DISPLAY_MODE "d3d9"
#define TEST_LABEL "op D3D9 remote capture: PASS"
#endif

extern void* xop_create(void);
extern void xop_destroy(void* context);
extern long long xop_bind_window(void* context, long long hwnd,
    const char* display, const char* mouse, const char* keypad, long long mode);
extern long long xop_unbind(void* context);
extern void* xop_get_color(void* context, long long x, long long y);

static int start_target(PROCESS_INFORMATION* process, HWND* window, int blue)
{
    wchar_t executable[128];
    wchar_t command[128];
    wchar_t title[96];
    STARTUPINFOW startup = { 0 };
    int i;
    startup.cb = sizeof(startup);
    wsprintfW(executable, L"build/test/op/%s", TARGET_EXE);
    wsprintfW(command, L"%s blue", TARGET_EXE);
    if (!CreateProcessW(executable, blue ? command : NULL, NULL, NULL, FALSE,
            CREATE_NO_WINDOW, NULL, NULL, &startup, process)) {
        fprintf(stderr, "remote target start: %lu\n", GetLastError());
        return 0;
    }
    wsprintfW(title, TARGET_TITLE, (unsigned long)process->dwProcessId);
    for (i = 0; i < 100; ++i) {
        DWORD pid = 0u;
        *window = FindWindowW(NULL, title);
        if (*window != NULL) GetWindowThreadProcessId(*window, &pid);
        if (pid == process->dwProcessId) return 1;
        *window = NULL;
        Sleep(20u);
    }
    fputs("remote target window missing\n", stderr);
    return 0;
}

static int wait_color(void* context, const char* expected)
{
    int i;
    char last[32] = "";
    for (i = 0; i < 100; ++i) {
        char* color = (char*)xop_get_color(context, 0, 0);
        int matched = color != NULL && strcmp(color, expected) == 0;
        if (color != NULL) snprintf(last, sizeof(last), "%s", color);
        xrtFree(color);
        if (matched) return 1;
        Sleep(30u);
    }
    fprintf(stderr, "remote frame mismatch: expected %s, last %s, error %s\n",
        expected, last, xrtGetError() ? xrtErrorMessage(xrtGetError()) : "none");
    return 0;
}

static void stop_target(PROCESS_INFORMATION* process, HWND window)
{
    if (process->hProcess == NULL) return;
    if (window != NULL && IsWindow(window)) PostMessageW(window, WM_CLOSE, 0u, 0);
    if (WaitForSingleObject(process->hProcess, 2000u) != WAIT_OBJECT_0) {
        TerminateProcess(process->hProcess, 3u);
        WaitForSingleObject(process->hProcess, INFINITE);
    }
    CloseHandle(process->hThread);
    CloseHandle(process->hProcess);
    process->hThread = NULL;
    process->hProcess = NULL;
}

static void report_target(const char* name, PROCESS_INFORMATION* process)
{
    DWORD status = 0;
    if (process->hProcess != NULL && GetExitCodeProcess(process->hProcess, &status))
        fprintf(stderr, "%s target status: 0x%08lx\n", name, (unsigned long)status);
}

int main(void)
{
    PROCESS_INFORMATION red = { 0 };
    PROCESS_INFORMATION blue = { 0 };
    HWND redWindow = NULL;
    HWND blueWindow = NULL;
    void* redContext = NULL;
    void* blueContext = NULL;
    int result = 1;
    if (!start_target(&red, &redWindow, 0)) goto done;
    redContext = xop_create();
    if (redContext == NULL || !xop_bind_window(redContext, (long long)(intptr_t)redWindow,
            DISPLAY_MODE, "normal", "normal", 0) || !wait_color(redContext, "FF0000")) {
        report_target("red", &red);
        goto done;
    }
    if (!start_target(&blue, &blueWindow, 1)) goto done;
    blueContext = xop_create();
    if (blueContext == NULL || !xop_bind_window(blueContext, (long long)(intptr_t)blueWindow,
            DISPLAY_MODE, "normal", "normal", 0) || !wait_color(blueContext, "0000FF")) goto done;
    if (!wait_color(redContext, "FF0000")) goto done;
    stop_target(&blue, blueWindow);
    {
        char* color = (char*)xop_get_color(blueContext, 0, 0);
        int stale = color != NULL && color[0] != '\0';
        xrtFree(color);
        if (stale || !wait_color(redContext, "FF0000")) {
            fputs("exited target returned stale frame or affected the other context\n", stderr);
            goto done;
        }
    }
    puts(TEST_LABEL);
    result = 0;
done:
    if (blueContext != NULL) { xop_unbind(blueContext); xop_destroy(blueContext); }
    if (redContext != NULL) { xop_unbind(redContext); xop_destroy(redContext); }
    stop_target(&blue, blueWindow);
    stop_target(&red, redWindow);
    return result;
}
