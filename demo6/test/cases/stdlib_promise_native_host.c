#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_promise.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>

static XL6_FutureValueProfileV1 scalar(void)
{return (XL6_FutureValueProfileV1){sizeof(XL6_FutureValueProfileV1),XL6_FUTURE_VALUE_TRIVIAL,sizeof(int64_t),217,NULL};}
static XL6_FutureValueProfileV1 text(void)
{return (XL6_FutureValueProfileV1){sizeof(XL6_FutureValueProfileV1),XL6_FUTURE_VALUE_STRING,sizeof(xstrview),218,NULL};}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->DoubleFreeCount==after.DoubleFreeCount && before->InvalidFreeCount==after.InvalidFreeCount);
    assert(before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void prefixes(void)
{
    for(int mode=0;mode<2;++mode) {
        void* parent=xl6CancellationSourceCreateV1(); assert(parent);
        bool complete=false;
        for(size_t point=0;point<64;++point) {
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            xl6_foreign_error_scope scope={0};assert(xl6ForeignCallBegin(&scope));
            assert(xrtMemDebugFailAfter(point));
            void* raw=mode?xl6PromiseCreateParentV1(scalar(),parent):xl6PromiseCreateV1(scalar());
            xvalue* handle=raw?xl6NativeHandleTakeV1(&raw,xl6PromiseHandlePolicyV1Get()):NULL;
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            xl6_foreign_error_result result=xl6ForeignCallEnd(&scope);
            assert(!raw && (hit?!handle && !result.ok:handle && result.ok));
            xrtErrorFree(result.error);xrtClearError();xrtValueRelease(handle);
            assert(!xrtGetError());balanced(&before);
            if(!hit){printf("Promise factory prefix mode %d: %zu\n",mode,point);complete=true;break;}
        }
        assert(complete);xl6CancellationSourceDestroyV1(parent);
    }
    bool complete=false;
    for(size_t point=0;point<32;++point) {
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        void* raw=xl6PromiseCreateV1(text());assert(raw);
        xfuture* consumer=xl6PromiseFutureV1(text(),raw);assert(consumer);
        xl6_foreign_error_scope scope={0};assert(xl6ForeignCallBegin(&scope));
        xstrview input={"a\0b",3};assert(xrtMemDebugFailAfter(point));
        bool ok=xl6PromiseResolveV1(text(),raw,&input);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        xl6_foreign_error_result result=xl6ForeignCallEnd(&scope);
        assert(hit?!ok && !result.ok:ok && result.ok);
        xrtErrorFree(result.error);xrtClearError();
        if(hit){assert(!xl6PromiseDoneV1(raw));assert(xl6PromiseResolveV1(text(),raw,&input));}
        assert(xrtFutureDone(consumer));
        const xstrview* output=xrtFutureValue(consumer);assert(output && output->Size==3);
        assert(output->Data[0]=='a' && output->Data[1]==0 && output->Data[2]=='b');
        xl6PromiseDestroyV1(raw);xrtFutureDestroy(consumer);assert(!xrtGetError());balanced(&before);
        if(!hit){printf("Promise string completion prefix: %zu\n",point);complete=true;break;}
    }
    assert(complete);
}
static bool collect(xrtownershipref ref,void* output){*(xrtownershipref*)output=ref;return true;}
static void pinnedOwner(unsigned mode)
{
    void* raw=xl6PromiseCreateV1(scalar());assert(raw);
    xfuture* consumer=xl6PromiseFutureV1(scalar(),raw);assert(consumer);
    void* wrapped=raw;xvalue* handle=xl6NativeHandleTakeV1(&wrapped,xl6PromiseHandlePolicyV1Get());assert(handle);
    xrtownershipref state={0};assert(xl6PromiseHandlePolicyV1Get()->Trace(handle,collect,&state));
    const xrtownershipfinishv1* completion=NULL;
    const xrtownershipadapterv1* adapter=xl6PromiseOwnershipAdapterV1(state,&completion);assert(adapter && completion);
    int token;xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(adapter->Hold(state.Data));
    if(mode)assert(adapter->Claim(state.Data,&token));
    if(mode==2)adapter->Clear(state.Data,&token);
    assert(xrtOwnershipScopeEnd(&freeze));
    xrtValueRelease(handle);
    assert(!xrtGetError());
    if(mode==0)assert(xrtFutureDone(consumer)); // ROOTED plan pin is not a producer.
    else if(mode==1) {
        assert(!xrtFutureDone(consumer));assert(xrtOwnershipFreezeTryBegin(&freeze));
        adapter->Restore(state.Data,&token);assert(xrtOwnershipScopeEnd(&freeze));
    } else assert(completion->Step(state.Data,&token)==XRT_OWNERSHIP_FINISH_READY);
    adapter->Drop(state.Data);assert(xrtFutureDone(consumer));xrtFutureDestroy(consumer);
}
static XL6_OwnershipCatalog* catalog(void)
{
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainPromiseV1Get(),
        xl6OwnershipDomainNativeFutureV1Get(),xl6OwnershipDomainFuturePayloadV1Get()};
    return xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));
}
static void cycle(bool abortPlan)
{
    XL6_FutureValueProfileV1 profile={sizeof(profile),XL6_FUTURE_VALUE_XVALUE,sizeof(xvalue*),219,NULL};
    void* raw=xl6PromiseCreateV1(profile);assert(raw);void* state=raw;
    xvalue* handle=xl6NativeHandleTakeV1(&raw,xl6PromiseHandlePolicyV1Get());assert(handle);
    xvalue* root=xrtValueObject();assert(root);
    assert(xrtValueObjectSet(root,(xstrview){"producer",8},handle));xrtValueRelease(handle);
    assert(xl6PromiseResolveV1(profile,state,&root));
    xvalue* weak=xrtValueWeakRef(root);assert(weak);
    XL6_OwnershipCatalog* policies=catalog();assert(policies);XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV6(xrtValueOwnership(root),xl6OwnershipCatalogProbeV1,NULL,false,
        policies,&plan)==XL6_OWNERSHIP_PLAN_ROOTED && plan);
    xrtValueRelease(root);assert(xl6OwnershipPlanTryRefresh(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    if(abortPlan) {
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(!xrtValueWeakRefExpired(weak));
        assert(xrtValueRetain(root));
        /* Detach before Drop, matching language post-commit field writes.
         * A payload can release the enclosing object reentrantly. */
        xvalue* detached=xrtValueObjectTake(root,(xstrview){"producer",8});assert(detached);
        xrtValueRelease(detached);
        xrtValueRelease(root);
    } else {
        assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    }
    assert(xrtValueWeakRefExpired(weak));xrtValueRelease(weak);xl6OwnershipCatalogDestroyV1(policies);
}
typedef struct Reentry {void* producer;unsigned notified,released;} Reentry;
static bool traceEmpty(const void* raw,xrtownershipvisitor visitor,void* context)
{(void)raw;(void)context;return visitor!=NULL;}
static void notified(void* raw)
{
    Reentry* value=raw;int64_t duplicate=999;
    assert(!xl6PromiseResolveV1(scalar(),value->producer,&duplicate));
    ++value->notified;xl6PromiseDestroyV1(value->producer);value->producer=NULL;
}
static void released(void* raw){++((Reentry*)raw)->released;}
static void reentry(void)
{
    Reentry state={.producer=xl6PromiseCreateV1(scalar())};assert(state.producer);
    xfuture* consumer=xl6PromiseFutureV1(scalar(),state.producer);assert(consumer);
    xfuturewatch watch={0};assert(xrtFutureWatchInitPhased(&watch,notified,released,&state,traceEmpty));
    assert(xrtFutureWatchAdd(consumer,&watch)==XFUTURE_WATCH_PENDING);
    int64_t input=227;assert(xl6PromiseResolveV1(scalar(),state.producer,&input));
    assert(!state.producer && state.notified==1 && state.released==1);
    assert(*(int64_t*)xrtFutureValue(consumer)==input);xrtFutureDestroy(consumer);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));void* warm=xl6PromiseCreateV1(scalar());assert(warm);xl6PromiseDestroyV1(warm);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    prefixes();pinnedOwner(0);pinnedOwner(1);pinnedOwner(2);cycle(false);cycle(true);reentry();
    assert(!xrtGetError());balanced(&before);
    puts("Promise complete OOM prefixes, independent failure, actual plan pins, commit/abort cycles balanced");
    return 0;
}
