#include "include/xlc.h"
#include "src/common/xl6_xrt.h"
#include "foreign_xge3_package_tail.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
static int loaded(const char* name){return GetModuleHandleA(name)!=NULL;}
static void* address(const char* name,const char* symbol)
{HMODULE h=GetModuleHandleA(name);return h ? (void*)GetProcAddress(h,symbol):NULL;}
#else
#include <dlfcn.h>
static int loaded(const char* name)
{void* h=dlopen(name,RTLD_NOLOAD|RTLD_NOW);if(!h)return 0;dlclose(h);return 1;}
static void* address(const char* name,const char* symbol)
{void* h=dlopen(name,RTLD_NOLOAD|RTLD_NOW);if(!h)return NULL;void* p=dlsym(h,symbol);dlclose(h);return p;}
#endif
static XL_Module* module;
static TailControl control;
static int (*dispatch)(TailControl*);
static volatile unsigned requested,workerResult;
void tail_request_close(void)
{
    XL_Module* closing=module;assert(closing);module=NULL;
    xlDestroyModule(closing);__atomic_store_n(&requested,1,__ATOMIC_RELEASE);
}
static int32 worker(void* unused)
{(void)unused;__atomic_store_n(&workerResult,(unsigned)dispatch(&control),__ATOMIC_RELEASE);return 0;}
int main(int argc,char** argv)
{
    if(argc!=9)return 2;
    XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir=argv[2];options.tempDir=argv[6];options.tccDllPath=argv[4];options.tccLibPath=argv[5];
    XLC_Context* compiler=xlcCreateContext(&options);assert(compiler);
    assert(xlcAddIncludePath(compiler,argv[2]) && xlcAddIncludePath(compiler,argv[3]) && xlcAddIncludePath(compiler,argv[7]));
    assert(xlcRegisterCFunction(compiler,"tail_request_close",(void*)tail_request_close));
    if(!xlcAddSourceFile(compiler,argv[1]) || !xlcCompile(compiler)){
        fprintf(stderr,"native tail fixture compile failed: %s\n",xlcGetLastError(compiler));
        xlcDestroyContext(compiler);return 3;
    }
    module=xlcRelocateModule(compiler);
    if(!module){fprintf(stderr,"native tail relocation failed: %s\n",xlcGetLastError(compiler));xlcDestroyContext(compiler);return 4;}
    xlcDestroyContext(compiler);
    XL_Context* runner=xlCreateContext(&options);assert(runner);
    assert(xlRunModuleMain(runner,module,NULL) && !xlGetLastExitCode(runner));xlDestroyContext(runner);
    assert(loaded(argv[8]));dispatch=(int(*)(TailControl*))address(argv[8],"tail_dispatch");assert(dispatch);
    xthread* thread=xrtThreadCreate(worker,NULL,0);assert(thread);
    double deadline=xrtTimer()+10.0;
    while(__atomic_load_n(&control.phase,__ATOMIC_ACQUIRE)!=1){assert(xrtTimer()<deadline);xrtThreadYield();}
    assert(__atomic_load_n(&requested,__ATOMIC_ACQUIRE) && !module && loaded(argv[8]));
    /* Exercise automatic collection on another thread while the DLL's real
     * return tail is still running. This is not an explicit quiescence claim. */
    XL_Context* probe=xlCreateContext(&options);assert(probe);xlDestroyContext(probe);
    assert(loaded(argv[8]));
    __atomic_store_n(&control.release,1,__ATOMIC_RELEASE);
    assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
    assert(__atomic_load_n(&control.phase,__ATOMIC_ACQUIRE)==2 && !__atomic_load_n(&workerResult,__ATOMIC_ACQUIRE));
    assert(loaded(argv[8])); /* No automatic boundary may guess global quiescence. */
    XL_RuntimeDrainResult drained=xlRuntimeDrainRetiringChecked(-1);
    int remains=loaded(argv[8]);
    if(!drained.ready || !drained.ok || remains || xrtGetError()){
        fprintf(stderr,"tail final drain: ready=%d ok=%d publisherLoaded=%d errorKind=%d\n",drained.ready,drained.ok,remains,(int)xrtErrorKind(xrtGetError()));
        return 5;
    }
    puts("native publisher continued after final owner Drop; join established quiescence; explicit drain unloaded DLL");
    return 0;
}
