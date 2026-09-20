/* Run the existing real TCP/UDP/shared-listener callback cases unchanged,
 * then exercise the new binding participant with the production XLIR plan.
 * These cycles certify the binding participant, not a complete transport. */
#define NET_CALLBACK_OWNERSHIP_SINGLE
#define main callback_network_main
#include "tests/network/test_net_callback_ownership.c"
#undef main
#include "src/runtime/xl6_ownership_plan.h"

typedef struct CollectOwner {
    volatile int32 References;
    __xrt_net_callback* Slots[2];
    const void* Claim;
    bool Finalized, Cleared, Resurrect;
} CollectOwner;
static CollectOwner* collector_escape;
static unsigned collector_freed, collector_finalized, collector_context_drops;
static unsigned collector_cycles, collector_shared, collector_restored, collector_active, collector_wrong;
static bool collector_bad_policy;

static bool collector_hold(const void* data)
{
    CollectOwner* p=(CollectOwner*)data;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
    bool ok=!p->Cleared&&xrtRefRetain(&p->References)>0;assert(xrtOwnershipScopeEnd(&s));return ok;
}
static void collector_drop(const void* data)
{
    CollectOwner* p=(CollectOwner*)data;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
    int32 count=xrtRefRelease(&p->References);assert(count>=0);
    if(!count){assert(!p->Slots[0]&&!p->Slots[1]);xrtFree(p);++collector_freed;}
    assert(xrtOwnershipScopeEnd(&s));
}
static bool collector_count(const void* data,size_t* count)
{
    const CollectOwner* p=data;if(!p||!count||p->Cleared)return false;
    int32 actual=__xrtAtomicRefLoad(&p->References);if(actual<=0)return false;*count=(size_t)actual;return true;
}
static bool collector_trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const CollectOwner* p=data;size_t count;if(!visit||!collector_count(p,&count))return false;
    for(size_t i=0;i<2;++i)if(p->Slots[i]&&!visit(__xrtNetCallbackOwnership(p->Slots[i]),context))return false;
    return true;
}
static const xrtownershipops collector_ops={collector_count,collector_trace};
static xrtownershipref collector_ref(const CollectOwner* p)
{return (xrtownershipref){p,&collector_ops};}
static bool collector_claim(const void* data,const void* token)
{
    CollectOwner* p=(CollectOwner*)data;size_t count;
    if(!token||!collector_count(p,&count)||(p->Claim&&p->Claim!=token))return false;
    p->Claim=token;return true;
}
static void collector_restore(const void* data,const void* token)
{CollectOwner* p=(CollectOwner*)data;assert(token&&p->Claim==token&&!p->Cleared);p->Claim=NULL;}
static bool collector_finalize(const void* data,const void* token)
{
    CollectOwner* p=(CollectOwner*)data;assert(token&&p->Claim==token&&!p->Cleared);
    if(p->Finalized)return true;
    xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));p->Finalized=true;++collector_finalized;
    if(p->Resurrect){assert(!collector_escape&&collector_hold(p));collector_escape=p;}
    assert(xrtOwnershipScopeEnd(&s));return true;
}
static void collector_clear(const void* data,const void* token)
{CollectOwner* p=(CollectOwner*)data;assert(token&&p->Claim==token&&p->Finalized&&!p->Cleared);p->Cleared=true;}
static bool collector_finish(const void* data,const void* token)
{
    CollectOwner* p=(CollectOwner*)data;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
    assert(token&&p->Claim==token&&p->Cleared);__xrt_net_callback* a=p->Slots[0];__xrt_net_callback* b=p->Slots[1];
    p->Slots[0]=p->Slots[1]=NULL;assert(xrtOwnershipScopeEnd(&s));
    __xrtNetCallbackDrop(a);__xrtNetCallbackDrop(b);return true;
}
static const xrtownershipadapterv1 collector_adapter={sizeof(collector_adapter),collector_hold,collector_drop,
    collector_claim,collector_restore,collector_finalize,collector_clear,collector_finish};
static int32 collector_probe_freeze(ptr data)
{(void)data;xrtownershipscope s={0};freeze(&s);assert(xrtOwnershipScopeEnd(&s));return 0;}
static void collector_context_drop(ptr data)
{
    /* Joining a different thread that freezes the graph catches any inherited
     * Clear/freeze or mutation scope around the resident mechanical Drop. */
    xthread* t=xrtThreadCreate(collector_probe_freeze,NULL,0);assert(t);
    assert(xrtThreadWaitFor(t,5000000)==XWAIT_OK);xrtThreadDestroy(t);
    ++collector_context_drops;collector_drop(data);
}
static const xnetcallbackownershipv1 collector_policy={sizeof(collector_policy),collector_context_drop,&collector_ops};
static const xnetcallbackownershipv1 collector_other_policy={sizeof(collector_other_policy),collector_context_drop,&collector_ops};
static bool collector_resolve(xrtownershipref ref,void* data,const XL6_OwnershipAdapterV1** adapter,
    const xrtownershippreparationv1** preparation)
{
    (void)data;*preparation=NULL;
    const xnetcallbackownershipv1* admitted=collector_bad_policy?&collector_other_policy:&collector_policy;
    *adapter=ref.Ops==&collector_ops?&collector_adapter:xrtNetCallbackOwnershipAdapterV1(ref,&admitted,1,preparation);
    return *adapter!=NULL;
}
static XL6_OwnershipPlanResult collector_create(CollectOwner* p,XL6_OwnershipPlan** plan)
{return xl6OwnershipPlanTryCreateV2(collector_ref(p),collector_resolve,NULL,plan);}

