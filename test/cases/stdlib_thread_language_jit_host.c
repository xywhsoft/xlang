/* Genuine public compiler, TCC relocation, language execution and retirement.
 * This does not certify dlclose/OS-thread join or arbitrary closure graphs. */
#include "xl.h"
#include "xruntime_decl.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
static XL_Context* create(const char* root,const char* headers,const char* temporary)
{
    XL_ContextOptions options=xlDefaultContextOptions(); char runtime[4096],libraries[4096];
#ifdef _WIN32
    int n=snprintf(runtime,sizeof(runtime),"%s/release/xl.dll",root);
    int m=snprintf(libraries,sizeof(libraries),"%s/res/tcc/win64/x86_64",root);
#else
    const char* native=getenv("XL6_SOURCE_NATIVE_ROOT"); assert(native && *native);
    int n=snprintf(runtime,sizeof(runtime),"%s/release/libxl.so",native);
    int m=snprintf(libraries,sizeof(libraries),"%s/build/linux/tcc-dist",native);
#endif
    assert(n>0 && (size_t)n<sizeof(runtime) && m>0 && (size_t)m<sizeof(libraries));
    options.workDir=root; options.tempDir=temporary; options.tccDllPath=runtime; options.tccLibPath=libraries;
    XL_Context* context=xlCreateContext(&options); assert(context);
    assert(xlAddIncludePath(context,headers)); return context;
}
int main(int argc,char** argv)
{
    assert(argc==5); assert(xrtMemDebugEnable(true) && xlRuntimeInit());
    XL_Context* context=create(argv[1],argv[2],argv[3]);
    assert(xlRunScriptString(context,"assert true\n")); xlDestroyContext(context); xrtClearError();
    xl6ProductMemoryBegin(SIZE_MAX); context=create(argv[1],argv[2],argv[3]);
    XL_Module* module=xlCompileScriptFileToModule(context,argv[4]);
    if (!module) fprintf(stderr,"compile failed: %s\n",xlGetLastError(context));
    assert(module);
    void* entry=xlModuleGetSymbol(module,"xl_script_main_checked"); assert(entry);
    bool ok=xlRunModuleMain(context,module,NULL)!=0;
    if (!ok) fprintf(stderr,"run failed: %s\n",xlGetLastError(context));
    assert(ok && !xlGetLastExitCode(context));
    assert(xlDeactivateModule(context,module)); xlDestroyModule(module); xlDestroyContext(context);
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION information;
    assert(VirtualQuery(entry,&information,sizeof(information))==sizeof(information));
    assert(information.State==MEM_FREE);
#else
    (void)entry;
#endif
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); xlRuntimeUnit();
    puts("actual Thread JIT: source image retired and lifetimes balanced"); return 0;
}
