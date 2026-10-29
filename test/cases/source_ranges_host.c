/* Public range-API contract gate; explicitly load the isolated compiler DLL. */
#include "include/xlc.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(c) do { if (!(c)) { fprintf(stderr, "source_ranges_host:%d: %s\n", __LINE__, #c); exit(1); } } while (0)
#define BIND(n) do { FARPROC p = GetProcAddress(module, #n); REQUIRE(p); memcpy(&api.n, &p, sizeof(api.n)); } while (0)
static struct {
    XLC_ContextOptions (*xlcDefaultContextOptions)(void);
    XLC_Context* (*xlcCreateContext)(const XLC_ContextOptions*);
    void (*xlcDestroyContext)(XLC_Context*);
    int (*xlcAddSourceString)(XLC_Context*, const char*, const char*);
    int (*xlcAddSourceFile)(XLC_Context*, const char*);
    int (*xlcSetSourceOverlay)(XLC_Context*, const char*, const char*, size_t);
    int (*xlcClearSourceOverlays)(XLC_Context*);
    int (*xlcEmitAst)(XLC_Context*, const char*, int);
    int (*xlcEmitAstWithRanges)(XLC_Context*, const char*);
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
    const char source[] = "func \xE8\xAE\xA1\xE7\xAE\x97(int \xE5\x80\xBC) as int\r\n    return (\xE5\x80\xBC + 1)\r\nend";
    const char replacement[] = "func other() as int\n    return 2\nend\n";
    REQUIRE(argc == 3); module = LoadLibraryW(argv[1]); REQUIRE(module);
    BIND(xlcDefaultContextOptions); BIND(xlcCreateContext); BIND(xlcDestroyContext);
    BIND(xlcAddSourceString); BIND(xlcAddSourceFile); BIND(xlcSetSourceOverlay);
    BIND(xlcClearSourceOverlays); BIND(xlcEmitAst); BIND(xlcEmitAstWithRanges); BIND(xlcGetLastError);
    REQUIRE(!api.xlcEmitAstWithRanges(NULL, NULL));
    directory = utf8(argv[2]); options = api.xlcDefaultContextOptions(); options.workDir = directory;
    ctx = api.xlcCreateContext(&options); REQUIRE(ctx);
    REQUIRE(api.xlcAddSourceString(ctx, "memory.xl", source));
    REQUIRE(api.xlcEmitAstWithRanges(ctx, "native-memory.json"));
    REQUIRE(api.xlcEmitAst(ctx, "native-legacy.json", 1));
    REQUIRE(api.xlcSetSourceOverlay(ctx, "memory.xl", replacement, sizeof(replacement)-1));
    REQUIRE(api.xlcEmitAstWithRanges(ctx, "native-string-authoritative.json"));
    REQUIRE(api.xlcAddSourceFile(ctx, "virtual.xl"));
    REQUIRE(api.xlcSetSourceOverlay(ctx, "virtual.xl", replacement, sizeof(replacement)-1));
    REQUIRE(api.xlcEmitAstWithRanges(ctx, "native-virtual.json"));
    REQUIRE(api.xlcClearSourceOverlays(ctx));
    REQUIRE(!api.xlcEmitAstWithRanges(ctx, "native-missing.json"));
    REQUIRE(*api.xlcGetLastError(ctx));
    REQUIRE(api.xlcAddSourceString(ctx, "invalid.xl", "func incomplete(\n"));
    REQUIRE(!api.xlcEmitAstWithRanges(ctx, "native-invalid.json"));
    REQUIRE(*api.xlcGetLastError(ctx));
    {
        size_t count = 280, capacity = count * 6 + 128; char* deep = malloc(capacity); char* p = deep;
        REQUIRE(deep); p += sprintf(p, "func main() as int\n    return ");
        for (size_t i=0; i<count; ++i) p += sprintf(p, "1 + ");
        p += sprintf(p, "1");
        p += sprintf(p, "\nend\n"); REQUIRE((size_t)(p-deep) < capacity);
        { FILE* prior = fopen("native-deep.json", "wb"); REQUIRE(prior);
          REQUIRE(fputs("prior-output", prior) >= 0); REQUIRE(fclose(prior) == 0); }
        REQUIRE(api.xlcAddSourceString(ctx, "deep.xl", deep));
        REQUIRE(!api.xlcEmitAstWithRanges(ctx, "native-deep.json"));
        REQUIRE(strstr(api.xlcGetLastError(ctx), "exceeds limits")); free(deep);
    }
    {
        size_t count = 10000; char* wide = malloc(count * 2 + 1); REQUIRE(wide);
        for (size_t i = 0; i < count; ++i) { wide[i*2]='1'; wide[i*2+1]='\n'; }
        wide[count*2]=0;
        REQUIRE(api.xlcAddSourceString(ctx, "wide.xl", wide));
        REQUIRE(!api.xlcEmitAstWithRanges(ctx, "native-wide.json"));
        REQUIRE(strstr(api.xlcGetLastError(ctx), "exceeds limits")); free(wide);
    }
    REQUIRE(api.xlcAddSourceString(ctx, "recovered.xl", replacement));
    REQUIRE(api.xlcEmitAstWithRanges(ctx, "native-recovered.json"));
    api.xlcDestroyContext(ctx); free(directory);
    /* Like source_overlay_host, keep XRT loaded for process lifetime:
     * its FLS cleanup callbacks must remain mapped through thread shutdown. */
    puts("source ranges native API: PASS"); return 0;
}
