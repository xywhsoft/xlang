#include "src/runtime/xl6_process_live.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_future_payload.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_value_admission.h"
#include <assert.h>
#include <stdio.h>
static const char* child;
static void frozen(xrtownershipscope* scope)
{ double deadline=xrtTimer()+5;while(!xrtOwnershipFreezeTryBegin(scope)){assert(xrtTimer()<deadline);xrtThreadYield();} }
static XL6_OwnershipCatalog* catalog(void)
{
    const XL6_OwnershipDomainV1* domains[]={
#define XL6_OWNERSHIP_DOMAIN_ENTRY(Producer, Component, Getter) Getter(),
#include "src/runtime/xl6_ownership_domains.inc"
#undef XL6_OWNERSHIP_DOMAIN_ENTRY
    };return xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));
}
static bool resolve(xrtownershipref ref,void* data,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** finish)
{ return xl6OwnershipCatalogResolveV1(ref,data,adapter,preparation,finish); }
static bool admit(xrtownershipref ref,void* data)
{
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finish=NULL;
    return resolve(ref,data,&adapter,&preparation,&finish);
}
static void balanced(const xmemdebugsnapshot* before)
{
    /* Observe the actual native waiter/thread release tail, not a sleep or a
     * discounted allocator ledger. The ordinary Process API does not join it. */
    double deadline=xrtTimer()+5;xmemdebugsnapshot after;xrtClearError();
    for(;;){xrtMemDebugSnapshot(&after);
        assert(after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
            after.UseAfterFreeCount==before->UseAfterFreeCount);
        if(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
            after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount)break;
        assert(xrtTimer()<deadline);xrtThreadYield();
    }
}
static XL6_ProcessLive* spawn(void)
{
    xvalue* args=xrtValueArray();assert(args && xrtValueArrayAppendNew(args,xrtValueString(XRT_STR_LITERAL("echo"))));
    void* command=xl6ProcessCommandDirectV1((xstrview){child,strlen(child)},args,-1,-1);xrtValueRelease(args);assert(command);
    /* Last-user release closes every parent pipe. This child must not write
     * stderr after EOF and turn a correct close into POSIX SIGPIPE. */
    ((XL6_ProcessCommand*)command)->config.Stdout.Mode=XPROCESS_IO_NULL;
    ((XL6_ProcessCommand*)command)->config.Stderr.Mode=XPROCESS_IO_NULL;
    XL6_ProcessLive* process=xl6ProcessSpawnV1(command);xl6ProcessCommandDestroyV1(command);assert(process);return process;
}
static void snapshot(XL6_OwnershipCatalog* cat,xvalue* value,size_t expected,bool accepted)
{
    xrtownershipref root=xrtValueOwnership(value);xrtownershipsnapshot* graph=NULL;xrtownershipscope freeze={0};frozen(&freeze);
    bool ok=xrtOwnershipSnapshotCreate(&root,1,NULL,0,admit,cat,&graph);assert(ok==accepted);
    if(ok)assert(graph && xrtOwnershipSnapshotNodeCount(graph)==expected);else assert(!graph);
    xrtOwnershipSnapshotDestroy(graph);assert(xrtOwnershipScopeEnd(&freeze));xrtClearError();
}
static void legacy(const xfutureresult* input,xpromise* output,void* context)
{ (void)input;(void)context;(void)xrtPromiseClose(output); }
static void graph(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_OwnershipCatalog* cat=catalog();assert(cat);
    XL6_ProcessLive* process=spawn();void* raw=process;xvalue* value=xl6NativeHandleTakeV1(&raw,xl6ProcessHandlePolicyV1Get());assert(value && !raw);
    snapshot(cat,value,3,true); /* handle, shared wrapper, native process */
    xfuture* shared=xrtProcessWaitAsync(process->native);assert(shared);
    xfuture* ready1=xl6ProcessWaitReadyAsyncV1(process),*ready2=xl6ProcessWaitReadyAsyncV1(process);assert(ready1 && ready2);
    snapshot(cat,value,11,true); /* exact physical frames, shared states and tokens */
    xfuture* opaque=xrtFutureThen(shared,legacy,NULL);assert(opaque);snapshot(cat,value,0,false);
    assert(xrtFutureCancel(ready1) && !xrtFutureDone(ready2) && xl6ProcessRunningV1(process));
    assert(xl6ProcessCloseV1(process,XPROCESS_STDIN));assert(xrtFutureWaitFor(shared,UINT64_C(5000000))==XWAIT_OK);
    assert(xrtFutureWaitFor(ready1,UINT64_C(5000000))==XWAIT_OK && xrtFutureWaitFor(ready2,UINT64_C(5000000))==XWAIT_OK);
    assert(xrtFutureState(ready1)==XFUTURE_CANCELLED && xrtFutureState(ready2)==XFUTURE_RESOLVED);
    assert(*(const bool*)xrtFutureValue(ready2));
    /* A completion can still be retiring its Watch. Admit only the actual
     * stable graph, without assuming that EXITED or FutureDone joined it. */
    xrtownershipref root=xrtValueOwnership(value);double deadline=xrtTimer()+5;
    for(;;){xrtownershipscope freeze={0};frozen(&freeze);xrtownershipsnapshot* graph=NULL;
        bool ok=xrtOwnershipSnapshotCreate(&root,1,NULL,0,admit,cat,&graph);
        if(ok){assert(xrtOwnershipSnapshotNodeCount(graph)==5);xrtOwnershipSnapshotDestroy(graph);}
        assert(xrtOwnershipScopeEnd(&freeze));xrtClearError();if(ok)break;
        assert(xrtTimer()<deadline);xrtThreadYield();
    }
    xrtValueRelease(value);assert(((const xprocessstatus*)xrtFutureValue(shared))->Code==0);
    xrtFutureDestroy(opaque);xrtFutureDestroy(ready1);xrtFutureDestroy(ready2);xrtFutureDestroy(shared);
    xl6OwnershipCatalogDestroyV1(cat);balanced(&before);
    puts("process graph: 3/11/5 physical nodes; opaque legacy observer refused; local cancellation and shared result preserved");
}
static void cycle(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_OwnershipCatalog* cat=catalog();assert(cat);
    XL6_ProcessLive* process=spawn();xprocess* native=process->native;xrtownershipref nativeRef=xrtProcessOwnership(native);
    xrtownershipscope freeze={0};frozen(&freeze);const xrtownershipadapterv1* nativeAdapter=xrtProcessOwnershipAdapterV1(nativeRef);assert(nativeAdapter && nativeAdapter->Hold(native));
    assert(xrtOwnershipScopeEnd(&freeze));xfuture* terminal=xrtProcessWaitAsync(native);assert(terminal);
    void* raw=process;xvalue* owned=xl6NativeHandleTakeV1(&raw,xl6ProcessHandlePolicyV1Get());assert(owned && !raw);
    const uint64_t identity=UINT64_C(0x99110001),element=UINT64_C(0x99110002);
    xvalue* ring=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_STACK_V1,identity,element,XL6_VALUE_ADMIT_ANY,false,element);assert(ring);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(ring,XL6_SEQUENCE_STACK_V1,identity,element,3,sizeof(xvalue*),xrtTypeValue());assert(pin);
    assert(xl6SequenceAdmitSlotV1(pin,&ring) && xrtTypedArrayPush(xl6SequenceStorageV1(pin),&ring));
    assert(xl6SequenceAdmitSlotV1(pin,&owned) && xrtTypedArrayPush(xl6SequenceStorageV1(pin),&owned));
    xl6SequenceEndV1(pin);xrtValueRelease(owned);
    xrtValueRelease(ring); /* The self edge is now its only real owning slot. */
    XL6_OwnershipPlan* plan=NULL;XL6_OwnershipPlanResult result;double deadline=xrtTimer()+5;
    do{result=xl6OwnershipPlanTryCreateV3(xrtValueOwnership(ring),resolve,cat,&plan);if(result==XL6_OWNERSHIP_PLAN_BUSY){assert(xrtTimer()<deadline);xrtThreadYield();}}while(result==XL6_OWNERSHIP_PLAN_BUSY);
    assert(result==XL6_OWNERSHIP_PLAN_READY && plan);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
    do{result=xl6OwnershipPlanTryCommit(plan);if(result==XL6_OWNERSHIP_PLAN_BUSY){assert(xrtTimer()<deadline);xrtThreadYield();}}while(result==XL6_OWNERSHIP_PLAN_BUSY);
    assert(result==XL6_OWNERSHIP_PLAN_READY && process->cleared && process->native==native);
    assert(xrtProcessState(native)==XPROCESS_RUNNING && !xrtFutureDone(terminal)); /* Clear did not close stdin. */
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xrtFutureWaitFor(terminal,UINT64_C(5000000))==XWAIT_OK && ((const xprocessstatus*)xrtFutureValue(terminal))->Code==0);
    nativeAdapter->Drop(native);xrtFutureDestroy(terminal);xl6OwnershipCatalogDestroyV1(cat);balanced(&before);
    puts("process cycle: finalization before Clear; EOF only at Finish; actual shared result and release ledger balanced");
}
static void prefixes(void)
{
    unsigned point;
    for(point=0;point<128;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ProcessLive* process=spawn();
        xfuture* shared=xrtProcessWaitAsync(process->native);assert(shared);
        assert(xrtMemDebugFailAfter(point));xfuture* ready=xl6ProcessWaitReadyAsyncV1(process);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert(hit?!ready:ready!=NULL);xrtClearError();
        if(!ready)ready=xl6ProcessWaitReadyAsyncV1(process);
        assert(ready);xl6ProcessDestroyV1(process);
        assert(xrtFutureWaitFor(ready,UINT64_C(5000000))==XWAIT_OK && xrtFutureState(ready)==XFUTURE_RESOLVED && *(const bool*)xrtFutureValue(ready));
        assert(xrtFutureWaitFor(shared,UINT64_C(5000000))==XWAIT_OK);
        xrtFutureDestroy(ready);xrtFutureDestroy(shared);balanced(&before);if(!hit)break;
    }
    assert(point>0 && point<128);printf("process certified continuation prefixes=%u first-no-hit=%u balanced\n",point+1,point);
}
int main(int argc,char** argv)
{
    assert(argc==2 && xrtMemDebugEnable(true));child=argv[1];setvbuf(stdout,NULL,_IONBF,0);
    xrtSetErrorKind(XERR_MEMORY);xrtClearError();graph();cycle();prefixes();assert(!xrtGetError());return 0;
}
