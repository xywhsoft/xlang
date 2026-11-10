#include "include/xlc.h"
#include <xrt_decl.h>
#include <assert.h>
#include <stdio.h>
#include "foreign_xge3_graphics_context_module.h"
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
static const char* engine;
static xge_graphics_context retained;
int fixtureContextModuleMode(void){return mode;}
int fixtureContextModulePublish(xge_graphics_context context)
{
 typedef int (*Share)(xge_graphics_context,xge_graphics_context*);
 Share share=(Share)address(engine,"xgeGraphicsContextShareOwned");
 assert(share && !retained);return share(context,&retained);
}
int main(int argc,char** argv)
{
 assert(argc==8);mode=argv[6][0]-'0';engine=argv[5];assert(!loaded(engine));
 XLC_ContextOptions options=xlcDefaultContextOptions();options.workDir=argv[2];options.tempDir=argv[4];
 XLC_Context* compiler=xlcCreateContext(&options);assert(compiler);
 assert(xlcAddIncludePath(compiler,argv[2]) && xlcAddIncludePath(compiler,argv[3]) && xlcAddIncludePath(compiler,argv[7]));
 assert(xlcRegisterCFunction(compiler,"fixtureContextModuleMode",(void*)fixtureContextModuleMode));
 assert(xlcRegisterCFunction(compiler,"fixtureContextModulePublish",(void*)fixtureContextModulePublish));
 if(!xlcAddSourceFile(compiler,argv[1]) || !xlcCompile(compiler)){
  fprintf(stderr,"Context module compile failed: %s\n",xlcGetLastError(compiler));return 3;
 }
 XL_Module* module=xlcRelocateModule(compiler);
 if(!module){fprintf(stderr,"Context module relocate failed: %s\n",xlcGetLastError(compiler));return 4;}
 typedef int (*CheckedMain)(xvalue*,int*);
 CheckedMain entry=(CheckedMain)xlModuleGetSymbol(module,"xl_script_main_checked");assert(entry);
 xlcDestroyContext(compiler);int code=1;assert(entry(NULL,&code) && !code && retained && loaded(engine));
 typedef int (*Info)(xge_graphics_context,xge_graphics_context_info_t*);
 typedef int (*Current)(xge_graphics_context);
 typedef int (*Release)(xge_graphics_context,bool*);
 typedef int (*Operation)(void);
 Info info=(Info)address(engine,"xgeGraphicsContextInfoGet");
 Current current=(Current)address(engine,"xgeGraphicsContextMakeCurrent");
 Release release=(Release)address(engine,"xgeGraphicsContextReleaseOwnerChecked");
 Operation unit=(Operation)address(engine,"xgeUnitChecked");assert(info && current && release && unit);
 xlDestroyModule(module);assert(loaded(engine));
 xge_graphics_context_info_t state={0};state.size=sizeof(state);
 assert(info(retained,&state)==XGE_OK && state.current && state.graphicsReady);
 assert(current(retained)==XGE_OK && unit()==XGE_ERROR_BUSY && !xrtGetError());
 bool consumed=false;int status=release(retained,&consumed);
 assert(status==XGE_OK && consumed);retained=NULL;
 XL_RuntimeDrainResult first=xlRuntimeDrainRetiringChecked(0);
 int unitStatus=unit();
 fprintf(stderr,"Context release: mode=%d ready=%d ok=%d unit=%d\n",mode,first.ready,first.ok,unitStatus);
 assert(unitStatus==XGE_OK);
 XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(5000);
 if(!drain.ready || !drain.ok || loaded(engine) || xrtGetError()){
  fprintf(stderr,"Context final drain: ready=%d ok=%d loaded=%d kind=%d\n",drain.ready,drain.ok,loaded(engine),(int)xrtErrorKind(xrtGetError()));return 5;
 }
 puts("Owning context kept real module code through Close; native/global graph retired and image unloaded");return 0;
}
