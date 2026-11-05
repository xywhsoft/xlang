/* Real script capture -> native Manager -> owned callback -> script capture.
 * The non-owning watch observes logical invalidation. Checked drain and final
 * DLL unloading independently verify completion of physical retirement. */
#include "include/xlc.h"
#include "src/common/xl6_xrt.h"
#include "foreign_xge3_binding_manager_cycle.h"
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
static unsigned observed,destroyed;
static void invalidate(void* user)
{unsigned* count=user;assert(*count<2);++*count;}
int binding_manager_cycle_observe(xui_msgbox manager)
{
    typedef int (*Watch)(xui_msgbox,xui_lifetime_link_t*,xui_lifetime_invalidate_proc,void*);
    Watch watch=(Watch)address(engine,"xuiMsgBoxLifetimeWatch");
    assert(watch && observed<2 && manager);
    int status=watch(manager,&watches[observed],invalidate,&destroyed);
    if(status==XUI_OK)++observed;
    return status;
}
int main(int argc,char** argv)
{
    if(argc!=10)return 2;
    engine=argv[8];assert(!loaded(engine));
    XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir=argv[2];options.tempDir=argv[6];options.tccDllPath=argv[4];options.tccLibPath=argv[5];
    XL_Module* modules[2]={NULL,NULL};
    typedef int (*CheckedMain)(xvalue*,int*);
    CheckedMain entries[2]={NULL,NULL};
    for(unsigned i=0;i<2;++i){
        XLC_Context* compiler=xlcCreateContext(&options);assert(compiler);
        assert(xlcAddIncludePath(compiler,argv[2]) && xlcAddIncludePath(compiler,argv[3]) &&
            xlcAddIncludePath(compiler,argv[7]) && xlcAddIncludePath(compiler,argv[9]));
        assert(xlcRegisterCFunction(compiler,"binding_manager_cycle_observe",(void*)binding_manager_cycle_observe));
        if(!xlcAddSourceFile(compiler,argv[1]) || !xlcCompile(compiler)){
            fprintf(stderr,"script cycle compile failed: %s\n",xlcGetLastError(compiler));return 3;
        }
        if(i==0){
            char path[1200];int length=snprintf(path,sizeof(path),"%s-source.c",argv[6]);
            assert(length>0 && (size_t)length<sizeof(path) && xlcEmitXlirC(compiler,path));
        }
        modules[i]=xlcRelocateModule(compiler);
        if(!modules[i]){fprintf(stderr,"script cycle relocation failed: %s\n",xlcGetLastError(compiler));return 4;}
        entries[i]=(CheckedMain)xlModuleGetSymbol(modules[i],"xl_script_main_checked");assert(entries[i]);
        xlcDestroyContext(compiler);
        assert(loaded(engine));
    }
    for(unsigned i=0;i<2;++i){
        int code=1;
        /* xlRunModuleMain is explicitly a one-shot operation which clears
         * globals on return. The exported checked entry instead preserves the
         * language lifetime; it supplies its own CodeActivity protection. */
        if(!entries[i](NULL,&code) || code){
            fprintf(stderr,"script cycle execution failed: native error kind=%d code=%d\n",(int)xrtErrorKind(xrtGetError()),(int)xrtErrorCode(xrtGetError()));return 5;
        }
        assert(observed==i+1 && destroyed==0);
    }
    XL_Context* runner=xlCreateContext(&options);assert(runner);xlDestroyContext(runner);
    assert(observed==2 && destroyed==0 && loaded(engine));
    xlDestroyModule(modules[0]);
    XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(-1);
    int probeCode=1;int probe=entries[1](NULL,&probeCode) && !probeCode;
    if(!probe)fprintf(stderr,"surviving module retained Manager read failed: native error kind=%d\n",(int)xrtErrorKind(xrtGetError()));
    if(!probe || !drain.ready || !drain.ok || destroyed!=1 || !loaded(engine) || xrtGetError()){
        fprintf(stderr,"first script cycle retirement: ready=%d ok=%d destroyed=%u loaded=%d\n",drain.ready,drain.ok,destroyed,loaded(engine));return 6;
    }
    xlDestroyModule(modules[1]);drain=xlRuntimeDrainRetiringChecked(-1);
    if(!drain.ready || !drain.ok || destroyed!=2 || loaded(engine) || xrtGetError()){
        fprintf(stderr,"last script cycle retirement: ready=%d ok=%d destroyed=%u loaded=%d\n",drain.ready,drain.ok,destroyed,loaded(engine));return 7;
    }
    puts("two actual script Manager callback cycles survived compiler destruction; another module's cleanup preserved the live global; explicit final cleanup collected both once and unloaded native code");
    return 0;
}
