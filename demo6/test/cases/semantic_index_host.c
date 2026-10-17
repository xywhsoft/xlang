/* Public API lifetime: destroy contexts, keep XRT DLL loaded to process exit. */
#include "include/xlc.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(c) do { if (!(c)) { fprintf(stderr, "semantic_index_host:%d: %s\n", __LINE__, #c); exit(1); } } while (0)
#define BIND(n) do { FARPROC p = GetProcAddress(module, #n); REQUIRE(p); memcpy(&api.n, &p, sizeof(api.n)); } while (0)
static struct {
    XLC_ContextOptions (*xlcDefaultContextOptions)(void);
    XLC_Context* (*xlcCreateContext)(const XLC_ContextOptions*);
    void (*xlcDestroyContext)(XLC_Context*);
    int (*xlcAddSourceString)(XLC_Context*, const char*, const char*);
    int (*xlcAddSourceFile)(XLC_Context*, const char*);
    int (*xlcSetSourceOverlay)(XLC_Context*, const char*, const char*, size_t);
    int (*xlcCheckWithIndex)(XLC_Context*, const char*);
    int (*xlcCheckWithSources)(XLC_Context*, const char*);
    const char* (*xlcGetLastError)(XLC_Context*);
} api;
static char* utf8(const wchar_t* w)
{
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w, -1, NULL, 0, NULL, NULL);
    char* p; REQUIRE(n > 0); p = malloc((size_t)n); REQUIRE(p);
    REQUIRE(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w, -1, p, n, NULL, NULL)); return p;
}
int wmain(int argc, wchar_t** argv)
{
    HMODULE module; XLC_ContextOptions options; XLC_Context* ctx; char* directory;
    const char source[] = "\xEF\xBB\xBF" "func native(int value) as int\r\n    auto copy = value\r\n    return copy\r\nend";
    const char replacement[] = "func replacement() as int\n    return 2\nend\n";
    REQUIRE(argc == 3); module = LoadLibraryW(argv[1]); REQUIRE(module);
    BIND(xlcDefaultContextOptions); BIND(xlcCreateContext); BIND(xlcDestroyContext);
    BIND(xlcAddSourceString); BIND(xlcAddSourceFile); BIND(xlcSetSourceOverlay);
    BIND(xlcCheckWithIndex); BIND(xlcCheckWithSources); BIND(xlcGetLastError);
    REQUIRE(!api.xlcCheckWithIndex(NULL, NULL));
    directory = utf8(argv[2]); options = api.xlcDefaultContextOptions(); options.workDir = directory;
    ctx = api.xlcCreateContext(&options); REQUIRE(ctx);
    REQUIRE(api.xlcAddSourceString(ctx, "native-memory.xl", source));
    REQUIRE(api.xlcCheckWithIndex(ctx, "native-memory.json"));
    REQUIRE(api.xlcSetSourceOverlay(ctx, "native-memory.xl", replacement, sizeof(replacement)-1));
    REQUIRE(api.xlcCheckWithIndex(ctx, "native-string-authoritative.json"));
    REQUIRE(api.xlcAddSourceFile(ctx, "native-virtual.xl"));
    REQUIRE(api.xlcSetSourceOverlay(ctx, "native-virtual.xl", replacement, sizeof(replacement)-1));
    REQUIRE(api.xlcCheckWithIndex(ctx, "native-virtual.json"));
    REQUIRE(api.xlcAddSourceString(ctx, "native-invalid.xl", "func invalid() as int\n return missing\nend\n"));
    REQUIRE(!api.xlcCheckWithIndex(ctx, "native-invalid.json"));
    REQUIRE(*api.xlcGetLastError(ctx));
    REQUIRE(api.xlcAddSourceString(ctx, "native-memory.xl", source));
    REQUIRE(api.xlcCheckWithIndex(ctx, "native-recovery.json"));
    REQUIRE(api.xlcCheckWithSources(ctx, "native-legacy.json"));
    api.xlcDestroyContext(ctx); free(directory);
    puts("semantic_index_host passed"); return 0;
}
