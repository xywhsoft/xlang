#include "include/xlc.h"
#include <xrt_decl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "foreign_xge3_init_module.h"
#if defined(_WIN32)
#include <windows.h>
static int loaded(const char* path){return GetModuleHandleA(path)!=NULL;}
static void* address(const char* path,const char* name)
{HMODULE h=GetModuleHandleA(path);return h ? (void*)GetProcAddress(h,name):NULL;}
#else
#include <dlfcn.h>
static int loaded(const char* path)
{void* h=dlopen(path,RTLD_NOLOAD|RTLD_NOW);if(!h)return 0;dlclose(h);return 1;}
static void* address(const char* path,const char* name)
{void* h=dlopen(path,RTLD_NOLOAD|RTLD_NOW);if(!h)return NULL;void* p=dlsym(h,name);dlclose(h);return p;}
#endif
static int mode;
int fixtureInitModuleMode(void){return mode;}
int main(int argc,char** argv)
{
 assert(argc==8);mode=argv[6][0]-'0';const char* engine=argv[5];assert(!loaded(engine));
 XLC_ContextOptions options=xlcDefaultContextOptions();options.workDir=argv[2];options.tempDir=argv[4];
 XLC_Context* compiler=xlcCreateContext(&options);assert(compiler);
 assert(xlcAddIncludePath(compiler,argv[2]) && xlcAddIncludePath(compiler,argv[3]) && xlcAddIncludePath(compiler,argv[7]));
 assert(xlcRegisterCFunction(compiler,"fixtureInitModuleMode",(void*)fixtureInitModuleMode));
 if(!xlcAddSourceFile(compiler,argv[1]) || !xlcCompile(compiler)){
  fprintf(stderr,"Init module compile failed: %s\n",xlcGetLastError(compiler));return 3;
 }
 XL_Module* module=xlcRelocateModule(compiler);
 if(!module){fprintf(stderr,"Init module relocate failed: %s\n",xlcGetLastError(compiler));return 4;}
 typedef int (*CheckedMain)(xvalue*,int*);
 CheckedMain entry=(CheckedMain)xlModuleGetSymbol(module,"xl_script_main_checked");assert(entry);
 xlcDestroyContext(compiler);int code=1;assert(entry(NULL,&code) && !code && loaded(engine));
 typedef int (*Operation)(void);
 typedef const char* (*Title)(void);
 typedef uint64 (*Count)(void);
 Operation flush=(Operation)address(engine,"xgeFlush"),unit=(Operation)address(engine,"xgeUnitChecked");
 Title title=(Title)address(engine,"xgeGetTitle");Count count=(Count)address(engine,"xgeGraphicsOwnedResourceCount");
 assert(flush && unit && title && count);
 xlDestroyModule(module);assert(loaded(engine));
 assert(title() && !strcmp(title(),mode==2 ? "XGE":mode==1 ? "owned engine 1":"owned engine 0"));
 assert(count()==(mode==1 ? 1u:0u));
 /* Explicitly flush here; two-phase shutdown is a separate design decision. */
 assert(flush()==0 && !count());assert(unit()==0);
 XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(5000);
 if(!drain.ready || !drain.ok || loaded(engine) || xrtGetError()){
  fprintf(stderr,"Init final drain: ready=%d ok=%d loaded=%d kind=%d\n",drain.ready,drain.ok,loaded(engine),(int)xrtErrorKind(xrtGetError()));return 5;
 }
 puts("Engine retained code after module Close; pending work completed; final checked shutdown and drain unloaded the image");return 0;
}
