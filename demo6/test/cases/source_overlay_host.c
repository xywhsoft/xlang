/* Native public-API contract test. Load the isolated compiler DLL explicitly;
 * neither an IDE runtime nor a second XRT implementation enters this host. */
#include "include/xlc.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(c) do { if (!(c)) { fprintf(stderr, "source_overlay_host:%d: %s\n", __LINE__, #c); exit(1); } } while (0)
#define BIND(n) do { FARPROC proc = GetProcAddress(module, #n); REQUIRE(proc != NULL); memcpy(&api.n, &proc, sizeof(api.n)); } while (0)
static struct {
    XLC_ContextOptions (*xlcDefaultContextOptions)(void);
    XLC_Context* (*xlcCreateContext)(const XLC_ContextOptions*);
    void (*xlcDestroyContext)(XLC_Context*);
    int (*xlcSetWorkDir)(XLC_Context*, const char*);
    int (*xlcAddSourceFile)(XLC_Context*, const char*);
    int (*xlcAddSourceString)(XLC_Context*, const char*, const char*);
    int (*xlcSetSourceOverlay)(XLC_Context*, const char*, const char*, size_t);
    int (*xlcRemoveSourceOverlay)(XLC_Context*, const char*);
    int (*xlcClearSourceOverlays)(XLC_Context*);
    int (*xlcCompile)(XLC_Context*);
    int (*xlcCheck)(XLC_Context*, const char*, int);
    int (*xlcEmitTokens)(XLC_Context*, const char*, int);
    int (*xlcEmitAst)(XLC_Context*, const char*, int);
    const char* (*xlcGetLastError)(XLC_Context*);
} api;

static char* utf8(const wchar_t* wide)
{
    int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, NULL, 0, NULL, NULL);
    char* text;
    REQUIRE(count > 0);
    text = (char*)malloc((size_t)count); REQUIRE(text);
    REQUIRE(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, text, count, NULL, NULL));
    return text;
}

static char* readOutput(const wchar_t* dir, const wchar_t* name)
{
    wchar_t path[4096];
    HANDLE file;
    DWORD size, actual;
    char* text;
    REQUIRE(swprintf(path, 4096, L"%ls\\%ls", dir, name) > 0);
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    REQUIRE(file != INVALID_HANDLE_VALUE);
    size = GetFileSize(file, NULL); REQUIRE(size < 1024 * 1024);
    text = (char*)malloc((size_t)size + 1); REQUIRE(text);
    REQUIRE(ReadFile(file, text, size, &actual, NULL) && actual == size);
    CloseHandle(file); text[size] = 0; return text;
}

static XLC_Context* context(const char* dir)
{
    XLC_ContextOptions options = api.xlcDefaultContextOptions();
    XLC_Context* ctx;
    options.workDir = dir;
    ctx = api.xlcCreateContext(&options); REQUIRE(ctx);
    REQUIRE(api.xlcAddSourceFile(ctx, "main.xl"));
    return ctx;
}

static void overlay(XLC_Context* ctx, const char* path, const char* text)
{
    REQUIRE(api.xlcSetSourceOverlay(ctx, path, text, strlen(text)));
}

static const char good[] = "export func value() as int\n    return 7\nend\n";
static const char bad[] = "export func value() as int\n    return \"bad\"\nend\n";
static const char recursive[] = "import leaf\nexport func value() as int\n    return leaf.value()\nend\n";
static const char entry[] = "import dep\nfunc main(...) as int\n    return dep.value()\nend\n";

typedef struct CheckThread { XLC_Context* ctx; const char* output; int expected; } CheckThread;
static DWORD WINAPI checks(void* argument)
{
    CheckThread* test = (CheckThread*)argument;
    for (int i = 0; i < 20; ++i) REQUIRE(api.xlcCheck(test->ctx, test->output, 1) == test->expected);
    return 0;
}

