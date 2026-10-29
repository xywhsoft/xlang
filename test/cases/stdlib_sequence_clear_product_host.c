/* Real shared-product compilation, explicit callback injection and complete
 * module/context retirement. No native provider or generated C is replaced. */
#include "xl.h"
#include "xruntime_decl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void clearProbeInstallV1(xvalue*);
int64_t clearProbeDropsV1(void);
void clearProbeFinishV1(void);

static void transaction(const char* runtime,const char* source,const char* temporary)
{
    assert(xlRuntimeInit());
    XL_ContextOptions options=xlDefaultContextOptions();
    options.workDir="."; options.tempDir=temporary; options.tccDllPath=runtime;
    XL_Context* context=xlCreateContext(&options);assert(context);
    assert(xlRegisterCFunction(context,"clearProbeInstallV1",(void*)clearProbeInstallV1));
    assert(xlRegisterCFunction(context,"clearProbeDropsV1",(void*)clearProbeDropsV1));
    assert(xlRegisterCFunction(context,"clearProbeFinishV1",(void*)clearProbeFinishV1));
    XL_Module* module=xlCompileScriptFileToModule(context,source);
    if(!module){fprintf(stderr,"clear compile: %s\n",xlGetLastError(context));assert(module);}
    int ok=xlRunModuleMain(context,module,NULL);
    if(!ok)fprintf(stderr,"clear main: %s\n",xlGetLastError(context));
    assert(ok && xlGetLastExitCode(context)==0 && !xrtGetError());
    xlDestroyModule(module);xlDestroyContext(context);
    XL_RuntimeDrainResult drained=xlRuntimeDrainRetiringChecked(5000);
    if(!drained.ready || !drained.ok)fprintf(stderr,"clear drain: ready=%d ok=%d error=%s\n",drained.ready,drained.ok,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");
    assert(drained.ready && drained.ok && !xrtGetError());
    xlRuntimeUnit();
    assert(!xrtGetError());
}
int main(int argc,char** argv)
{
    assert(argc==4);assert(xrtMemDebugEnable(true));
    transaction(argv[1],argv[2],argv[3]);transaction(argv[1],argv[2],argv[3]);
    for(unsigned round=0;round<25;++round){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
        transaction(argv[1],argv[2],argv[3]);xrtMemDebugSnapshot(&after);
        if(before.LiveCount!=after.LiveCount || before.LiveBytes!=after.LiveBytes)
            fprintf(stderr,"clear round=%u live=%llu->%llu bytes=%llu->%llu\n",round,
                (unsigned long long)before.LiveCount,(unsigned long long)after.LiveCount,
                (unsigned long long)before.LiveBytes,(unsigned long long)after.LiveBytes);
        assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
            before.InvalidFreeCount==after.InvalidFreeCount &&
            before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
    }
    puts("clear product: 25 complete language lifecycles, first error and async/generator cleanup, memory balanced");
    return 0;
}
