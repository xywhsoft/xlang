#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_cancellation.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include "xl_cancellation_retirement.h"
#include <assert.h>
#include <stdio.h>

static bool good(void* raw) { (void)raw; return true; }
static bool fail(void* raw) { (void)raw; return false; }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->DoubleFreeCount==after.DoubleFreeCount && before->InvalidFreeCount==after.InvalidFreeCount);
    assert(before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static XL6_Closure* closure(bool failed)
{
    return xl6ClosureRuntimeV1Get()->Create(NULL,(void (*)(void))(failed?fail:good),NULL,
        NULL,NULL,NULL,NULL,0,NULL);
}
static void prefixes(void)
{
    const XL6_ClosureRuntimeV1* runtime=xl6ClosureRuntimeV1Get();
    for (int mode=0;mode!=3;++mode) {
        void* parent=xl6CancellationSourceCreateV1(); assert(parent);
        bool complete=false;
        for(size_t point=0;point!=48;++point) {
            xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
            xl6_foreign_error_scope scope={0}; assert(xl6ForeignCallBegin(&scope));
            assert(xrtMemDebugFailAfter(point));
            void* raw=mode==0?xl6CancellationSourceCreateV1():mode==1?
                xl6CancellationSourceChildV1(parent):xl6CancellationTokenCreateV1(parent);
            xvalue* value=raw?xl6NativeHandleTakeV1(&raw,mode==2?
                xl6CancellationTokenHandlePolicyV1Get():xl6CancellationSourceHandlePolicyV1Get()):NULL;
            bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            xl6_foreign_error_result result=xl6ForeignCallEnd(&scope);
            assert(!raw && (hit ? !value && !result.ok : value && result.ok));
            xrtErrorFree(result.error); xrtClearError(); xrtValueRelease(value);
            assert(!xrtGetError()); balanced(&before);
            if(!hit) { printf("cancellation factory prefix mode %d: %zu\n",mode,point);complete=true;break; }
        }
        assert(complete); xl6CancellationSourceDestroyV1(parent);
    }
    for (int mode=0;mode!=3;++mode) {
        void* source=xl6CancellationSourceCreateV1(); assert(source);
        void* child=mode==2?xl6CancellationSourceChildV1(source):NULL;
        void* token=child?child:source;
        XL6_Closure* callback=closure(false); assert(callback);
        if (mode==1) assert(xl6CancellationSourceCancelV1(source));
        bool complete=false;
        for(size_t point=0;point!=96;++point) {
            xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
            xl6_foreign_error_scope scope={0}; assert(xl6ForeignCallBegin(&scope));
            assert(xrtMemDebugFailAfter(point));
            void* raw=xl6CancellationRegistrationCreateV1(token,callback);
            xvalue* value=raw?xl6NativeHandleTakeV1(&raw,xl6CancellationRegistrationHandlePolicyV1Get()):NULL;
            bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            xl6_foreign_error_result result=xl6ForeignCallEnd(&scope);
            assert(!raw && (hit ? !value && !result.ok : value && result.ok));
            xrtErrorFree(result.error); xrtClearError(); xrtValueRelease(value);
            assert(!xrtGetError()); balanced(&before);
            if(!hit) { printf("cancellation complete allocation prefix mode %d: %zu\n",mode,point);complete=true;break; }
        }
        assert(complete); runtime->Release(callback);
        xl6CancellationSourceDestroyV1(child); xl6CancellationSourceDestroyV1(source);
    }
}
static void independentFailure(void)
{
    void* source=xl6CancellationSourceCreateV1(); XL6_Closure* callback=closure(true);
    assert(source && callback);
    void* registration=xl6CancellationRegistrationCreateV1(source,callback); assert(registration);
    xl6_foreign_error_scope scope={0}; assert(xl6ForeignCallBegin(&scope));
    assert(xrtMemDebugFailAfter(0));
    assert(xl6CancellationSourceCancelV1(source));
    xrtMemDebugFailClear();
    xl6_foreign_error_result result=xl6ForeignCallEnd(&scope);
    assert(!result.ok); xrtErrorFree(result.error); xrtClearError();
    assert(xl6CancellationRegistrationFailedV1(registration));
    assert(xl6ForeignCallBegin(&scope));
    assert(xrtMemDebugFailAfter(0));
    xl6CancellationRegistrationCheckV1(registration);
    xrtMemDebugFailClear(); result=xl6ForeignCallEnd(&scope);
    assert(!result.ok); xrtErrorFree(result.error); xrtClearError();
    xl6CancellationRegistrationDestroyV1(registration);
    assert(xl6ForeignCallBegin(&scope));
    assert(!xl6CancellationRegistrationCreateV1(source,callback));
    result=xl6ForeignCallEnd(&scope); assert(!result.ok); xrtErrorFree(result.error); xrtClearError();
    xl6ClosureRuntimeV1Get()->Release(callback); xl6CancellationSourceDestroyV1(source);
}
static bool collect(xrtownershipref ref,void* context) { *(xrtownershipref*)context=ref;return true; }
static void checkedRetirement(void)
{
    void* source=xl6CancellationSourceCreateV1(); XL6_Closure* callback=closure(false);
    void* raw=xl6CancellationRegistrationCreateV1(source,callback); assert(raw);
    xvalue* handle=xl6NativeHandleTakeV1(&raw,xl6CancellationRegistrationHandlePolicyV1Get()); assert(handle && !raw);
    xrtownershipref state={0};
    assert(xl6CancellationRegistrationHandlePolicyV1Get()->Trace(handle,collect,&state));
    const xrtownershipfinishv1* completion=NULL;
    const xrtownershipadapterv1* adapter=xl6CancellationRegistrationAdapterV1(state,&completion);
    assert(adapter && completion && completion->Adapter==adapter);
    int owner;
    xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(adapter->Claim(state.Data,&owner)); assert(xrtOwnershipScopeEnd(&freeze));
    xl6_foreign_error_scope scope={0}; assert(xl6ForeignCallBegin(&scope));
    xrtValueRelease(handle);
    xl6_foreign_error_result result=xl6ForeignCallEnd(&scope);
    assert(!result.ok); xrtErrorFree(result.error); xrtClearError();
    XL_CancellationRetirementV1 row; size_t total=0;
    assert(xlCancellationRetirementSnapshotCheckedV1(&row,1,&total) && total==1 && row.token);
    assert(!xlCancellationRetirementRetryCheckedV1(row.token)); xrtClearError();
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    adapter->Restore(state.Data,&owner); assert(xrtOwnershipScopeEnd(&freeze));
    assert(xlCancellationRetirementRetryCheckedV1(row.token));
    assert(xlCancellationRetirementSnapshotCheckedV1(NULL,0,&total) && total==0);
    assert(!xlCancellationRetirementRetryCheckedV1(row.token)); xrtClearError();
    assert(xl6CancellationSourceCancelV1(source));
    xl6ClosureRuntimeV1Get()->Release(callback); xl6CancellationSourceDestroyV1(source);
}
static XL6_OwnershipCatalog* catalog(void)
{
    /* Multiple consumers must select the exact same physical cancellation
     * adapters/completions. No table-order fallback or producer guessing. */
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainCancellationSourceV1Get(),
        xl6OwnershipDomainCancellationRegistrationV1Get(),xl6OwnershipDomainClosureV1Get(),
        xl6OwnershipDomainNativeFutureV1Get(),xl6OwnershipDomainResourceTaskGroupV1Get(),
        xl6OwnershipDomainResourceProcessOptionsV1Get()};
    return xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));
}
typedef struct Captured { xvalue* value; } Captured;
static void cycle(bool abortPlan)
{
    void* source=xl6CancellationSourceCreateV1(); assert(source);
    xvalue* root=xrtValueObject(); assert(root);
    static const XL6_ClosureCaptureSlot slot={XL6_CLOSURE_CAPTURE_XVALUE,offsetof(Captured,value),sizeof(xvalue*)};
    static const XL6_ClosureEnvironmentV1 layout={sizeof(layout),sizeof(Captured),1,&slot};
    Captured* captured=xrtMalloc(sizeof(*captured)); assert(captured);
    captured->value=xrtValueRetain(root);
    XL6_Closure* callback=xl6ClosureRuntimeV2Get()->Create(NULL,(void (*)(void))good,NULL,
        captured,&layout,NULL,NULL,0);
    assert(callback);
    void* raw=xl6CancellationRegistrationCreateV1(source,callback); assert(raw);
    xl6ClosureRuntimeV1Get()->Release(callback);
    xvalue* handle=xl6NativeHandleTakeV1(&raw,xl6CancellationRegistrationHandlePolicyV1Get()); assert(handle);
    assert(xrtValueObjectSet(root,(xstrview){"listener",8},handle)); xrtValueRelease(handle);
    xvalue* weak=xrtValueWeakRef(root); assert(weak);
    XL6_OwnershipCatalog* policies=catalog(); assert(policies);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV6(xrtValueOwnership(root),xl6OwnershipCatalogProbeV1,NULL,false,
        policies,&plan)==XL6_OWNERSHIP_PLAN_ROOTED && plan);
    xrtValueRelease(root);
    assert(xl6OwnershipPlanTryRefresh(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    if (abortPlan) {
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(!xrtValueWeakRefExpired(weak));
        assert(xl6CancellationSourceCancelV1(source)); /* Abort restored the armed watch. */
        assert(xrtValueWeakRefExpired(weak));
    } else {
        assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(xrtValueWeakRefExpired(weak));
        assert(xl6CancellationSourceCancelV1(source)); /* Retirement never requests cancellation. */
    }
    xrtValueRelease(weak); xl6OwnershipCatalogDestroyV1(policies); xl6CancellationSourceDestroyV1(source);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    void* warm=xl6CancellationSourceCreateV1(); XL6_Closure* cb=closure(false);
    void* listener=xl6CancellationRegistrationCreateV1(warm,cb); assert(listener);
    assert(xl6CancellationSourceCancelV1(warm)); xl6CancellationRegistrationDestroyV1(listener);
    xl6ClosureRuntimeV1Get()->Release(cb); xl6CancellationSourceDestroyV1(warm);
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    prefixes(); independentFailure(); checkedRetirement(); cycle(false); cycle(true);
    assert(!xrtGetError()); balanced(&before);
    puts("cancellation checked callbacks, complete OOM prefixes, owner-driven cycles, abort and recovery balanced");
    return 0;
}
