/* dupfind_shim.c - build-time shim supplying xl6_command_argv_to_arg.
 *
 * WORKAROUND for release build breakage: the xlang codegen emits a call to
 * xl6_command_argv_to_arg() in the generated main wrapper, but the helper that
 * should emit its static definition (xl6CgenEmitArgvArrayHelper in
 * xl6_cgen_main.c) is dead code -- never called. xl run resolves the symbol
 * from xl.dll exports, but xl build statically embeds a trimmed XRT that lacks
 * it, so TCC reports "unresolved reference to xl6_command_argv_to_arg" for
 * EVERY program. This file provides the missing definition with external
 * linkage so module.c can link against it. The body mirrors exactly what the
 * dead codegen helper would have emitted (Windows UTF-16 argv -> UTF-8 path).
 *
 * This is a user-side build shim (like codec's manual hex to bypass BUG-15/16),
 * not a compiler-source modification. Build with:
 *   xl build dupfind.xl dupfind_shim.c -o dupfind.exe
 */
#ifndef XL6_XVALUE_TYPEDEF
#define XL6_XVALUE_TYPEDEF
typedef struct xvalue_struct* xvalue;
#endif
#include <stddef.h>
#include <stdbool.h>

xvalue xvoCreateArray(void);
xvalue xvoCreateText(void* sVal, unsigned int iSize, bool bColloc);
bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);
void xvoUnref(xvalue pVal);

void* xrtMalloc(size_t size);
void xrtFree(void* p);

static xvalue xl6_argv_to_arg(int argc, char** argv)
{
    xvalue arg = xvoCreateArray();
    int i;

    if ( arg == 0 ) {
        return 0;
    }
    for ( i = 0; i < argc; ++i ) {
        const char* text = (argv != 0 && argv[i] != 0) ? argv[i] : "";
        xvalue item = xvoCreateText((void*)text, 0, false);
        if ( item == 0 || !xvoArrayAppendValue(arg, item, true) ) {
            if ( item != 0 ) {
                xvoUnref(item);
            }
            xvoUnref(arg);
            return 0;
        }
    }
    return arg;
}

#if defined(_WIN32) || defined(_WIN64)
#include <wchar.h>
#ifndef XL6_WINAPI_ARGV_DECL
#define XL6_WINAPI_ARGV_DECL
__declspec(dllimport) wchar_t* __stdcall GetCommandLineW(void);
__declspec(dllimport) wchar_t** __stdcall CommandLineToArgvW(const wchar_t* lpCmdLine, int* pNumArgs);
__declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int CodePage, unsigned long dwFlags, const wchar_t* lpWideCharStr, int cchWideChar, char* lpMultiByteStr, int cbMultiByte, const char* lpDefaultChar, int* lpUsedDefaultChar);
__declspec(dllimport) void* __stdcall LocalFree(void* hMem);
#define XL6_CP_UTF8 65001u
#endif

static char* xl6_wide_to_utf8(const wchar_t* text)
{
    int size;
    char* utf8;

    if ( text == 0 ) {
        text = L"";
    }
    size = WideCharToMultiByte(XL6_CP_UTF8, 0, text, -1, 0, 0, 0, 0);
    if ( size <= 0 ) {
        return 0;
    }
    utf8 = (char*)xrtMalloc((size_t)size);
    if ( utf8 == 0 ) {
        return 0;
    }
    if ( WideCharToMultiByte(XL6_CP_UTF8, 0, text, -1, utf8, size, 0, 0) <= 0 ) {
        xrtFree(utf8);
        return 0;
    }
    return utf8;
}

static xvalue xl6_windows_argv_to_arg(int fallbackArgc, char** fallbackArgv)
{
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    xvalue arg;
    int i;

    if ( argv == 0 ) {
        return xl6_argv_to_arg(fallbackArgc, fallbackArgv);
    }
    arg = xvoCreateArray();
    if ( arg == 0 ) {
        LocalFree(argv);
        return 0;
    }
    for ( i = 0; i < argc; ++i ) {
        char* text = xl6_wide_to_utf8(argv[i]);
        xvalue item = text != 0 ? xvoCreateText((void*)text, 0, false) : 0;
        if ( text != 0 ) {
            xrtFree(text);
        }
        if ( item == 0 || !xvoArrayAppendValue(arg, item, true) ) {
            if ( item != 0 ) {
                xvoUnref(item);
            }
            xvoUnref(arg);
            LocalFree(argv);
            return 0;
        }
    }
    LocalFree(argv);
    return arg;
}
#endif

/* External (non-static) so the generated module.c main wrapper can link. */
xvalue xl6_command_argv_to_arg(int argc, char** argv)
{
#if defined(_WIN32) || defined(_WIN64)
    return xl6_windows_argv_to_arg(argc, argv);
#else
    return xl6_argv_to_arg(argc, argv);
#endif
}
