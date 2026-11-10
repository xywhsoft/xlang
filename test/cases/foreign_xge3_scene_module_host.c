#include "include/xlc.h"
#include <xrt_decl.h>
#include <assert.h>
#include <stdio.h>
#include "foreign_xge3_scene_module.h"
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
static int ticks;
static int callbacks;
int fixtureSceneModuleTick(void){return ++ticks;}
int fixtureSceneModuleCallbacks(void){return callbacks;}
int main(int argc,char** argv)
{
    assert(argc==8);callbacks=argv[6][0]=='1';const char* engine=argv[5];assert(!loaded(engine));
    XLC_ContextOptions options=xlcDefaultContextOptions();options.workDir=argv[2];options.tempDir=argv[4];
    XLC_Context* compiler=xlcCreateContext(&options);assert(compiler);
    assert(xlcAddIncludePath(compiler,argv[2]) && xlcAddIncludePath(compiler,argv[3]) && xlcAddIncludePath(compiler,argv[7]));
    assert(xlcRegisterCFunction(compiler,"fixtureSceneModuleTick",(void*)fixtureSceneModuleTick));
    assert(xlcRegisterCFunction(compiler,"fixtureSceneModuleCallbacks",(void*)fixtureSceneModuleCallbacks));
    if(!xlcAddSourceFile(compiler,argv[1]) || !xlcCompile(compiler)){
        fprintf(stderr,"Scene module compile failed: %s\n",xlcGetLastError(compiler));return 3;
    }
    XL_Module* module=xlcRelocateModule(compiler);
    if(!module){fprintf(stderr,"Scene module relocate failed: %s\n",xlcGetLastError(compiler));return 4;}
    typedef int (*CheckedMain)(xvalue*,int*);
    CheckedMain entry=(CheckedMain)xlModuleGetSymbol(module,"xl_script_main_checked");assert(entry);
    xlcDestroyContext(compiler);int code=1;assert(entry(NULL,&code) && !code && loaded(engine));
    typedef int (*Operation)(void);
    Operation frame=(Operation)address(engine,"xgeFrame"),clear=(Operation)address(engine,"xgeSceneClear"),unit=(Operation)address(engine,"xgeUnitChecked");
    assert(frame && clear && unit && frame()==1 && ticks==callbacks);
    xlDestroyModule(module);
    assert(loaded(engine));
    /* The stack's real owning edge keeps the callback, captures, module and
     * native image alive after host Close. Existing accepted capability only. */
    assert(frame()==1 && ticks==2*callbacks && !xrtGetError());
    assert(clear()==0 && unit()==0);
    XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(5000);
    if(!drain.ready || !drain.ok || loaded(engine) || xrtGetError()){
        fprintf(stderr,"Scene final drain: ready=%d ok=%d loaded=%d kind=%d\n",drain.ready,drain.ok,loaded(engine),(int)xrtErrorKind(xrtGetError()));return 5;
    }
    puts("Scene stack kept real module code through Close; cycle collected and native image unloaded after clear");return 0;
}