int wmain(int argc, wchar_t** argv)
{
    HMODULE module;
    XLC_Context *a, *b, *limits;
    char *dirA, *dirB, *text, *large;
    wchar_t cwd[4096], after[4096];
    char path[96];
    const char invalid[] = { (char)0xed, (char)0xa0, (char)0x80 };
    const char nul[] = { 'a', 0, 'b' };
    REQUIRE(argc == 4);
    module = LoadLibraryW(argv[1]); REQUIRE(module);
    BIND(xlcDefaultContextOptions); BIND(xlcCreateContext); BIND(xlcDestroyContext);
    BIND(xlcSetWorkDir); BIND(xlcAddSourceFile); BIND(xlcAddSourceString);
    BIND(xlcSetSourceOverlay); BIND(xlcRemoveSourceOverlay); BIND(xlcClearSourceOverlays);
    BIND(xlcCompile); BIND(xlcCheck); BIND(xlcEmitTokens); BIND(xlcEmitAst); BIND(xlcGetLastError);
    dirA = utf8(argv[2]); dirB = utf8(argv[3]);
    REQUIRE(GetCurrentDirectoryW(4096, cwd) > 0);
    a = context(dirA); b = context(dirA);
    REQUIRE(!api.xlcCheck(a, "initial.json", 1));
    text = readOutput(argv[2], L"initial.json"); REQUIRE(strstr(text, "dep.xl")); free(text);
    overlay(a, "./dep.xl", good);
    REQUIRE(api.xlcCheck(a, "good.json", 1));
    REQUIRE(!api.xlcCheck(b, "other.json", 1));
    REQUIRE(api.xlcCompile(a));
    REQUIRE(!api.xlcSetSourceOverlay(a, "dep.xl", invalid, sizeof(invalid)));
    REQUIRE(*api.xlcGetLastError(a));
    REQUIRE(!api.xlcSetSourceOverlay(a, "dep.xl", nul, sizeof(nul)));
    REQUIRE(api.xlcCompile(a)); /* Failed mutations retain a usable cached unit. */
    overlay(a, "DEP.XL", bad);
    REQUIRE(!api.xlcCheck(a, "changed.json", 1));
    overlay(a, "dep.xl", recursive); overlay(a, "leaf.xl", good);
    REQUIRE(api.xlcCheck(a, "recursive.json", 1));
    overlay(a, "leaf.xl", bad);
    REQUIRE(!api.xlcCheck(a, "leaf-sem.json", 1));
    text = readOutput(argv[2], L"leaf-sem.json");
    REQUIRE(strstr(text, "leaf.xl") && strstr(text, "\"line\":2")); free(text);
    overlay(a, "leaf.xl", "export func value(\n");
    REQUIRE(!api.xlcCheck(a, "leaf-parse.json", 1));
    text = readOutput(argv[2], L"leaf-parse.json"); REQUIRE(strstr(text, "leaf.xl")); free(text);
    overlay(a, "leaf.xl", good);
    REQUIRE(api.xlcEmitAst(a, "main.ast.json", 1));
    overlay(a, "main.xl", "func main(...) as int\n    return 123\nend\n");
    REQUIRE(api.xlcEmitTokens(a, "main.tokens.json", 1));
    text = readOutput(argv[2], L"main.tokens.json"); REQUIRE(strstr(text, "123")); free(text);
    REQUIRE(api.xlcCheck(a, "entry-overlay.json", 1));
    REQUIRE(api.xlcRemoveSourceOverlay(a, "main.xl"));
    REQUIRE(api.xlcCheck(a, "restored-entry.json", 1));
    REQUIRE(api.xlcRemoveSourceOverlay(a, "leaf.xl"));
    REQUIRE(!api.xlcCheck(a, "removed-leaf.json", 1));
    overlay(a, "leaf.xl", "import dep\nexport func value() as int\n    return 7\nend\n");
    REQUIRE(!api.xlcCheck(a, "cycle.json", 1));
    text = readOutput(argv[2], L"cycle.json"); REQUIRE(strstr(text, "source import cycle detected")); free(text);
    overlay(a, "leaf.xl", good);
    REQUIRE(api.xlcAddSourceFile(a, "virtual-entry.xl"));
    overlay(a, "virtual-entry.xl", entry);
    REQUIRE(api.xlcCheck(a, "virtual-entry.json", 1));
    REQUIRE(api.xlcAddSourceString(a, "virtual-entry.xl", "func main(...) as int\n    return 0\nend\n"));
    overlay(a, "virtual-entry.xl", "invalid ! !\n");
    REQUIRE(api.xlcCheck(a, "string-entry.json", 1));
    REQUIRE(api.xlcAddSourceFile(a, "main.xl"));
    overlay(a, "子/Ω.xl", good);
    overlay(a, "子/ω.XL", bad); /* Unicode Windows case aliases replace one record. */
    overlay(a, "main.xl", "import \"子/Ω.xl\" as u\nfunc main(...) as int\n    return u.value()\nend\n");
    REQUIRE(!api.xlcCheck(a, "unicode.json", 1));
    text = readOutput(argv[2], L"unicode.json"); REQUIRE(strstr(text, "ω.XL")); free(text);
    REQUIRE(api.xlcRemoveSourceOverlay(a, "子/Ω.xl"));
    REQUIRE(api.xlcRemoveSourceOverlay(a, "main.xl"));
    REQUIRE(api.xlcCompile(a));
    REQUIRE(api.xlcSetWorkDir(a, dirB));
    REQUIRE(!api.xlcCompile(a));
    REQUIRE(!api.xlcCheck(a, "other-dir.json", 1));
    overlay(a, "dep.xl", good);
    REQUIRE(api.xlcCheck(a, "other-dir-good.json", 1));
    REQUIRE(api.xlcSetWorkDir(a, dirA));
    REQUIRE(api.xlcCheck(a, "original-dir.json", 1));
    {
        CheckThread aa = { a, "thread-a.json", 1 }, bb = { b, "thread-b.json", 0 };
        HANDLE threads[2] = { CreateThread(NULL, 0, checks, &aa, 0, NULL), CreateThread(NULL, 0, checks, &bb, 0, NULL) };
        REQUIRE(threads[0] && threads[1]);
        REQUIRE(WaitForMultipleObjects(2, threads, TRUE, 60000) == WAIT_OBJECT_0);
        CloseHandle(threads[0]); CloseHandle(threads[1]);
    }
    REQUIRE(api.xlcClearSourceOverlays(a));
    REQUIRE(!api.xlcCheck(a, "clear.json", 1));
    REQUIRE(api.xlcRemoveSourceOverlay(a, "missing.xl"));
    limits = context(dirA);
    for (int i = 0; i < 64; ++i) { snprintf(path, sizeof(path), "virtual-%02d.xl", i); overlay(limits, path, ""); }
    REQUIRE(!api.xlcSetSourceOverlay(limits, "overflow.xl", "", 0));
    overlay(limits, "VIRTUAL-00.XL", good);
    REQUIRE(api.xlcRemoveSourceOverlay(limits, "virtual-01.xl"));
    overlay(limits, "overflow.xl", good);
    REQUIRE(api.xlcClearSourceOverlays(limits));
    large = (char*)malloc(8u * 1024u * 1024u + 1); REQUIRE(large);
    memset(large, ' ', 8u * 1024u * 1024u + 1);
    REQUIRE(!api.xlcSetSourceOverlay(limits, "large.xl", large, 8u * 1024u * 1024u + 1));
    for (int i = 0; i < 4; ++i) { snprintf(path, sizeof(path), "large-%d.xl", i); REQUIRE(api.xlcSetSourceOverlay(limits, path, large, 8u * 1024u * 1024u)); }
    REQUIRE(!api.xlcSetSourceOverlay(limits, "overflow.xl", " ", 1));
    overlay(limits, "large-0.xl", "");
    overlay(limits, "overflow.xl", good);
    free(large);
    api.xlcDestroyContext(limits); api.xlcDestroyContext(b); api.xlcDestroyContext(a);
    REQUIRE(GetCurrentDirectoryW(4096, after) > 0 && wcscmp(cwd, after) == 0);
    free(dirA); free(dirB);
    /* DEMO6/XRT DLLs stay loaded for this process lifetime. Early unload
     * leaves thread cleanup callbacks in both the old and current runtime. */
    puts("source overlay public API: passed");
    return 0;
}
