/* Use real public compiler products. xlcCompile is only the frontend; ABI
 * rejection is tested at verified C/header emission, never by executing a
 * deliberately invalid native call. No compiler/runtime provider is linked. */
#include "xlc.h"
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <limits.h>
#include <stdlib.h>
#endif

static int exactCompilerImage(const char* requested)
{
#if defined(_WIN32)
    wchar_t input[32768], expected[32768], actual[32768];
    HMODULE module = GetModuleHandleW(L"xlc.dll");
    if (!module || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, requested, -1, input, 32768))
        return 0;
    DWORD n = GetFullPathNameW(input, 32768, expected, NULL);
    DWORD m = GetModuleFileNameW(module, actual, 32768);
    return n && n < 32768 && m && m < 32768 &&
        CompareStringOrdinal(expected, -1, actual, -1, TRUE) == CSTR_EQUAL;
#else
    Dl_info info;
    char expected[PATH_MAX], actual[PATH_MAX];
    return dladdr((void*)xlcDefaultContextOptions, &info) && info.dli_fname &&
        realpath(requested, expected) && realpath(info.dli_fname, actual) && !strcmp(expected, actual);
#endif
}

int main(int argc, char** argv)
{
    if (argc != 6 || (strcmp(argv[3], "c") && strcmp(argv[3], "header") && strcmp(argv[3], "front")))
        return 2;
    if (!exactCompilerImage(argv[1])) {
        fputs("ABI observer loaded compiler does not match requested image\n", stderr);
        return 4;
    }
    printf("ABI observer compiler verified: %s\n", argv[1]);
    XLC_ContextOptions options = xlcDefaultContextOptions();
    options.workDir = ".";
    options.tempDir = argv[4];
    options.tccDllPath = argv[1];
    XLC_Context* compiler = xlcCreateContext(&options);
    if (!compiler) { fputs("ABI observer context creation failed\n", stderr); return 3; }
    int ok = xlcAddSourceFile(compiler, argv[2]) && xlcCompile(compiler);
    if (ok && !strcmp(argv[3], "c")) ok = xlcEmitXlirC(compiler, argv[5]);
    if (ok && !strcmp(argv[3], "header"))
        ok = xlcEmitXlirHeader(compiler, argv[5], "STDLIB_ABI_OBSERVER_H");
    if (!ok) {
        const char* error = xlcGetLastError(compiler);
        if (!error || !*error) error = xlGetLastError(xlcRuntimeContext(compiler));
        fprintf(stderr, "ABI observer %s rejected: %s\n", argv[3], error && *error ? error : "without diagnostic");
    }
    xlcDestroyContext(compiler);
    if (ok) printf("ABI observer %s complete\n", argv[3]);
    return ok ? 0 : 1;
}
