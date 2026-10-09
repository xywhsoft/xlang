/* Genuine product API/TCC and embedded headers only. The resident product
 * remains loaded until process exit: Thread.wait is NOT an OS-thread join. */
#include "xl.h"
#include "xruntime_decl.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#ifndef THREAD_SHIPPING_PRODUCT
#error Select the actual resident shipping product
#endif
static XL_Context* create(const char* root,const char* temporary)
{
    XL_ContextOptions options=xlDefaultContextOptions(); char runtime[4096];
    int n=snprintf(runtime,sizeof(runtime),"%s/release/" THREAD_SHIPPING_PRODUCT,root);
    assert(n>0 && (size_t)n<sizeof(runtime));
    options.workDir=root; options.tempDir=temporary;
    options.tccDllPath=runtime; options.tccLibPath=NULL;
    XL_Context* context=xlCreateContext(&options); assert(context); return context;
}
int main(int argc,char** argv)
{
    assert(argc==4 && xrtMemDebugEnable(true) && xlRuntimeInit());
    XL_Context* context=create(argv[1],argv[2]);
    assert(xlRunScriptString(context,"assert true\n")); xlDestroyContext(context); xrtClearError();
    xl6ProductMemoryBegin(SIZE_MAX); context=create(argv[1],argv[2]);
    XL_Module* module=xlCompileScriptFileToModule(context,argv[3]);
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
    puts("shipping Thread JIT: embedded VFS, source image retired and lifetimes balanced"); return 0;
}
