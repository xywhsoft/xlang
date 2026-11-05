#include "include/xlc.h"
#include "src/common/xl6_xrt.h"
#include "foreign_xge3_data_object_cycle.h"
#include <assert.h>
#include <stdio.h>
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
static const char* engine;
static xui_lifetime_link_t watches[2];
static unsigned observed,closed;
static void invalidate(void* user){unsigned* count=user;assert(*count<2);++*count;}
int binding_data_cycle_observe(xui_context context)
{
    typedef int (*Watch)(xui_context,xui_lifetime_link_t*,xui_lifetime_invalidate_proc,void*);
    Watch watch=(Watch)address(engine,"xuiContextLifetimeWatch");assert(watch && observed<2 && context);
    int status=watch(context,&watches[observed],invalidate,&closed);if(status==XUI_OK)++observed;return status;
}
int main(int argc,char** argv)
{
    if(argc!=10)return 2;
    engine=argv[8];assert(!loaded(engine));XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir=argv[2];options.tempDir=argv[6];options.tccDllPath=argv[4];options.tccLibPath=argv[5];
    XL_Module* modules[2]={NULL,NULL};typedef int (*CheckedMain)(xvalue*,int*);CheckedMain entries[2]={NULL,NULL};
    for(unsigned i=0;i<2;++i){XLC_Context* compiler=xlcCreateContext(&options);assert(compiler);
        assert(xlcAddIncludePath(compiler,argv[2]) && xlcAddIncludePath(compiler,argv[3]) &&
            xlcAddIncludePath(compiler,argv[7]) && xlcAddIncludePath(compiler,argv[9]));
        assert(xlcRegisterCFunction(compiler,"binding_data_cycle_observe",(void*)binding_data_cycle_observe));
        if(!xlcAddSourceFile(compiler,argv[1]) || !xlcCompile(compiler)){
            fprintf(stderr,"Data cycle compile failed: %s\n",xlcGetLastError(compiler));return 3;}
        modules[i]=xlcRelocateModule(compiler);if(!modules[i]){fprintf(stderr,"Data cycle relocate failed: %s\n",xlcGetLastError(compiler));return 4;}
        entries[i]=(CheckedMain)xlModuleGetSymbol(modules[i],"xl_script_main_checked");assert(entries[i]);xlcDestroyContext(compiler);assert(loaded(engine));
    }
    for(unsigned i=0;i<2;++i){int code=1;if(!entries[i](NULL,&code) || code){fprintf(stderr,"Data cycle main failed: kind=%d code=%d\n",(int)xrtErrorKind(xrtGetError()),(int)xrtErrorCode(xrtGetError()));return 5;}assert(observed==i+1 && closed==0);}
    XL_Context* runner=xlCreateContext(&options);assert(runner);xlDestroyContext(runner);assert(closed==0 && loaded(engine));
    xlDestroyModule(modules[0]);XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(-1);int code=1;
    if(!drain.ready || !drain.ok || closed!=1 || !loaded(engine) || !entries[1](NULL,&code) || code || xrtGetError()){
        fprintf(stderr,"Data first unit: ready=%d ok=%d closed=%u loaded=%d\n",drain.ready,drain.ok,closed,loaded(engine));return 6;}
    xlDestroyModule(modules[1]);drain=xlRuntimeDrainRetiringChecked(-1);
    if(!drain.ready || !drain.ok || closed!=2 || loaded(engine) || xrtGetError()){
        fprintf(stderr,"Data last unit: ready=%d ok=%d closed=%u loaded=%d\n",drain.ready,drain.ok,closed,loaded(engine));return 7;}
    puts("two real Context -> Data -> provider -> Context cycles: compiler/runner destruction kept live globals, per-module unit closed each Context once, final checked drain unloaded Native engine");return 0;
}
