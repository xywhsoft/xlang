/* Context refresh and query-only lifetime gate; keep XRT DLL resident. */
#include "include/xlc.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(c) do { if (!(c)) { fprintf(stderr, "c_bridge_host:%d: %s\n", __LINE__, #c); exit(1); } } while (0)
#define BIND(n) do { FARPROC p = GetProcAddress(module, #n); REQUIRE(p); memcpy(&api.n, &p, sizeof(api.n)); } while (0)
static struct {
 XLC_ContextOptions (*xlcDefaultContextOptions)(void);
 XLC_Context* (*xlcCreateContext)(const XLC_ContextOptions*);
 void (*xlcDestroyContext)(XLC_Context*);
 int (*xlcAddSourceString)(XLC_Context*,const char*,const char*);
 int (*xlcAddSourceFile)(XLC_Context*,const char*);
 int (*xlcSetSourceOverlay)(XLC_Context*,const char*,const char*,size_t);
 int (*xlcClearSourceOverlays)(XLC_Context*);
 int (*xlcCompile)(XLC_Context*);
 int (*xlcCBridge)(XLC_Context*,const char*);
 int (*xlcEmitXlirHeader)(XLC_Context*,const char*,const char*);
 int (*xlcCheckWithSources)(XLC_Context*,const char*);
 const char* (*xlcGetLastError)(XLC_Context*);
} api;
static void writeSource(const char* text)
{
 FILE* f=fopen("disk.xl","wb");size_t n=strlen(text);
 REQUIRE(f);REQUIRE(fwrite(text,1,n,f)==n);REQUIRE(fclose(f)==0);
}
static char* utf8(const wchar_t* w)
{
 int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,NULL,0,NULL,NULL);
 char* p; REQUIRE(n>0); p=malloc((size_t)n); REQUIRE(p);
 REQUIRE(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,p,n,NULL,NULL));return p;
}
int wmain(int argc,wchar_t** argv)
{
 const char a[]="export \"memory_first\" func first(c_int n) as c_int\n return n\nend\n";
 const char b[]="export \"memory_second\" func second(c_double n) as c_double\n return n\nend\n";
 const char invalid[]="func broken() as int\n return unknown\nend\n";
 HMODULE module; XLC_ContextOptions options; XLC_Context *ctx,*other;char* directory;
 REQUIRE(argc==3 || argc==4);module=LoadLibraryW(argv[1]);REQUIRE(module);
 if (argc==4) { REQUIRE(!GetProcAddress(module,"xlcCBridge"));REQUIRE(!GetProcAddress(module,"xlcCreateContext"));puts("runtime-only DLL correctly has no compiler query API");return 0; }
 REQUIRE(SetCurrentDirectoryW(argv[2]));
 BIND(xlcDefaultContextOptions);BIND(xlcCreateContext);BIND(xlcDestroyContext);
 BIND(xlcAddSourceString);BIND(xlcAddSourceFile);BIND(xlcSetSourceOverlay);BIND(xlcClearSourceOverlays);
 BIND(xlcCompile);BIND(xlcCBridge);BIND(xlcEmitXlirHeader);BIND(xlcCheckWithSources);BIND(xlcGetLastError);
 REQUIRE(!api.xlcCBridge(NULL,NULL));directory=utf8(argv[2]);options=api.xlcDefaultContextOptions();options.workDir=directory;
 ctx=api.xlcCreateContext(&options);REQUIRE(ctx);REQUIRE(!api.xlcCBridge(ctx,"unconfigured.json"));
 REQUIRE(api.xlcAddSourceString(ctx,"memory.xl",a));REQUIRE(api.xlcCompile(ctx));
 REQUIRE(api.xlcCBridge(ctx,"first.json"));
 REQUIRE(api.xlcEmitXlirHeader(ctx,"first.h","XL6_MODULE_H"));
 REQUIRE(api.xlcSetSourceOverlay(ctx,"memory.xl",b,sizeof(b)-1));
 REQUIRE(api.xlcCBridge(ctx,"authoritative.json"));
 REQUIRE(api.xlcAddSourceFile(ctx,"virtual.xl"));
 REQUIRE(api.xlcSetSourceOverlay(ctx,"virtual.xl",b,sizeof(b)-1));
 REQUIRE(api.xlcCBridge(ctx,"virtual.json"));
 REQUIRE(api.xlcSetSourceOverlay(ctx,"virtual.xl",invalid,sizeof(invalid)-1));
 REQUIRE(!api.xlcCBridge(ctx,"invalid.json"));REQUIRE(*api.xlcGetLastError(ctx));
 REQUIRE(api.xlcSetSourceOverlay(ctx,"virtual.xl",a,sizeof(a)-1));
 REQUIRE(api.xlcCBridge(ctx,"recovered.json"));
 REQUIRE(!api.xlcCBridge(ctx,"absent-parent/query.json"));REQUIRE(*api.xlcGetLastError(ctx));
 REQUIRE(api.xlcClearSourceOverlays(ctx));REQUIRE(!api.xlcCBridge(ctx,"removed.json"));
 REQUIRE(api.xlcAddSourceString(ctx,"memory.xl",b));REQUIRE(api.xlcCBridge(ctx,"replaced.json"));
 other=api.xlcCreateContext(&options);REQUIRE(other);
 REQUIRE(api.xlcAddSourceString(other,"memory.xl",a));REQUIRE(api.xlcCBridge(other,"other.json"));
 REQUIRE(api.xlcCBridge(ctx,"unchanged.json"));REQUIRE(api.xlcCheckWithSources(ctx,"legacy.json"));
 writeSource(a);REQUIRE(api.xlcAddSourceFile(ctx,"disk.xl"));REQUIRE(api.xlcCompile(ctx));
 REQUIRE(api.xlcCBridge(ctx,"disk-before.json"));writeSource(b);
 REQUIRE(api.xlcCBridge(ctx,"disk-changed.json"));
 api.xlcDestroyContext(other);api.xlcDestroyContext(ctx);free(directory);
 puts("c_bridge_host passed");return 0;
}
