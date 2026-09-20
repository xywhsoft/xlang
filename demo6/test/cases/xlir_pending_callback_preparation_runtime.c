#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_future_callback.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_cell.h"
#include "src/runtime/xl6_future_payload.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int pending_graph_bind(const XL6_CodeOwner*);
extern int pending_graph_init(void);
extern int pending_graph_prepare(void);
extern int pending_graph_finish(void);
extern bool pending_graph_trace(XL6_ModuleOwnershipVisit,void*);
extern void pending_graph_keep(int64_t);
typedef struct Owner {
    size_t refs;
    xpromise *promise,*retired;
    const void* claim;
    bool closed,cleared,finished;
} Owner;
static Owner owner;
static xvalue* alias;
static unsigned events,callbacks,finalizers,deferrals;
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
static bool resolve(xrtownershipref ref,void* context,const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** prep)
{
    (void)context;*prep=NULL;
    if(ref.Ops==&owner_ops){*adapter=&owner_adapter;return true;}
    if((*adapter=xl6FutureCallbackOwnershipAdapterV1(ref,prep)))return true;
    const xfutureproducerownershipv1* producers[]={xl6FutureCallbackProducerPolicyV1Get()};
    const xfuturewatchownershipv1* watches[]={xl6FutureCallbackWatchPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[6];
    for(unsigned i=0;i<6;++i)payloads[i]=xl6PayloadPolicyGet((XL6_FuturePayloadKind)(XL6_FUTURE_PAYLOAD_VALUE+i));
    if((*adapter=xrtFutureOwnershipAdapterV3(ref,payloads,6,producers,1,watches,1,prep)))return true;
    if((*adapter=xl6ClosureOwnershipAdapterV1(ref)))return true;
    if((*adapter=xl6CellOwnershipAdapterV1(ref)))return true;
    if((*adapter=xl6CodeLeaseOwnershipAdapterV1(ref)))return true;
    if((*adapter=xl6ClassOwnershipAdapterV1(ref)))return true;
    if((*adapter=xrtValueCallableOwnershipAdapterV1(ref)))return true;
    if((*adapter=xrtValueFutureOwnershipAdapterV1(ref)))return true;
    if((*adapter=xrtCancelOwnershipAdapterV1(ref)))return true;
    if((*adapter=xrtErrorOwnershipAdapterV1(ref)))return true;
    *adapter=xrtValueOwnershipAdapterV1(ref);return *adapter!=NULL;
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
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    const XL6_CodeOwnerGraph code={{sizeof(code),&owner,acquire,code_release},0x584C36434F574E31ULL,&owner_ops};
    assert(pending_graph_bind(&code.base));
    for(unsigned n=0;n<100;++n){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);owner=(Owner){0};owner.refs=1;events=0;
        assert(pending_graph_init());pending_graph_keep(n%2);assert(!xrtGetError()&&!events&&owner.promise);
        assert(pending_graph_prepare()==XL6_MODULE_PREPARE_READY);
        xrtownershipscope mutation={0};begin(&mutation);owner.closed=true;end(&mutation);release(&owner);
        xrtownershipref root={&owner,&owner_ops};XL6_OwnershipPlan* plan=NULL;
        if(alias){
            assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
            assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY&&!events);
            assert(xl6OwnershipPlanFinalize(plan)&&!events&&xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);
            assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);plan=NULL;
            xrtValueRelease(alias);alias=NULL;++deferrals;assert(!events);
        }
        assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
        assert(xl6OwnershipPlanFinalize(plan)&&!events); /* Cannot skip preparation. */
        assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY&&events==12);
        assert(xl6OwnershipPlanFinalize(plan)&&events==123);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY&&xl6OwnershipPlanFinish(plan));
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY&&!owner.refs&&owner.finished);
        xrtClearError();xrtMemDebugSnapshot(&after);
        assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
            before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
    }
    assert(callbacks==200&&finalizers==100&&deferrals==50);
    puts("Pending callback preparation: 100 generated two-finally graphs, 50 external-result deferrals, 200 ordered callbacks before 100 class finalizers, complete trace/prepare/clear/finish and zero live delta; no fake roots or guard leases");return 0;
}
