#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_NET_RESOLVER_FUTURE
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xrt/xrt.h"
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* An actual owner cycle: Root owns its Resolver creator and Future observer;
 * the resolver's owned lookup context (and pending Future waiter) retain Root.
 * No synthetic RC or internal-root subtraction, no fake service retirement. */
typedef struct Root {
    volatile int32 refs;
    xnetresolver* resolver;
    xfuture* future;
    xfuturewatch watch;
    const void* claim;
    unsigned active,done,lookups;
    bool cleared,finalized,cancel,resurrect;
} Root;
static Root* escaped;
static Root* current;
static xrtownershipref last_refusal;
static xnetaddrlist* independent;
static unsigned cycles,pending_plans,notifications,finalizations,retired,refusals,resurrections,infra_retries;
static void begin(xrtownershipscope* s){assert(xrtOwnershipMutationBegin(s));}
static void end(xrtownershipscope* s){assert(xrtOwnershipScopeEnd(s));}
static void freeze_begin(xrtownershipscope* s)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtOwnershipFreezeTryBegin(s)){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static void idle_freeze(xnetresolver* resolver,xrtownershipscope* s)
{
    xrtownershipref ref=xrtNetResolverOwnership(resolver);size_t count;XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){freeze_begin(s);if(ref.Ops->Count(ref.Data,&count))return;end(s);assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static bool root_hold(const void* data)
{
    Root* root=(Root*)data;xrtownershipscope s={0};begin(&s);
    bool ok=!root->cleared&&xrtRefRetain(&root->refs)>0;end(&s);return ok;
}
static void root_drop(const void* data)
{
    Root* root=(Root*)data;xrtownershipscope s={0};begin(&s);int32 refs=xrtRefRelease(&root->refs);assert(refs>=0);
    if(!refs){assert(!root->resolver&&!root->future&&!root->active);xrtFree(root);++retired;}
    end(&s);
}
static bool root_count(const void* data,size_t* count)
{
    const Root* root=data;
    if(root->cleared||root->active||root->refs<=0)return false;
    *count=(size_t)root->refs;return true;
}
static bool root_trace(const void* data,xrtownershipvisitor visit,ptr other)
{
    const Root* root=data;
    return visit(xrtNetResolverOwnership(root->resolver),other)&&visit(xrtFutureOwnership(root->future),other);
}
static const xrtownershipops root_ops={root_count,root_trace};
static xrtownershipref root_ref(const Root* root){return (xrtownershipref){root,&root_ops};}
static bool root_claim(const void* data,const void* token)
{Root* root=(Root*)data;if(root->claim&&root->claim!=token)return false;root->claim=token;return true;}
static void root_restore(const void* data,const void* token)
{Root* root=(Root*)data;assert(root->claim==token&&!root->cleared);root->claim=NULL;}
static bool root_finalize(const void* data,const void* token)
{
    Root* root=(Root*)data;assert(root->claim==token&&root->done==1&&!root->active&&!root->cleared&&!root->finalized);
    assert(xrtFutureState(root->future)==(root->cancel?XFUTURE_CANCELLED:XFUTURE_RESOLVED));
    root->finalized=true;++finalizations;return true;
}
static void root_clear(const void* data,const void* token)
{Root* root=(Root*)data;assert(root->claim==token&&root->finalized&&!root->cleared);root->cleared=true;}
static bool root_finish(const void* data,const void* token)
{
    Root* root=(Root*)data;xrtownershipscope s={0};begin(&s);assert(root->claim==token&&root->cleared);
    xnetresolver* resolver=root->resolver;xfuture* future=root->future;
    root->resolver=NULL;root->future=NULL;end(&s);
    assert(xrtNetResolverTryDestroy(resolver)==XNET_RETIRE_READY);xrtFutureDestroy(future);return true;
}
static const xrtownershipadapterv1 root_adapter={sizeof(root_adapter),root_hold,root_drop,
    root_claim,root_restore,root_finalize,root_clear,root_finish};
static xnetaddrlist* lookup(cstr host,xnetfamily family,ptr data)
{
    Root* root=data;(void)host;(void)family;xrtownershipscope s={0};begin(&s);++root->lookups;end(&s);
    xnetaddr address;assert(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));return xrtNetAddrListCreate(&address,1);
}
static const xnetresolverlookupownershipv1 lookup_policy={sizeof(lookup_policy),lookup,root_drop,&root_ops};
static const xnetresolverlookupownershipv1 wrong_lookup={sizeof(wrong_lookup),lookup,root_drop,&root_ops};
static void notify(ptr data)
{
    Root* root=data;xrtownershipscope s={0};begin(&s);++root->active;++root->done;++notifications;
    assert(root->done==1&&!root->cleared&&!root->finalized);
    if(!root->cancel){assert(!independent);independent=xrtNetAddrListRef((xnetaddrlist*)xrtFutureValue(root->future));assert(independent);}
    if(root->resurrect){assert(!escaped&&root_hold(root));escaped=root;}
    --root->active;end(&s);
}
static void release(ptr data){root_drop(data);}
static const xfuturewatchownershipv1 waiter_policy={sizeof(waiter_policy),notify,release,&root_ops};
static bool resolve(xrtownershipref ref,void* data,const XL6_OwnershipAdapterV1** output,
    const xrtownershippreparationv1** preparation)
{
    bool rejected=*(bool*)data;const xrtownershipadapterv1* result=NULL;*preparation=NULL;
    const xnetresolverlookupownershipv1* lookups=rejected?&wrong_lookup:&lookup_policy;
    const xnetresolveownershipv1* request=xrtNetResolverFutureRequestPolicyV1Get();
    const xfutureproducerownershipv1* producer=xrtNetResolverFutureProducerPolicyV1Get();
    const xfuturepayloadownershipv1* payload=xrtNetResolverFuturePayloadPolicyV1Get();
    const xcancelwatchownershipv1* cancel=xrtNetResolverFutureCancelPolicyV1Get();
    const xfuturewatchownershipv1* waiter=&waiter_policy;
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);
    admission.ProducerPolicies=&producer;admission.ProducerPolicyCount=1;
    admission.PayloadPolicies=&payload;admission.PayloadPolicyCount=1;
    admission.WatchPolicies=&waiter;admission.WatchPolicyCount=1;
    admission.CancelWatchPolicies=&cancel;admission.CancelWatchPolicyCount=1;
    if(ref.Ops==&root_ops)result=&root_adapter;
    else if((result=xrtNetResolverOwnershipAdapterV1(ref,&lookups,1,preparation))){}
    else if((result=xrtNetResolveOpOwnershipAdapterV1(ref,&request,1,preparation))){}
    else if((result=xrtNetResolverFutureOwnershipAdapterV1(ref,preparation))){}
    else if((result=xrtFutureOwnershipAdapterV4(ref,&admission,preparation))){}
    else if((result=xrtCancelOwnershipAdapterV2(ref,&cancel,1,preparation))){}
    else if((result=xrtCancelWatchOwnershipAdapterV1(ref,&cancel,1,preparation))){}
    else if((result=xrtNetAddrListOwnershipAdapterV1(ref))){}
    else result=xrtErrorOwnershipAdapterV1(ref);
    if(!result)last_refusal=ref;
    *output=result;return result!=NULL;
}
static XL6_OwnershipPlanResult create_plan(Root* root,bool* rejected,XL6_OwnershipPlan** plan)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){
        XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCreateV2(root_ref(root),resolve,rejected,plan);
        if(status>XL6_OWNERSHIP_PLAN_BUSY)return status;
        if(*rejected&&status==XL6_OWNERSHIP_PLAN_ERROR)return status;
        assert(!xl6DeadlineExpiredV1(deadline));xrtClearError();++infra_retries;xrtThreadYield();
    }
}
static void destroy_plan(XL6_OwnershipPlan* plan)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    while(xl6OwnershipPlanTryDestroy(plan)!=XL6_OWNERSHIP_PLAN_READY){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static XL6_OwnershipPlanResult settle(XL6_OwnershipPlan* plan)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){
        XL6_OwnershipStepResult step=xl6OwnershipPlanPrepare(plan);
        assert(step!=XL6_OWNERSHIP_STEP_FAILED);
        XL6_OwnershipPlanResult state=xl6OwnershipPlanTryRefresh(plan);
        if(state==XL6_OWNERSHIP_PLAN_ROOTED||state==XL6_OWNERSHIP_PLAN_FINALIZERS||state==XL6_OWNERSHIP_PLAN_READY)return state;
        if(xl6DeadlineExpiredV1(deadline)){
            fprintf(stderr,"settle timeout: step=%d state=%d done=%u lookups=%u root-active=%u last-resolver=%d last-future=%d error=%d\n",
                (int)step,(int)state,current->done,current->lookups,current->active,
                last_refusal.Data==current->resolver,last_refusal.Data==current->future,(int)xrtErrorKind(xrtGetError()));
            abort();
        }
        xrtClearError();++infra_retries;xrtThreadYield();
    }
}
static void one(unsigned mode)
{
    xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
    Root* root=xrtCalloc(1,sizeof(*root));assert(root);current=root;root->refs=1;root->cancel=(mode&1)!=0;root->resurrect=(mode&2)!=0;
    assert(root_hold(root));xnetresolverconfig config;xrtNetResolverConfigInit(&config);config.Workers=1;config.CacheEntries=0;
    root->resolver=xrtNetResolverCreateOwnedV1(&config,root,&lookup_policy);assert(root->resolver);
    xrtownershipscope freeze={0};idle_freeze(root->resolver,&freeze);
    root->future=xrtNetResolveAsync(root->resolver,"fixture.test",XNET_FAMILY_IPV4);assert(root->future);
    assert(xrtFutureWatchInitOwnershipV1(&root->watch,root,&waiter_policy)&&root_hold(root));
    assert(xrtFutureWatchAdd(root->future,&root->watch)==XFUTURE_WATCH_PENDING);root_drop(root);
    xrtownershipref view=root_ref(root);xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&view,1,NULL,0,&graph)&&graph.NodeCount==8&&graph.EdgeCount==15&&graph.ExternalRootCount==0);
    if(root->cancel)assert(xrtFutureCancel(root->future));
    end(&freeze);
    bool rejected=(mode&4)!=0;XL6_OwnershipPlan* plan=NULL;
    if(rejected){
        assert(create_plan(root,&rejected,&plan)==XL6_OWNERSHIP_PLAN_ERROR&&!plan);++refusals;xrtClearError();rejected=false;
    }
    XL6_OwnershipPlanResult state=create_plan(root,&rejected,&plan);assert(plan);
    if(state==XL6_OWNERSHIP_PLAN_PREPARATIONS)++pending_plans;
    state=settle(plan);
    if(root->resurrect){
        assert(state==XL6_OWNERSHIP_PLAN_ROOTED&&escaped==root&&!root->finalized);
        destroy_plan(plan);plan=NULL;root_drop(escaped);escaped=NULL;++resurrections;
        (void)create_plan(root,&rejected,&plan);assert(plan);state=settle(plan);
    }
    assert(state==XL6_OWNERSHIP_PLAN_FINALIZERS&&!root->finalized);
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){
        XL6_OwnershipStepResult step=xl6OwnershipPlanFinalizeV2(plan);assert(step!=XL6_OWNERSHIP_STEP_FAILED);
        state=xl6OwnershipPlanTryCommit(plan);
        if(state==XL6_OWNERSHIP_PLAN_READY)break;
        assert(state==XL6_OWNERSHIP_PLAN_BUSY||state==XL6_OWNERSHIP_PLAN_FINALIZERS);
        assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();
    }
    assert(root->finalized&&root->done==1);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    destroy_plan(plan);
    if(independent){assert(xrtNetAddrListCount(independent)==1);xrtNetAddrListDestroy(independent);independent=NULL;}
    xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount);++cycles;
}
int main(void)
{
    for(unsigned i=0;i<12;++i)for(unsigned mode=0;mode<8;++mode)one(mode);
    assert(cycles==96&&notifications==96&&finalizations==96&&retired==96&&refusals==48&&resurrections==48);
    assert(xrtMemDebugReset());
    printf("Resolver Future real XLIR plans: cycles=%u pending-admissions=%u notifications=%u finalizers=%u retired=%u policy-refusals=%u resurrections=%u infrastructure-retries=%u; exact memory balance; not module or DLL-unload proof\n",
        cycles,pending_plans,notifications,finalizations,retired,refusals,resurrections,infra_retries);
    return 0;
}
