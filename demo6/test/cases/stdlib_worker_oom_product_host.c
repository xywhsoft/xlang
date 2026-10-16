#include "xl.h"
#include "xruntime_decl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
void workerProbeConfigure(int64_t,uint64_t);
int64_t workerProbeCase(void);
uint64_t workerProbeBudget(void);
bool workerProbeStart(void);
bool workerProbeFinish(bool);
bool workerProbeCompleted(void);
bool workerProbeTriggered(void);
void workerProbeDescribe(void);
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    if(before->LiveCount!=after.LiveCount || before->LiveBytes!=after.LiveBytes)
        fprintf(stderr,"worker ledger live=%zu/%zu bytes=%zu/%zu\n",before->LiveCount,after.LiveCount,before->LiveBytes,after.LiveBytes);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->DoubleFreeCount==after.DoubleFreeCount && before->InvalidFreeCount==after.InvalidFreeCount && before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static bool invoke(XL_Context* context,XL_Module* module,int64_t kind,uint64_t budget,bool ledger)
{
    workerProbeConfigure(kind,budget);xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    int ok=xlRunModuleMain(context,module,NULL);
    if(!ok){
        fprintf(stderr,"worker main: case=%lld budget=%llu %s\n",(long long)kind,(unsigned long long)budget,xlGetLastError(context));
        workerProbeDescribe();
    }
    assert(ok && xlGetLastExitCode(context)==0 && !xrtGetError());
    bool hit=workerProbeTriggered();if(ledger)balanced(&before);return hit;
}
static void transaction(const char* runtime,const char* source,const char* temporary,bool prefixes,int64_t selected)
{
    assert(xlRuntimeInit());XL_ContextOptions options=xlDefaultContextOptions();
    options.workDir=".";options.tempDir=temporary;options.tccDllPath=runtime;
    XL_Context* context=xlCreateContext(&options);assert(context);
#define REGISTER(Name) assert(xlRegisterCFunction(context,#Name,(void*)Name))
    REGISTER(workerProbeCase);REGISTER(workerProbeBudget);REGISTER(workerProbeStart);REGISTER(workerProbeFinish);REGISTER(workerProbeCompleted);
#undef REGISTER
    XL_Module* module=xlCompileScriptFileToModule(context,source);
    if(!module)fprintf(stderr,"worker compile: %s\n",xlGetLastError(context));
    assert(module);
    for(int64_t kind=0;kind<10;++kind){
        if(selected>=0 && kind!=selected)continue;
        assert(!invoke(context,module,kind,UINT64_MAX,false));
        assert(invoke(context,module,kind,0,false));
    }
    for(int64_t kind=0;prefixes && kind<10;++kind){
        if(selected>=0 && kind!=selected)continue;
        uint64_t point=0;
        for(;point<512;++point)if(!invoke(context,module,kind,point,true))break;
        assert(point>0 && point<512);
        assert(!invoke(context,module,kind,point+1,true));
        printf("worker-complete-prefix case=%lld failures=%llu first-no-hit=%llu confirmed=%llu balanced=1\n",
            (long long)kind,(unsigned long long)point,(unsigned long long)point,(unsigned long long)(point+1));fflush(stdout);
    }
    xlDestroyModule(module);xlDestroyContext(context);
    XL_RuntimeDrainResult drained=xlRuntimeDrainRetiringChecked(5000);
    assert(drained.ready && drained.ok && !xrtGetError());xlRuntimeUnit();assert(!xrtGetError());
}
int main(int argc,char** argv)
{
    assert((argc==4 || argc==5) && xrtMemDebugEnable(true));
    int64_t selected=argc==5 ? strtoll(argv[4],NULL,10) : -1;
    assert(selected>=-1 && selected<10);
    transaction(argv[1],argv[2],argv[3],false,selected);
    transaction(argv[1],argv[2],argv[3],false,selected);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    transaction(argv[1],argv[2],argv[3],true,selected);balanced(&before);
    printf("worker product: %u complete worker-body OOM prefixes, MemoryError/finally, caller isolation, retained XRT completion and balanced owners\n",selected<0 ? 10u : 1u);
    return 0;
}
