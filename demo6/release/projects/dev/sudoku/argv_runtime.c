/* argv_runtime.c - companion for xl build (BUG-19 workaround).
 *
 * xl build does not emit the static argv helper into module.c
 * (xl6CgenEmitArgvArrayHelper in src/cgen/xl6_cgen_main.c is defined but
 * never wired into the codegen pipeline), so the standalone exe has an
 * unresolved reference to xl6_command_argv_to_arg. This file provides that
 * definition using the XRT C API, mirroring the intended cgen emission.
 *
 * Build:
 *   xl build config.xl argv_runtime.c -o config.exe \
 *       --link <xrt.o> --link <libtcc1.a> --link Ws2_32 --link IPHLPAPI --link Shell32
 */

#include <stddef.h>

/* The xl build csrc wrapper includes the xlang runtime headers before this
 * file, so xvalue / xvoCreateArray / xvoCreateText / xvoArrayAppendValue /
 * xvoUnref / xrtMalloc / xrtFree are already declared. We only provide the
 * missing xl6_command_argv_to_arg definition below. */


/* Plain fallback: build the xvalue argv array from the C runtime argv. */
static xvalue xl6_argv_to_arg(int argc, char** argv)
{
    xvalue arg = xvoCreateArray();
    int i;

    if (arg == 0) {
        return 0;
    }
    for (i = 0; i < argc; ++i) {
        const char* text = (argv != 0 && argv[i] != 0) ? argv[i] : "";
        xvalue item = xvoCreateText((void*)text, 0, 0);
        if (item == 0 || !xvoArrayAppendValue(arg, item, 1)) {
            if (item != 0) {
                xvoUnref(item);
            }
            xvoUnref(arg);
            return 0;
        }
    }
    return arg;
}


#if defined(_WIN32) || defined(_WIN64)
/* Direct dllimport declarations guarded so they don't conflict if the host
 * already pulled in windows.h / shellapi.h (matches the cgen helper). */
#ifndef XL6_WINAPI_ARGV_DECL
#define XL6_WINAPI_ARGV_DECL
__declspec(dllimport) wchar_t*  __stdcall GetCommandLineW(void);
__declspec(dllimport) wchar_t** __stdcall CommandLineToArgvW(const wchar_t* lpCmdLine, int* pNumArgs);
__declspec(dllimport) int       __stdcall WideCharToMultiByte(unsigned int CodePage, unsigned long dwFlags, const wchar_t* lpWideCharStr, int cchWideChar, char* lpMultiByteStr, int cbMultiByte, const char* lpDefaultChar, int* lpUsedDefaultChar);
__declspec(dllimport) void*     __stdcall LocalFree(void* hMem);
#ifndef XL6_CP_UTF8
#define XL6_CP_UTF8 65001u
#endif
#endif

static char* xl6_wide_to_utf8(const wchar_t* text)
{
    int size;
    char* utf8;

    if (text == 0) {
        text = L"";
    }
    size = WideCharToMultiByte(XL6_CP_UTF8, 0, text, -1, 0, 0, 0, 0);
    if (size <= 0) {
        return 0;
    }
    utf8 = (char*)xrtMalloc((size_t)size);
    if (utf8 == 0) {
        return 0;
    }
    if (WideCharToMultiByte(XL6_CP_UTF8, 0, text, -1, utf8, size, 0, 0) <= 0) {
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

    if (argv == 0) {
        return xl6_argv_to_arg(fallbackArgc, fallbackArgv);
    }
    arg = xvoCreateArray();
    if (arg == 0) {
        LocalFree(argv);
        return 0;
    }
    for (i = 0; i < argc; ++i) {
        char* text = xl6_wide_to_utf8(argv[i]);
        xvalue item = text != 0 ? xvoCreateText((void*)text, 0, 0) : 0;
        if (text != 0) {
            xrtFree(text);
        }
        if (item == 0 || !xvoArrayAppendValue(arg, item, 1)) {
            if (item != 0) {
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


/* The symbol module.c calls but does not define. External linkage so the
 * linker resolves the reference from the generated code. */
xvalue xl6_command_argv_to_arg(int argc, char** argv)
{
#if defined(_WIN32) || defined(_WIN64)
    return xl6_windows_argv_to_arg(argc, argv);
#else
    return xl6_argv_to_arg(argc, argv);
#endif
}
