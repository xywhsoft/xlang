/* Real public compiler/API + genuine TCC relocation and retirement. The
 * recovery functions live in resident providers, not the source image. */
#include "xl.h"
#include "xl_sync_retirement.h"
#include "xruntime_decl.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
static void repair(unsigned mode)
{
    XL_SyncRetirementV1 row; size_t count=0;
    assert(xlSyncRetirementSnapshotCheckedV1(&row,1,&count) && count==1);
    xrtClearError();
    assert(!xlSyncRetirementRetryCheckedV1(row.token));
    xrtClearError();
    xvalue* handle=NULL; assert(xlSyncRetirementAcquireCheckedV1(row.token,&handle) && handle);
    assert(xlSyncRetirementUnlockCheckedV1(handle,mode));
    assert(xlSyncRetirementRetryCheckedV1(row.token)); xrtValueRelease(handle);
    assert(xlSyncRetirementSnapshotCheckedV1(NULL,0,&count) && count==0);
}
static XL_Context* create(const char* root,const char* headers,const char* temporary)
{
    XL_ContextOptions options=xlDefaultContextOptions();
    char runtime[4096],libraries[4096];
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
    assert(argc==7);
    unsigned mode=(unsigned)strtoul(argv[5],NULL,10);
    bool global=strtoul(argv[6],NULL,10)!=0;
    assert(xrtMemDebugEnable(true) && xlRuntimeInit());
    XL_Context* context=create(argv[1],argv[2],argv[3]);
    assert(xlRunScriptString(context,"assert true\n")); xlDestroyContext(context);
    xrtClearError(); xl6ProductMemoryBegin(SIZE_MAX);
    context=create(argv[1],argv[2],argv[3]);
    if (global) {
        XL_Module* module=xlCompileScriptFileToModule(context,argv[4]);
        if(!module) fprintf(stderr,"compile failed: %s\n",xlGetLastError(context));
        assert(module && xlActivateModule(context,module,NULL));
        assert(!xlDeactivateModule(context,module) && xlGetLastError(context));
        repair(mode); xlClearLastError(context);
        assert(xlDeactivateModule(context,module));
        xlDestroyModule(module); xlDestroyContext(context);
    } else {
        XL_Module* module=xlCompileScriptFileToModule(context,argv[4]);
        if(!module) fprintf(stderr,"compile failed: %s\n",xlGetLastError(context));
        assert(module);
        void* entry=xlModuleGetSymbol(module,"xl_script_main_checked"); assert(entry);
        assert(!xlRunModuleMain(context,module,NULL) && xlGetLastError(context));
        if(strstr(argv[4],"primary")) assert(strstr(xlGetLastError(context),"sync original assertion"));
        xlClearLastError(context); assert(xlDeactivateModule(context,module));
        xlDestroyModule(module); xlDestroyContext(context);
#ifdef _WIN32
        MEMORY_BASIC_INFORMATION information;
        assert(VirtualQuery(entry,&information,sizeof(information))==sizeof(information));
        assert(information.State==MEM_FREE);
#endif
        /* The genuine relocated image has been released before recovery. */
        repair(mode);
    }
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    xlRuntimeUnit();
    puts("actual JIT held cleanup: checked module failure, explicit repair/retry and lifetimes balanced");
    return 0;
}
