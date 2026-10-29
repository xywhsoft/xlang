#include "include/xlc.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(c) do { if (!(c)) { fprintf(stderr, "source_report_api_host:%d: %s\n", __LINE__, #c); exit(1); } } while (0)
#define BIND(n) do { FARPROC proc = GetProcAddress(module, #n); REQUIRE(proc); memcpy(&api.n, &proc, sizeof(api.n)); } while (0)
static struct {
    XLC_ContextOptions (*xlcDefaultContextOptions)(void);
    XLC_Context* (*xlcCreateContext)(const XLC_ContextOptions*);
    void (*xlcDestroyContext)(XLC_Context*);
    int (*xlcAddSourceFile)(XLC_Context*, const char*);
    int (*xlcAddSourceString)(XLC_Context*, const char*, const char*);
    int (*xlcSetSourceOverlay)(XLC_Context*, const char*, const char*, size_t);
    int (*xlcClearSourceOverlays)(XLC_Context*);
    int (*xlcCheckWithSources)(XLC_Context*, const char*);
    int (*xlcCheck)(XLC_Context*, const char*, int);
    const char* (*xlcGetLastError)(XLC_Context*);
} api;
static char* utf8(const wchar_t* wide)
{
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, NULL, 0, NULL, NULL);
    char* text = (char*)malloc((size_t)n); REQUIRE(n > 0 && text);
    REQUIRE(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, text, n, NULL, NULL)); return text;
}
typedef struct Worker { XLC_Context* ctx; const char* output; int expected; } Worker;
static DWORD WINAPI checks(void* ptr)
{
    Worker* w = (Worker*)ptr;
    for (int i = 0; i < 12; ++i) REQUIRE(api.xlcCheckWithSources(w->ctx, w->output) == w->expected);
    return 0;
}
int wmain(int argc, wchar_t** argv)
{
    HMODULE module;
    char* directory;
    XLC_ContextOptions options;
    XLC_Context *a, *b;
    const char entry[] = "import dep\nfunc main(...) as int\n    return dep.value()\nend\n";
    const char bad[] = "export func value() as int\n    return \"bad\"\nend\n";
    wchar_t cwd[4096], after[4096];
    REQUIRE(argc == 3 && GetCurrentDirectoryW(4096, cwd)); module = LoadLibraryW(argv[1]); REQUIRE(module);
    BIND(xlcDefaultContextOptions); BIND(xlcCreateContext); BIND(xlcDestroyContext);
    BIND(xlcAddSourceFile); BIND(xlcAddSourceString); BIND(xlcSetSourceOverlay); BIND(xlcClearSourceOverlays);
    BIND(xlcCheckWithSources); BIND(xlcCheck); BIND(xlcGetLastError);
    directory = utf8(argv[2]); options = api.xlcDefaultContextOptions(); options.workDir = directory;
    a = api.xlcCreateContext(&options); b = api.xlcCreateContext(&options); REQUIRE(a && b);
    REQUIRE(api.xlcAddSourceFile(a, "main.xl") && api.xlcAddSourceFile(b, "main.xl"));
    REQUIRE(api.xlcCheckWithSources(a, "api-disk.json"));
    REQUIRE(api.xlcAddSourceString(a, "memory-main.xl", entry));
    REQUIRE(api.xlcSetSourceOverlay(a, "unused.xl", "ignored", 7));
    REQUIRE(api.xlcCheckWithSources(a, "api-memory.json"));
    REQUIRE(api.xlcSetSourceOverlay(a, "dep.xl", bad, sizeof(bad)-1));
    REQUIRE(!api.xlcCheckWithSources(a, "api-overlay-bad.json"));
    REQUIRE(strstr(api.xlcGetLastError(a), "dep.xl"));
    REQUIRE(api.xlcCheckWithSources(b, "api-isolated.json"));
    {
        Worker aa = {a, "api-thread-a.json", 0}, bb = {b, "api-thread-b.json", 1};
        HANDLE threads[2] = {CreateThread(NULL, 0, checks, &aa, 0, NULL), CreateThread(NULL, 0, checks, &bb, 0, NULL)};
        REQUIRE(threads[0] && threads[1]);
        REQUIRE(WaitForMultipleObjects(2, threads, TRUE, 60000) == WAIT_OBJECT_0);
        CloseHandle(threads[0]); CloseHandle(threads[1]);
    }
    REQUIRE(api.xlcClearSourceOverlays(a));
    REQUIRE(api.xlcCheckWithSources(a, "api-clear.json"));
    REQUIRE(api.xlcCheck(a, "api-original.json", 1));
    api.xlcDestroyContext(a); api.xlcDestroyContext(b); free(directory);
    REQUIRE(GetCurrentDirectoryW(4096, after) && !wcscmp(cwd, after));
    /* Hold XRT DLLs to process termination, as the isolated service does. */
    puts("source report public API: passed"); return 0;
}