static void collector_one(unsigned mode)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    unsigned drops_before=collector_context_drops,finalized_before=collector_finalized,freed_before=collector_freed;
    CollectOwner* p=xrtCalloc(1,sizeof(*p));assert(p);p->References=1;p->Resurrect=(mode&2)!=0;
    assert(collector_hold(p));__xrt_net_callback* binding=__xrtNetCallbackCreate(p,&collector_policy);assert(binding);
    p->Slots[0]=binding;
    if(mode&1){p->Slots[1]=__xrtNetCallbackRef(binding);++collector_shared;}
    collector_drop(p); /* Only the binding now owns Context; its two slots are real RC. */
    xrtownershipscope s={0};freeze(&s);xrtownershipref ref=collector_ref(p);xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&ref,1,NULL,0,&graph)&&graph.NodeCount==2&&
        graph.EdgeCount==((mode&1)?3u:2u)&&graph.ExternalRootCount==0);
    xrtownershipref br=__xrtNetCallbackOwnership(binding);
    const xrtownershippreparationv1* preparation=NULL;
    const xnetcallbackownershipv1* admitted=&collector_policy;
    const xrtownershipadapterv1* adapter=xrtNetCallbackOwnershipAdapterV1(br,&admitted,1,&preparation);
    assert(adapter&&preparation&&preparation->Adapter==adapter&&adapter->Hold(br.Data));
    int token=0,foreign=0;assert(adapter->Claim(br.Data,&token));
    assert(!adapter->Claim(br.Data,&foreign)&&adapter->Claim(br.Data,&token));
    assert(xrtOwnershipScopeEnd(&s));
    assert(__xrtNetCallbackEnter(binding)==binding&&__xrtNetCallbackEnter(binding)==binding);
    freeze(&s);size_t actual=731;assert(!br.Ops->Count(br.Data,&actual)&&actual==731);
    assert(!xrtNetCallbackOwnershipAdapterV1(br,&admitted,1,&preparation));
    assert(!preparation->Ready(br.Data));assert(xrtOwnershipScopeEnd(&s));
    assert(preparation->Prepare(br.Data,&token)==XRT_OWNERSHIP_PREPARE_BUSY);++collector_active;
    assert(collector_context_drops==drops_before);
    __xrtNetCallbackLeave(binding);__xrtNetCallbackLeave(binding);
    assert(preparation->Prepare(br.Data,&token)==XRT_OWNERSHIP_PREPARE_READY);
    freeze(&s);adapter->Restore(br.Data,&token);assert(!binding->Claim);assert(xrtOwnershipScopeEnd(&s));
    adapter->Drop(br.Data);++collector_restored;

    XL6_OwnershipPlan* plan=NULL;collector_bad_policy=true;
    assert(collector_create(p,&plan)==XL6_OWNERSHIP_PLAN_ERROR&&!plan&&!p->Claim&&!binding->Claim);
    assert(!p->Finalized&&collector_context_drops==drops_before);collector_bad_policy=false;xrtClearError();++collector_wrong;
    /* A real independent binding pin roots BOTH nodes. Abort restores claims
     * without consuming the callback Context or either real shared slot. */
    assert(adapter->Hold(br.Data));assert(collector_create(p,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);plan=NULL;adapter->Drop(br.Data);
    assert(!p->Claim&&!binding->Claim&&!p->Finalized);
    assert(collector_create(p,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS&&plan);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
    XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCommit(plan);
    if(p->Resurrect){assert(result==XL6_OWNERSHIP_PLAN_ROOTED&&collector_escape==p&&!p->Cleared&&!binding->Cleared);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);plan=NULL;
        assert(collector_create(p,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
        collector_drop(collector_escape);collector_escape=NULL;
        assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
        assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);result=xl6OwnershipPlanTryCommit(plan);
    }
    assert(result==XL6_OWNERSHIP_PLAN_READY&&p->Cleared&&binding->Cleared);
    assert(collector_context_drops==drops_before&&!adapter->Hold(br.Data));
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY&&collector_context_drops==drops_before+1);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY&&collector_context_drops==drops_before+1);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(collector_finalized==finalized_before+1&&collector_freed==freed_before+1);balance(&before);++collector_cycles;
}
int main(void)
{
    assert(callback_network_main()==0);
    for(unsigned i=0;i<4;++i)collector_one(i);
    testMemoryDebugDrain("callback collection balance");
    printf("Callback collection: cycles=%u shared=%u restored=%u active-busy=%u wrong-policy=%u context-drops=%u finalizers=%u freed=%u exact-memory=ok; full transport graph incomplete\n",
        collector_cycles,collector_shared,collector_restored,collector_active,collector_wrong,
        collector_context_drops,collector_finalized,collector_freed);
    return 0;
}
