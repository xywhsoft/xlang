/* A real TinyCC module owns its engine DLL references after its compiler
 * context ends. The final checked retirement must release those references. */
#include "include/xlc.h"
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
static int loaded(const char* path){return GetModuleHandleA(path)!=NULL;}
#else
#include <dlfcn.h>
static int loaded(const char* path)
{void* h=dlopen(path,RTLD_NOLOAD|RTLD_NOW);if(!h)return 0;dlclose(h);return 1;}
#endif
int main(int argc,char** argv)
{
    if(argc!=9)return 2;
    XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir=argv[2];options.tempDir=argv[6];options.tccDllPath=argv[4];options.tccLibPath=argv[5];
    XLC_Context* compiler=NULL;XL_Context* runner=NULL;XL_Module* modules[2]={NULL,NULL};int ok=0;
    for(unsigned i=0;i<2;++i){
        compiler=xlcCreateContext(&options);if(!compiler)goto done;
        if(!xlcAddIncludePath(compiler,argv[3]) || !xlcAddIncludePath(compiler,argv[7]))goto done;
        if(!xlcAddSourceFile(compiler,argv[1]) || !xlcCompile(compiler))goto done;
        modules[i]=xlcRelocateModule(compiler);if(!modules[i])goto done;
        xlcDestroyContext(compiler);compiler=NULL;
        if(!loaded(argv[8])){fputs("engine unloaded before module execution\n",stderr);goto done;}
    }
    runner=xlCreateContext(&options);if(!runner)goto done;
    ok=xlRunModuleMain(runner,modules[0],NULL) && !xlGetLastExitCode(runner) &&
        xlRunModuleMain(runner,modules[1],NULL) && !xlGetLastExitCode(runner);
done:
    if(!ok)fprintf(stderr,"package module failed: %s\n",compiler ? xlcGetLastError(compiler) : runner ? xlGetLastError(runner) : "context creation failed");
    for(unsigned i=0;i<2;++i)if(modules[i])xlDestroyModule(modules[i]);
    if(runner)xlDestroyContext(runner);
    if(compiler)xlcDestroyContext(compiler);
    XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(-1);
    if(!drain.ready || !drain.ok || loaded(argv[8])){
        fputs("checked module retirement failed or engine references remain\n",stderr);ok=0;
    }
    if(ok)puts("package module outlived compiler; engine unloaded after checked retirement");
    return ok ? 0:1;
}
