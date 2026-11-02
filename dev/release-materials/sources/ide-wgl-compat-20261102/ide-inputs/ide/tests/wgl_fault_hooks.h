#ifndef XIDE_WGL_FAULT_HOOKS_H
#define XIDE_WGL_FAULT_HOOKS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
typedef struct xide_wgl_test_stats { int major,minor,requests,created,deleted; } xide_wgl_test_stats;
HMODULE WINAPI xideWglLoadLibraryA(LPCSTR name);
HMODULE WINAPI xideWglLoadLibraryExA(LPCSTR name,HANDLE file,DWORD flags);
FARPROC WINAPI xideWglGetProcAddress(HMODULE module,LPCSTR name);
#ifdef XIDE_WGL_FAULT_IMPORTS
#define LoadLibraryA xideWglLoadLibraryA
#define LoadLibraryExA xideWglLoadLibraryExA
#define GetProcAddress xideWglGetProcAddress
#endif
#endif
