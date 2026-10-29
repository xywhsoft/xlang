/* Public source-JIT execution. Hooks observe generated catch/finally only. */
#include "xlc.h"
#include "release/include/xruntime_decl.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
static xfuture *bootFuture,*output;
static xfuture* nativeParent;
static atomic_int enteredCount,caughtCount,finallyCount,droppedCount;
static bool injectFailure;
static atomic_int poolStarted,poolRelease,poolFinally,poolDropped;
static xfuture* boot(void) { return xrtFutureRef(bootFuture); }
static xfuture* parentBoot(void) { assert(nativeParent);return xrtFutureRef(nativeParent); }
static bool hasParent(void) { return nativeParent!=NULL; }
static void capture(const xvalue* value) { assert(!output && xrtValueIsFuture(value));output=xrtFutureRef(xrtValueGetFuture(value));assert(output); }
static void entered(void) { ++enteredCount; }
static void caught(void) { ++caughtCount; }
static void finished(void) { ++finallyCount; }
static void dropped(void)
{
    assert(caughtCount==1 && finallyCount==1);++droppedCount;
    XL_RuntimeDrainResult reentry=xlRuntimeDrainRetiringChecked(0);assert(!reentry.ready && reentry.ok);
    if(injectFailure)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
}
static bool imageReleased(const void* address)
{
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION info;assert(VirtualQuery(address,&info,sizeof(info))==sizeof(info));return info.State==MEM_FREE;
#else
    FILE* maps=fopen("/proc/self/maps","r");assert(maps);char line[1024],permissions[5];
    unsigned long long first,last,target=(unsigned long long)(uintptr_t)address;bool released=true;
    while(fgets(line,sizeof(line),maps))if(sscanf(line,"%llx-%llx %4s",&first,&last,permissions)==3 && target>=first && target<last){released=permissions[2]!='x';break;}
    assert(!ferror(maps) && !fclose(maps));return released;
#endif
}
static void collect(void) { assert(xlRuntimeInit());xlRuntimeUnit();xrtThreadYield(); }
static long long poolWork(void)
{
    ++poolStarted;while(!poolRelease)xrtThreadYield();return 19;
}
static void poolWaitStarted(void)
{ double deadline=xrtTimer()+5;while(!poolStarted){assert(xrtTimer()<deadline);xrtThreadYield();} }
static void poolFinished(void) { assert(poolStarted==1);++poolFinally; }
static void poolDrop(void) { assert(poolFinally==1);++poolDropped; }
static void poolTransaction(char** args)
{
    poolStarted=poolRelease=poolFinally=poolDropped=0;assert(!output && xlRuntimeInit());
    XLC_ContextOptions options=xlcDefaultContextOptions();options.workDir=".";options.tempDir=args[5];options.tccDllPath=args[1];options.tccLibPath=args[2];
    XLC_Context* compiler=xlcCreateContext(&options);assert(compiler);
    assert(xlcRegisterCFunction(compiler,"pool_work",(void*)poolWork) && xlcRegisterCFunction(compiler,"pool_wait_started",(void*)poolWaitStarted) &&
        xlcRegisterCFunction(compiler,"pool_finally",(void*)poolFinished) && xlcRegisterCFunction(compiler,"pool_dropped",(void*)poolDrop) &&
        xlcRegisterCFunction(compiler,"group_capture",(void*)capture));
    assert(xlcAddIncludePath(compiler,args[3]) && xlcAddSourceFile(compiler,args[6]) && xlcCompile(compiler));
    XL_Module* module=xlcRelocateModule(compiler);assert(module);
    if(!xlRunModuleMain(xlcRuntimeContext(compiler),module,NULL)){
        fprintf(stderr,"pool-only main: %s\n",xlGetLastError(xlcRuntimeContext(compiler)));assert(0);
    }
    assert(output && poolStarted==1 && !poolFinally && !poolDropped && xrtFutureState(output)==XFUTURE_PENDING);
    const void* image=xlModuleGetSymbol(module,"xl_script_module_ownership_trace");assert(image && !imageReleased(image));
    xlDestroyModule(module);xlcDestroyContext(compiler);
    XL_RuntimeDrainResult pending=xlRuntimeDrainRetiringChecked(0);assert(!pending.ready && pending.ok && !xrtGetError());
    poolRelease=1;xrtFutureDestroy(output);output=NULL;
    XL_RuntimeDrainResult done=xlRuntimeDrainRetiringChecked(5000);
    if(!done.ready || !done.ok || xrtGetError())fprintf(stderr,"pool drain ready=%d ok=%d finally=%d dropped=%d error=%s\n",done.ready,done.ok,(int)poolFinally,(int)poolDropped,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");
    assert(done.ready && done.ok && !xrtGetError());
    assert(imageReleased(image) && poolFinally==1 && poolDropped==1);xlRuntimeUnit();
}
/* This native parent request has NO language CodeScope. Retiring while its
 * owned callback is paused must be BUSY, not a sticky semantic failure. */
typedef struct NativeCancel { volatile int32 refs; xatomic32 entered,release; xfuture* parent; } NativeCancel;
static bool nativeCount(const void* data,size_t* count)
{ const NativeCancel* value=data;if(!value || !count || value->refs<=0)return false;*count=(size_t)value->refs;return true; }
static bool nativeTrace(const void* data,xrtownershipvisitor visit,ptr context)
{ (void)data;(void)visit;(void)context;return true; }
static const xrtownershipops nativeOps={nativeCount,nativeTrace};
static void nativePause(ptr data)
{
    NativeCancel* value=data;xrtAtomic32Store(&value->entered,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&value->release,XMEMORY_ACQUIRE))xrtThreadYield();
}
static void nativeDrop(const void* data)
{
    NativeCancel* value=(NativeCancel*)data;xrtownershipscope scope={0};assert(xrtOwnershipMutationBegin(&scope));
    assert(!xrtRefRelease(&value->refs));assert(xrtOwnershipScopeEnd(&scope));
}
static const xcancelwatchownershipv1 nativePolicy={sizeof(nativePolicy),nativePause,nativeDrop,&nativeOps};
static int32 nativeRequest(ptr data)
{ NativeCancel* value=data;assert(xrtFutureCancel(value->parent) && !xrtGetError());return 0; }
static void nativeTransaction(char** args,bool oneShot)
{
    injectFailure=false;enteredCount=caughtCount=finallyCount=droppedCount=0;assert(!output && !bootFuture && xlRuntimeInit());
    xpromise* producer=xrtPromiseCreate(&bootFuture,NULL);xfuture* parent=NULL;
    xpromise* parentProducer=xrtPromiseCreate(&parent,NULL);assert(producer && bootFuture && parentProducer && parent && !nativeParent);nativeParent=parent;
    XLC_ContextOptions options=xlcDefaultContextOptions();options.workDir=".";options.tempDir=args[5];options.tccDllPath=args[1];options.tccLibPath=args[2];
    XLC_Context* compiler=xlcCreateContext(&options);assert(compiler);
    assert(xlcRegisterCFunction(compiler,"group_boot",(void*)boot) && xlcRegisterCFunction(compiler,"group_capture",(void*)capture) &&
        xlcRegisterCFunction(compiler,"group_entered",(void*)entered) && xlcRegisterCFunction(compiler,"group_caught",(void*)caught) &&
        xlcRegisterCFunction(compiler,"group_finally",(void*)finished) && xlcRegisterCFunction(compiler,"group_dropped",(void*)dropped) &&
        xlcRegisterCFunction(compiler,"group_parent",(void*)parentBoot) && xlcRegisterCFunction(compiler,"group_has_parent",(void*)hasParent));
    assert(xlcAddIncludePath(compiler,args[3]) && xlcAddSourceFile(compiler,args[4]) && xlcCompile(compiler));
    XL_Module* module=xlcRelocateModule(compiler);assert(module);
    assert(oneShot ? xlRunModuleMain(xlcRuntimeContext(compiler),module,NULL) : xlActivateModule(xlcRuntimeContext(compiler),module,NULL));
    if(!oneShot){void (*start)(void)=(void(*)(void))xlModuleGetSymbol(module,"group_start");assert(start);start();}
    assert(output);const void* image=xlModuleGetSymbol(module,"xl_script_module_ownership_trace");assert(image && !imageReleased(image));
    xvalue* value=xrtValueInt(1);assert(value && xrtPromiseResolve(producer,value));xrtValueRelease(value);
    xrtPromiseDestroy(producer);xrtFutureDestroy(bootFuture);bootFuture=NULL;double deadline=xrtTimer()+10;
    while(!enteredCount){assert(xrtTimer()<deadline);xrtThreadYield();}
    xcancel* token=xrtFutureCancelToken(output);assert(token);NativeCancel barrier={0};barrier.refs=1;barrier.parent=parent;
    xcancelwatch* watch=xrtCancelWatchOwnedV1(token,&barrier,&nativePolicy);assert(watch);
    xthread* thread=xrtThreadCreate(nativeRequest,&barrier,0);assert(thread);
    while(!xrtAtomic32Load(&barrier.entered,XMEMORY_ACQUIRE)){assert(xrtTimer()<deadline);xrtThreadYield();}
    xrtFutureDestroy(output);output=NULL; /* Only real in-flight native rights remain. */
    xlDestroyModule(module);xlcDestroyContext(compiler);
    XL_RuntimeDrainResult pending=xlRuntimeDrainRetiringChecked(0);
    if(pending.ready || !pending.ok)fprintf(stderr,"native parent pause drain ready=%d ok=%d error=%s\n",pending.ready,pending.ok,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");
    assert(!pending.ready && pending.ok && !xrtGetError() && !caughtCount && !finallyCount && !droppedCount && !imageReleased(image));
    xrtAtomic32Store(&barrier.release,1,XMEMORY_RELEASE);assert(xrtThreadWaitFor(thread,5000)==XWAIT_OK);xrtThreadDestroy(thread);
    assert(xrtCancelUnwatchChecked(watch) && !barrier.refs);xrtCancelDestroy(token);
    xrtFutureDestroy(output);output=NULL;xrtPromiseDestroy(parentProducer);xrtFutureDestroy(parent);nativeParent=NULL;
    XL_RuntimeDrainResult done=xlRuntimeDrainRetiringChecked(5000);
    if(!done.ready || !done.ok)fprintf(stderr,"native parent completion ready=%d ok=%d error=%s\n",done.ready,done.ok,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");
    assert(done.ready && done.ok && !xrtGetError() && imageReleased(image) && caughtCount==1 && finallyCount==1 && droppedCount==1);
    xlRuntimeUnit();
}
static void transaction(char** args,bool keepRoot,bool oneShot,bool failure)
{
    injectFailure=failure;
    enteredCount=caughtCount=finallyCount=droppedCount=0;assert(!bootFuture && !output && xlRuntimeInit());
    xpromise* producer=xrtPromiseCreate(&bootFuture,NULL);assert(producer && bootFuture);
    XLC_ContextOptions options=xlcDefaultContextOptions();options.workDir=".";options.tempDir=args[5];options.tccDllPath=args[1];options.tccLibPath=args[2];
    XLC_Context* compiler=xlcCreateContext(&options);assert(compiler);
    assert(xlcRegisterCFunction(compiler,"group_boot",(void*)boot) && xlcRegisterCFunction(compiler,"group_capture",(void*)capture) &&
        xlcRegisterCFunction(compiler,"group_entered",(void*)entered) && xlcRegisterCFunction(compiler,"group_caught",(void*)caught) &&
        xlcRegisterCFunction(compiler,"group_finally",(void*)finished) && xlcRegisterCFunction(compiler,"group_dropped",(void*)dropped) &&
        xlcRegisterCFunction(compiler,"group_parent",(void*)parentBoot) && xlcRegisterCFunction(compiler,"group_has_parent",(void*)hasParent));
    assert(xlcAddIncludePath(compiler,args[3]) && xlcAddSourceFile(compiler,args[4]));
    if(!xlcCompile(compiler)){fprintf(stderr,"%s\n",xlcGetLastError(compiler));assert(0);}
    XL_Module* module=xlcRelocateModule(compiler);
    if(!module){fprintf(stderr,"relocate: %s\n",xlcGetLastError(compiler));assert(0);}
    if(oneShot){assert(xlRunModuleMain(xlcRuntimeContext(compiler),module,NULL));}
    else if(!xlActivateModule(xlcRuntimeContext(compiler),module,NULL)){
        fprintf(stderr,"module activate: %s / %s\n",xlGetLastError(xlcRuntimeContext(compiler)),xrtGetError()?xrtErrorMessage(xrtGetError()):"no native error");assert(0);}
    if(!oneShot){void (*start)(void)=(void(*)(void))xlModuleGetSymbol(module,"group_start");assert(start);start();}
    if(xrtGetError())fprintf(stderr,"start: %s\n",xrtErrorMessage(xrtGetError()));
    assert(output);
    const void* image=xlModuleGetSymbol(module,"xl_script_module_ownership_trace");assert(image && !imageReleased(image));
    xvalue* value=xrtValueInt(1);assert(value && xrtPromiseResolve(producer,value));xrtValueRelease(value);
    xrtPromiseDestroy(producer);xrtFutureDestroy(bootFuture);bootFuture=NULL;
    double deadline=xrtTimer()+10;while(!enteredCount){assert(xrtTimer()<deadline);xrtThreadYield();}
    assert(!caughtCount && !finallyCount && xrtFutureState(output)==XFUTURE_PENDING);
    if(!keepRoot){xrtFutureDestroy(output);output=NULL;}
    xlDestroyModule(module);xlcDestroyContext(compiler);
    if(keepRoot){
        xerror* prior=xrtErrorCreate(XERR_STATE,"quiescence",0,"ambient error");assert(prior);xrtSetErrorTake(xrtErrorRef(prior));
        XL_RuntimeDrainResult pending=xlRuntimeDrainRetiringChecked(1);
        assert(!pending.ready && pending.ok && xrtGetError()==prior);xrtClearError();xrtErrorFree(prior);
        for(unsigned turn=0;turn<16;++turn)collect();
        assert(!caughtCount && !finallyCount && !droppedCount && !imageReleased(image));
        xrtFutureDestroy(output);output=NULL;
    }
    XL_RuntimeDrainResult drained=xlRuntimeDrainRetiringChecked(5000);
    assert(failure ? !drained.ok : drained.ok);
    if(failure){
        deadline=xrtTimer()+5;
        while(!drained.ready){assert(xrtTimer()<deadline);xrtClearError();drained=xlRuntimeDrainRetiringChecked(0);assert(!drained.ok);xrtThreadYield();}
        xrtClearError();
    }
    assert(drained.ready);
    deadline=xrtTimer()+10;
    while(!imageReleased(image) && !xrtGetError()){
        if(xrtTimer()>=deadline){fprintf(stderr,"retirement timeout entered=%d caught=%d finally=%d dropped=%d\n",enteredCount,caughtCount,finallyCount,droppedCount);assert(0);}
        collect();
    }
    xlRuntimeUnit();
    if(xrtGetError())fprintf(stderr,"retirement error %u/%d: %s\n",(unsigned)xrtErrorKind(xrtGetError()),(int)xrtErrorCode(xrtGetError()),xrtErrorMessage(xrtGetError()));
    assert(!xrtGetError() && imageReleased(image) && caughtCount==1 && finallyCount==1 && droppedCount==1);
}
int main(int argc,char** argv)
{
    assert(argc==7 && xrtMemDebugEnable(true));
    XL_RuntimeDrainResult invalid=xlRuntimeDrainRetiringChecked(-2);assert(!invalid.ready && !invalid.ok && xrtGetError() && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    xerror* prior=xrtErrorCreate(XERR_STATE,"drain",0,"ambient validation failure");assert(prior);xrtSetErrorTake(xrtErrorRef(prior));
    invalid=xlRuntimeDrainRetiringChecked(-2);assert(!invalid.ready && !invalid.ok && xrtGetError()==prior);xrtClearError();xrtErrorFree(prior);
    xl6_error_scope enclosing={0};assert(xl6ErrorScopeBegin(&enclosing));invalid=xlRuntimeDrainRetiringChecked(-2);
    assert(!invalid.ready && !invalid.ok && enclosing.failed);xl6_error_result rejected=xl6ErrorScopeEnd(&enclosing);
    assert(!rejected.ok && rejected.error && xrtErrorKind(rejected.error)==XERR_ARGUMENT);xrtErrorFree(rejected.error);xrtClearError();
    for(unsigned warm=0;warm<6;++warm)transaction(argv,(warm&1)!=0 || warm>=4,warm>=4 ? warm==5 : (warm&2)!=0,warm>=4);
    for(unsigned round=0;round<6;++round){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);transaction(argv,(round&1)!=0 || round>=4,round>=4 ? round==5 : (round&2)!=0,round>=4);double deadline=xrtTimer()+10;
        for(;;){xrtMemDebugSnapshot(&after);assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
            if(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount)break;
            assert(xrtTimer()<deadline);xrtThreadYield();}
    }
    poolTransaction(argv);poolTransaction(argv);
    for(unsigned round=0;round<4;++round){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);poolTransaction(argv);double deadline=xrtTimer()+10;
        for(;;){xrtMemDebugSnapshot(&after);assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
            if(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount)break;
            assert(xrtTimer()<deadline);xrtThreadYield();}
    }
    nativeTransaction(argv,false);nativeTransaction(argv,true);
    for(unsigned round=0;round<4;++round){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);nativeTransaction(argv,(round&1)!=0);double deadline=xrtTimer()+10;
        for(;;){xrtMemDebugSnapshot(&after);assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
            if(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount)break;
            assert(xrtTimer()<deadline);xrtThreadYield();}
    }
    puts("TaskGroup language: 24 real module lifetimes, native parent-cancel BUSY without language CodeScope, activated and one-shot cleanup, task-pool-only BUSY, checked drain/reentry, no-diagnostic finalizer failure preserved, external-output root preserved, generated catch/finally exactly once, code images retired, full ledger balanced");return 0;
}
