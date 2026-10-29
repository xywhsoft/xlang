#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_future_callback.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_cell.h"
#include "src/runtime/xl6_future_payload.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
extern int pending_graph_bind(const XL6_CodeOwner*);
extern int pending_graph_init(void);
extern int pending_graph_prepare(void);
extern int pending_graph_finish(void);
extern bool pending_graph_trace(XL6_ModuleOwnershipVisit,void*);
extern void pending_graph_keep(int64_t);
typedef struct Owner {
    atomic_size_t refs;
    xpromise *promise,*retired;
    const void* claim;
    bool closed,cleared,finished;
} Owner;
static Owner owner;
static xvalue* alias;
static atomic_uint events,callbacks,finalizers;
static unsigned deferrals;
static XL6_OwnershipCatalog* catalog;
static void begin(xrtownershipscope* s){assert(xrtOwnershipMutationBegin(s));}
static void end(xrtownershipscope* s){assert(xrtOwnershipScopeEnd(s));}
static int acquire(void* data)
{
    Owner* o=data;xrtownershipscope mutation={0};begin(&mutation);
    if(o->closed){end(&mutation);return 0;}assert(o->refs);++o->refs;end(&mutation);return 1;
}
static void release(const void* data)
{
    Owner* o=(Owner*)data;xrtownershipscope mutation={0};begin(&mutation);
    assert(o->refs);--o->refs;
    if(!o->refs)assert(o->finished&&!o->promise&&!o->retired);
    end(&mutation);
}
static void code_release(void* data){release(data);}
static bool owner_count(const void* data,size_t* result)
{const Owner* o=data;if(!o->refs||o->cleared)return false;*result=o->refs;return true;}
static bool owner_trace(const void* data,xrtownershipvisitor visit,void* context)
{const Owner* o=data;return visit(xrtPromiseOwnership(o->promise),context)&&pending_graph_trace(visit,context);}
static const xrtownershipops owner_ops={owner_count,owner_trace};
static bool hold(const void* data){Owner* o=(Owner*)data;assert(o->refs&&!o->cleared);++o->refs;return true;}
static bool claim(const void* data,const void* token){Owner* o=(Owner*)data;if(o->claim&&o->claim!=token)return false;o->claim=token;return true;}
static void restore(const void* data,const void* token){Owner* o=(Owner*)data;assert(o->claim==token&&!o->cleared);o->claim=NULL;}
static void clear(const void* data,const void* token)
{
    Owner* o=(Owner*)data;assert(o->claim==token&&!o->cleared&&events==123);
    o->retired=o->promise;o->promise=NULL;o->cleared=true;
}
static bool finish(const void* data,const void* token)
{
    Owner* o=(Owner*)data;xpromise* p;xrtownershipscope mutation={0};
    assert(o->claim==token&&o->cleared);if(o->finished)return true;
    assert(pending_graph_finish());begin(&mutation);p=o->retired;o->retired=NULL;o->finished=true;end(&mutation);
    xrtPromiseDestroy(p);return true;
}
static const xrtownershipadapterv1 owner_adapter={sizeof(owner_adapter),hold,release,claim,restore,NULL,clear,finish};
static XL6_OwnershipAdmissionV1 resolve(xrtownershipref ref,void* context,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** prep,const XL6_OwnershipFinishV1** completion)
{
    (void)context;*adapter=NULL;*prep=NULL;*completion=NULL;
    if(ref.Ops==&owner_ops){*adapter=&owner_adapter;return XL6_OWNERSHIP_ADMISSION_ADMITTED;}
    return xl6OwnershipCatalogProbeV1(ref,catalog,adapter,prep,completion);
}
static XL6_OwnershipPlanResult create_plan(xrtownershipref root,XL6_OwnershipPlan** plan)
{
    XL6_DeadlineV1 limit=xl6DeadlineAfterMillisecondsV1(5000);XL6_OwnershipPlanResult result;
    while((result=xl6OwnershipPlanTryCreateV6(root,resolve,NULL,false,NULL,plan))==XL6_OWNERSHIP_PLAN_BUSY){
        assert(!*plan&&!xl6DeadlineExpiredV1(limit));xrtThreadYield();
    }
    return result;
}
static XL6_OwnershipStepResult prepare_plan(XL6_OwnershipPlan* plan)
{
    XL6_DeadlineV1 limit=xl6DeadlineAfterMillisecondsV1(5000);XL6_OwnershipStepResult result;
    while((result=xl6OwnershipPlanPrepare(plan))==XL6_OWNERSHIP_STEP_BUSY){
        assert(events<=12&&!owner.cleared&&!xl6DeadlineExpiredV1(limit));xrtThreadYield();
    }
    return result;
}
void pending_callback_observe(int64_t event,const char* text)
{
    assert(!owner.cleared&&!owner.finished&&text&&!strcmp(text,"pending owner"));
    assert(event==(events==0?1:events==1?2:3));events=events*10+(unsigned)event;
    if(event==3)++finalizers;else ++callbacks;
}
void pending_callback_capture(xvalue* value)
{assert(!alias&&xrtValueIsFuture(value));alias=xrtValueClone(value);assert(alias);}
xfuture* pending_callback_input(void)
{
    xfuture* future=NULL;xrtownershipscope mutation={0};assert(!owner.promise);
    xpromise* promise=xrtPromiseCreate(&future,NULL);assert(promise);
    begin(&mutation);owner.promise=promise;end(&mutation);return future;
}
int main(void)
{
    xmemdebugsnapshot whole;
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&whole);
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainCodeLeaseV1Get(),xl6OwnershipDomainClassV1Get(),
        xl6OwnershipDomainCellV1Get(),xl6OwnershipDomainClosureV1Get(),xl6OwnershipDomainExecutorV1Get(),
        xl6OwnershipDomainQueueV1Get(),xl6OwnershipDomainFutureCallbackQueueV1Get(),
        xl6OwnershipDomainFuturePayloadV1Get(),xl6OwnershipDomainNativeFutureV1Get()};
    catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));assert(catalog);
    const XL6_CodeOwnerGraph code={{sizeof(code),&owner,acquire,code_release},0x584C36434F574E31ULL,&owner_ops};
    assert(pending_graph_bind(&code.base));
    for(unsigned n=0;n<100;++n){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);owner=(Owner){0};owner.refs=1;events=0;
        assert(pending_graph_init());pending_graph_keep(n%2);assert(!xrtGetError()&&!events&&owner.promise);
        assert(pending_graph_prepare()==XL6_MODULE_PREPARE_READY);
        xrtownershipscope mutation={0};begin(&mutation);owner.closed=true;end(&mutation);release(&owner);
        xrtownershipref root={&owner,&owner_ops};XL6_OwnershipPlan* plan=NULL;
        if(alias){
            assert(create_plan(root,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
            assert(prepare_plan(plan)==XL6_OWNERSHIP_STEP_READY&&!events);
            assert(xl6OwnershipPlanFinalize(plan)&&!events&&xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);
            assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);plan=NULL;
            xrtValueRelease(alias);alias=NULL;++deferrals;assert(!events);
        }
        assert(create_plan(root,&plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
        assert(xl6OwnershipPlanFinalize(plan)&&!events); /* Cannot skip preparation. */
        assert(prepare_plan(plan)==XL6_OWNERSHIP_STEP_READY&&events==12);
        assert(xl6OwnershipPlanFinalize(plan)&&events==123);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY&&xl6OwnershipPlanFinish(plan));
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY&&!owner.refs&&owner.finished);
        xrtClearError();xrtMemDebugSnapshot(&after);
        assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
            before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
    }
    assert(callbacks==200&&finalizers==100&&deferrals==50);
    xl6OwnershipCatalogDestroyV1(catalog);catalog=NULL;
    {xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
        assert(whole.LiveCount==after.LiveCount&&whole.LiveBytes==after.LiveBytes&&
            after.AllocCount-whole.AllocCount==after.FreeCount-whole.FreeCount);}
    puts("Pending callback preparation: 100 generated two-finally graphs, 50 external-result deferrals, 200 ordered callbacks before 100 class finalizers, complete trace/prepare/clear/finish and zero live delta; no fake roots or guard leases");return 0;
}
