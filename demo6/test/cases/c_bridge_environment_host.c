/* Exact builtin bytes, overlay/disk precedence and context environment gate. */
#include "include/xlc.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(c) do { if (!(c)) { fprintf(stderr,"c_bridge_environment_host:%d: %s\n",__LINE__,#c);exit(1); } } while(0)
#define BIND(n) do { FARPROC p=GetProcAddress(module,#n);REQUIRE(p);memcpy(&api.n,&p,sizeof(api.n)); } while(0)
static struct {
 XLC_ContextOptions (*xlcDefaultContextOptions)(void);
 XLC_Context* (*xlcCreateContext)(const XLC_ContextOptions*);
 void (*xlcDestroyContext)(XLC_Context*);
 int (*xlcAddSourceString)(XLC_Context*,const char*,const char*);
 int (*xlcAddCSourceFile)(XLC_Context*,const char*);
 int (*xlcAddIncludePath)(XLC_Context*,const char*);
 int (*xlcCBridge)(XLC_Context*,const char*);
 int (*xlcCBridgeWithEnvironment)(XLC_Context*,const char*,int);
 int (*tcc_vfs_read_builtin)(const char*,void*,size_t,size_t*);
 int (*tcc_vfs_mount_memory)(const char*,const void*,size_t);
 void (*tcc_vfs_clear_dynamic)(void);
 int (*tcc_vfs_open)(const char*,int,...);
 int (*tcc_vfs_read)(int,void*,size_t);
 int (*tcc_vfs_close)(int);
} api;
static char* utf8(const wchar_t* w)
{
 int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,NULL,0,NULL,NULL);char* p;
 REQUIRE(n>0);p=malloc((size_t)n);REQUIRE(p);REQUIRE(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w,-1,p,n,NULL,NULL));return p;
}
static void observed(const char* expected)
{
 char buffer[32]={0};int fd=api.tcc_vfs_open("include/stddef.h",0);REQUIRE(fd>=0);
 REQUIRE(api.tcc_vfs_read(fd,buffer,sizeof(buffer))==(int)strlen(expected));REQUIRE(!strcmp(buffer,expected));REQUIRE(api.tcc_vfs_close(fd)==0);
}
int wmain(int argc,wchar_t** argv)
{
 HMODULE module;char* directory;XLC_ContextOptions options;XLC_Context* ctx;
 size_t size=0,n=0;unsigned char *first,*second;FILE* file;unsigned int i;
 const char source[]="export \"env_add\" func add(c_int a,c_int b) as c_int\n return a+b\nend\n";
 REQUIRE(argc==3);module=LoadLibraryW(argv[1]);REQUIRE(module);REQUIRE(SetCurrentDirectoryW(argv[2]));
 BIND(xlcDefaultContextOptions);BIND(xlcCreateContext);BIND(xlcDestroyContext);BIND(xlcAddSourceString);
 BIND(xlcAddCSourceFile);BIND(xlcAddIncludePath);BIND(xlcCBridge);BIND(xlcCBridgeWithEnvironment);
 BIND(tcc_vfs_read_builtin);BIND(tcc_vfs_mount_memory);BIND(tcc_vfs_clear_dynamic);BIND(tcc_vfs_open);BIND(tcc_vfs_read);BIND(tcc_vfs_close);
 REQUIRE(!api.xlcCBridgeWithEnvironment(NULL,NULL,0));
 REQUIRE(api.tcc_vfs_read_builtin("include/stddef.h",NULL,0,&size) && size>32);
 first=malloc(size);second=malloc(size);REQUIRE(first && second);
 REQUIRE(api.tcc_vfs_read_builtin("include/stddef.h",first,size,&n) && n==size);
 REQUIRE(!api.tcc_vfs_read_builtin("include/stddef.h",second,size-1,&n) && n==size);
 REQUIRE(!api.tcc_vfs_read_builtin("include/missing-exact.h",NULL,0,&n) && !n);
 REQUIRE(!api.tcc_vfs_read_builtin("D:/suffix/include/stddef.h",NULL,0,&n) && !n);
 REQUIRE(!api.tcc_vfs_read_builtin("include/stddef.h",NULL,1,&n));
 REQUIRE(CreateDirectoryW(L"include",NULL));file=fopen("include/stddef.h","wb");REQUIRE(file);
 REQUIRE(fwrite("disk",1,4,file)==4 && fclose(file)==0);observed("disk");
 REQUIRE(api.tcc_vfs_mount_memory("include/stddef.h","dynamic",7));observed("dynamic");
 REQUIRE(api.tcc_vfs_read_builtin("./include/../include/STDDEF.H",second,size,&n) && n==size && !memcmp(first,second,size));
 directory=utf8(argv[2]);options=api.xlcDefaultContextOptions();options.workDir=directory;ctx=api.xlcCreateContext(&options);REQUIRE(ctx);
 REQUIRE(api.xlcAddSourceString(ctx,"environment-memory.xl",source));REQUIRE(!api.xlcCBridgeWithEnvironment(ctx,"invalid-kind.json",2));
 REQUIRE(api.xlcCBridgeWithEnvironment(ctx,"environment-exe.json",0));
 REQUIRE(api.xlcAddCSourceFile(ctx,"unread-native.c"));
 REQUIRE(api.xlcCBridgeWithEnvironment(ctx,"environment-native.json",0));
 REQUIRE(api.xlcCBridgeWithEnvironment(ctx,"environment-shared.json",1));
 REQUIRE(api.xlcAddSourceString(ctx,"environment-memory.xl","func broken() as int\n return unknown\nend\n"));
 REQUIRE(!api.xlcCBridgeWithEnvironment(ctx,"environment-invalid.json",0));
 REQUIRE(api.xlcAddSourceString(ctx,"environment-memory.xl",source));
 for(i=0;i<2049;++i) {char name[64];snprintf(name,sizeof(name),"include-config-%u",i);REQUIRE(api.xlcAddIncludePath(ctx,name));}
 REQUIRE(!api.xlcCBridgeWithEnvironment(ctx,"environment-limit.json",0));REQUIRE(api.xlcCBridge(ctx,"plain-after-limit.json"));
 api.xlcDestroyContext(ctx);free(directory);api.tcc_vfs_clear_dynamic();observed("disk");
 REQUIRE(api.tcc_vfs_read_builtin("include/stddef.h",second,size,&n) && n==size && !memcmp(first,second,size));
 free(first);free(second);puts("c_bridge_environment_host passed");return 0;
}
