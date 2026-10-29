#include "foreign-graph-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime_features.h"
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "include/xl_foreign_graph.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
const XL6_OwnershipDomainV1* xl6OwnershipDomainForeignResourceV1Get(void);

typedef struct Payload { xvalue* edge; } Payload;
static size_t drops,preparations;
static bool deferPreparation;
static bool rawTrace(const void* data,xrtownershipvisitor visit,void* context)
{ return visit(xrtValueOwnership(((const Payload*)data)->edge),context); }
static xrtownershipprepareresult rawPrepare(void* raw)
{
    assert(raw);
    if(deferPreparation){deferPreparation=false;return XRT_OWNERSHIP_PREPARE_BUSY;}
    ++preparations;return XRT_OWNERSHIP_PREPARE_READY;
}
static const XL6_ForeignGraphDescriptorV1 descriptor={sizeof(descriptor),XL6_FOREIGN_GRAPH_V1_TAG,rawTrace,rawPrepare,NULL};
static bool dispose(void* context,const xrtcallframe* frame,xrtcallresult* result)
{
    (void)context;(void)result;void* raw=NULL;
    assert(frame->ArgumentCount==1&&xrtValueGetPointer(frame->Arguments[0],&raw));
    Payload* payload=raw;assert(payload);xrtValueRelease(payload->edge);xrtFree(payload);++drops;return true;
}
static void unused(void) { assert(0); }
static xvalue* callback(void)
{
    XL6_Closure* closure=xl6ClosureRuntimeV2Get()->Create(NULL,unused,NULL,NULL,NULL,NULL,NULL,1);
    assert(closure);xvalue* result=xl6ClosureBoxPointActionV1(NULL,closure,dispose,42);
    xl6ClosureRuntimeV1Get()->Release(closure);assert(result);return result;
}
static Payload* allocate(xvalue* edge)
{ Payload* result=xrtMalloc(sizeof(*result));assert(result);result->edge=xrtValueRetain(edge);return result; }
static bool captureRef(xrtownershipref ref,void* context)
{ *(xrtownershipref*)context=ref;return true; }
static bool resolve(xrtownershipref ref,void* context,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** finish)
{
    *adapter=NULL;*preparation=NULL;*finish=NULL;
    if(xl6OwnershipCatalogResolveV1(ref,context,adapter,preparation,finish))return true;
    *adapter=xl6ClosureOwnershipAdapterV1(ref);
    if(!*adapter)*adapter=xrtValueCallableOwnershipAdapterV1(ref);
    return *adapter!=NULL;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xvalue* releaser=callback();
    const XL6_OwnershipDomainV1* domain=xl6OwnershipDomainForeignResourceV1Get();
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(&domain,1);assert(catalog);
    xvalue* object=xrtValueObject();assert(object);
    Payload* raw=allocate(object);
    void* record=xl6ForeignGraphCreateV1(raw,&descriptor,releaser);assert(record);
    xvalue* token=xl6NativeHandleTakeV1(&record,xl6ForeignGraphHandlePolicyV1Get());assert(token&&!record);
    void* native=NULL;assert(xrtValueGetHandle(token,&native,NULL,NULL));
    XL6_ForeignGraphScopeV1 scope={0};assert(xl6ForeignGraphEnterV1(native,&scope)&&scope.raw==raw);
    XL6_ForeignGraphScopeV1 reentrant={0};assert(!xl6ForeignGraphEnterV1(native,&reentrant));xrtClearError();
    /* An active native call is explicitly BUSY before its raw tracer runs. */
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* finish=NULL;xrtownershipref child={0};
    const XL6_NativeHandlePolicyV1* policy=xl6ForeignGraphHandlePolicyV1Get();
    assert(policy->Trace(token,captureRef,&child));
    assert(xl6OwnershipCatalogProbeV1(child,catalog,&adapter,&preparation,&finish)==XL6_OWNERSHIP_ADMISSION_BUSY);
    assert(xrtOwnershipScopeEnd(&freeze));xl6ForeignGraphLeaveV1(&scope);
    assert(xrtValueObjectSet(object,(xstrview){"native",6},token));
    xvalue* weak=xrtValueWeakRef(object);assert(weak);
    xrtValueRelease(token);xrtValueRelease(object);xrtValueRelease(releaser);
    XL6_OwnershipPlan* plan=NULL;
    XL6_OwnershipPlanResult ready=xl6OwnershipPlanTryCreateV3(child,resolve,catalog,&plan);
    assert(ready>0&&plan);
    deferPreparation=true;
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_BUSY);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)>0);
    assert(drops==1&&preparations==1&&xrtValueWeakRefExpired(weak));
    xrtValueRelease(weak);xl6OwnershipCatalogDestroyV1(catalog);xrtClearError();
    releaser=callback();
    /* Every allocation prefix must leave unaccepted raw ownership with the
     * caller; accepted records perform exactly one terminal destruction. */
    xrtSetErrorKind(XERR_MEMORY);xrtClearError();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    unsigned refused=0,accepted=0;
    for(unsigned fault=0;fault<16;++fault){
        Payload* payload=allocate(xrtValueNull());size_t prior=drops;
        xl6_error_scope call={0};assert(xl6ErrorScopeBegin(&call));
        assert(xrtMemDebugFailAfter(fault));
        void* candidate=xl6ForeignGraphCreateV1(payload,&descriptor,releaser);
        xrtMemDebugFailClear();
        xl6_error_result outcome=xl6ErrorScopeEnd(&call);
        assert(candidate?outcome.ok:!outcome.ok);
        xrtErrorFree(outcome.error);xrtClearError();
        if(candidate){++accepted;xl6ForeignGraphDestroyV1(candidate);assert(drops==prior+1);}
        else {++refused;assert(drops==prior);xrtValueRelease(payload->edge);xrtFree(payload);}
        xrtClearError();
    }
    assert(refused&&accepted);xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
    xrtValueRelease(releaser);
    puts("foreign graph: real native/language cycle, active-call BUSY, reentry rejection, phased at-most-once cleanup");
    return 0;
}
