/* Test-only driver faults at the actual Win32/WGL producer boundary.
   This object and the forced-include hooks never enter the production DLL. */
#include "wgl_fault_hooks.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static xide_wgl_test_stats stats;
static HGLRC dummy,actual;
typedef HGLRC (WINAPI *attrib_proc)(HDC,HGLRC,const int*);
static attrib_proc realAttrib;
static int mode(const char* name) { const char* m=getenv("XIDE_WGL_FAULT");return m && !strcmp(m,name); }
static HGLRC WINAPI create(HDC dc)
{
 if(mode("dummy-create")) {SetLastError(ERROR_GEN_FAILURE);return NULL;}
 dummy=wglCreateContext(dc);if(dummy)++stats.created;return dummy;
}
static BOOL WINAPI destroy(HGLRC rc) {BOOL okay=wglDeleteContext(rc);if(okay)++stats.deleted;return okay;}
static BOOL WINAPI current(HDC dc,HGLRC rc)
{
 if(rc && ((mode("dummy-current") && rc==dummy) || (mode("real-current") && rc==actual))) {SetLastError(ERROR_GEN_FAILURE);return FALSE;}
 return wglMakeCurrent(dc,rc);
}
static HGLRC WINAPI attributes(HDC dc,HGLRC share,const int* attrs)
{
 ++stats.requests;
 for(const int* p=attrs;*p;p+=2) {if(p[0]==0x2091)stats.major=p[1];if(p[0]==0x2092)stats.minor=p[1];}
 fprintf(stderr,"WGL-TEST requested=%d.%d\n",stats.major,stats.minor);fflush(stderr);
 if(mode("create") || stats.major>3 || (stats.major==3 && stats.minor>3)) {SetLastError(0xc0072095u);return NULL;}
 actual=realAttrib(dc,share,attrs);if(actual)++stats.created;return actual;
}
static PROC WINAPI extension(LPCSTR name)
{
 PROC p=wglGetProcAddress(name);
 if(!strcmp(name,"wglCreateContextAttribsARB")) {
  if(mode("missing-attrib"))return NULL;
  realAttrib=(attrib_proc)p;return (PROC)attributes;
 }
 return p;
}
HMODULE WINAPI xideWglLoadLibraryA(LPCSTR name)
{if(!_stricmp(name,"opengl32.dll") && mode("missing-library")){SetLastError(ERROR_MOD_NOT_FOUND);return NULL;}return LoadLibraryA(name);}
HMODULE WINAPI xideWglLoadLibraryExA(LPCSTR name,HANDLE file,DWORD flags)
{if(!_stricmp(name,"opengl32.dll") && mode("missing-library")){SetLastError(ERROR_MOD_NOT_FOUND);return NULL;}return LoadLibraryExA(name,file,flags);}
FARPROC WINAPI xideWglGetProcAddress(HMODULE module,LPCSTR name)
{
 if((ULONG_PTR)name>65535) {
  if(!strcmp(name,"wglGetProcAddress"))return mode("missing-export") ? NULL : (FARPROC)extension;
  if(!strcmp(name,"wglCreateContext"))return (FARPROC)create;
  if(!strcmp(name,"wglDeleteContext"))return (FARPROC)destroy;
  if(!strcmp(name,"wglMakeCurrent"))return (FARPROC)current;
 }
 return GetProcAddress(module,name);
}
__declspec(dllexport) xide_wgl_test_stats xideWglTestStats(void) {return stats;}
