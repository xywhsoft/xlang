#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>

typedef struct Node {
    volatile int32 refs;
    struct Node *first,*second;
    const void* claim;
    unsigned kind;
    bool ready,finalized,cleared;
} Node;
static Node* escaped;
static unsigned mode,malformed,notifications,finalizers,allocations,frees,counts,traces,probes;
static bool busy,failed_once,added,fail_snapshot_after_notify;
static void begin(xrtownershipscope* scope){assert(xrtOwnershipMutationBegin(scope));}
static void end(xrtownershipscope* scope){assert(xrtOwnershipScopeEnd(scope));}
static Node* make(unsigned kind){Node* n=xrtCalloc(1,sizeof(*n));assert(n);n->refs=1;n->kind=kind;++allocations;return n;}
static Node* retain(Node* n){if(n)assert(xrtRefRetain(&n->refs)>0);return n;}
static void drop(const void* data)
{
    Node* n=(Node*)data;Node *a,*b;xrtownershipscope mutation={0};if(!n)return;
    begin(&mutation);int32 refs=xrtRefRelease(&n->refs);assert(refs>=0);
    if(refs){end(&mutation);return;}
    a=n->first;b=n->second;n->first=n->second=NULL;++frees;xrtFree(n);end(&mutation);drop(a);drop(b);
}
static bool count(const void* data,size_t* result)
{ const Node* n=data;++counts;if(n->refs<=0 || n->cleared)return false;*result=(size_t)n->refs;return true; }
static bool trace(const void*,xrtownershipvisitor,void*);
static const xrtownershipops ops={count,trace};
static xrtownershipref view(const Node* n){return (xrtownershipref){n,n?&ops:NULL};}
static bool trace(const void* data,xrtownershipvisitor visit,void* context)
{const Node* n=data;++traces;return visit(view(n->first),context)&&visit(view(n->second),context);}
static bool hold(const void* data){return retain((Node*)data)!=NULL;}
static bool claim(const void* data,const void* token){Node* n=(Node*)data;if(n->claim&&n->claim!=token)return false;n->claim=token;return true;}
static void restore(const void* data,const void* token){Node* n=(Node*)data;assert(n->claim==token&&!n->cleared);n->claim=NULL;}
static void add_preparation(Node* owner)
{
    xrtownershipscope mutation={0};begin(&mutation);Node* child=make(2);
    child->first=retain(owner->first);assert(!owner->second->second);owner->second->second=child;added=true;end(&mutation);
}
static bool finalize(const void* data,const void* token)
{
    Node* n=(Node*)data;assert(n->claim==token&&!n->finalized&&!n->cleared);
    n->finalized=true;++finalizers;
    if(n->kind==0 && mode==5 && !added)add_preparation(n);
    return true;
}
static void clear(const void* data,const void* token)
{
    Node* n=(Node*)data;assert(n->claim==token&&!n->cleared&&(n->kind==2?n->ready:n->finalized));n->cleared=true;
}
static bool finish(const void* data,const void* token)
{
    Node* n=(Node*)data;Node *a,*b;xrtownershipscope mutation={0};begin(&mutation);
    assert(n->claim==token&&n->cleared);a=n->first;b=n->second;n->first=n->second=NULL;end(&mutation);drop(a);drop(b);return true;
}
static const xrtownershipadapterv1 object_adapter={sizeof(object_adapter),hold,drop,claim,restore,finalize,clear,finish};
static const xrtownershipadapterv1 resource_adapter={sizeof(resource_adapter),hold,drop,claim,restore,NULL,clear,finish};
static bool ready(const void* data){return ((const Node*)data)->ready;}
static int32 probe(void* data)
{xrtownershipscope freeze={0};(void)data;assert(xrtOwnershipFreezeTryBegin(&freeze));end(&freeze);return 0;}
static xrtownershipprepareresult prepare(const void* data,const void* token)
{
    Node* n=(Node*)data;assert(n->claim==token&&!n->ready&&!n->cleared);
    if(busy)return XRT_OWNERSHIP_PREPARE_BUSY;
    if((mode==3||mode==4)&&!failed_once){failed_once=true;if(mode==4)xrtSetErrorKind(XERR_STATE);return XRT_OWNERSHIP_PREPARE_FAILED;}
    assert(n->first && n->first->kind==1 && !n->first->finalized);
    xthread* thread=xrtThreadCreate(probe,NULL,0);assert(thread);
    assert(xrtThreadWaitFor(thread,5000000)==XWAIT_OK);xrtThreadDestroy(thread);++probes;
    /* A semantic callback may take a new external owner or create a new
     * resource. Every next finalizer must see a freshly reconstructed graph. */
    xrtownershipscope mutation={0};begin(&mutation);n->ready=true;++notifications;
    if(mode==1&&!escaped)escaped=retain(n->first);
    end(&mutation);
    if(mode==2&&!added)add_preparation(n->first->first);
    if(fail_snapshot_after_notify){fail_snapshot_after_notify=false;assert(xrtMemDebugFailAfter(0));}
    return XRT_OWNERSHIP_PREPARE_READY;
}
static const xrtownershippreparationv1 preparation={sizeof(preparation),&resource_adapter,ready,prepare};
static bool resolve(xrtownershipref ref,void* context,const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** prep)
{
    static xrtownershippreparationv1 invalid;(void)context;
    if(ref.Ops!=&ops)return false;
    const Node* n=ref.Data;*adapter=n->kind==2?&resource_adapter:&object_adapter;*prep=n->kind==2?&preparation:NULL;
    /* Refuse malformed identities at the FIRST node, before its Count/Trace. */
    if(malformed){invalid=preparation;invalid.Adapter=*adapter;
        if(malformed==1)invalid.size=0;
        if(malformed==2)invalid.Adapter=NULL;
        if(malformed==3)invalid.Ready=NULL;
        if(malformed==4)invalid.Prepare=NULL;
        *prep=&invalid;
    }
    return true;
}
static Node* cycle(void)
{
    Node* owner=make(0),*value=make(1),*resource=make(2);
    owner->first=retain(value);owner->second=retain(resource);value->first=retain(owner);resource->first=retain(value);
    drop(owner);drop(value);drop(resource);return owner;
}
static void collect(Node* owner,XL6_OwnershipPlan* plan)
{
    XL6_OwnershipPlanResult state;unsigned turns=0;
    if(!plan){state=xl6OwnershipPlanTryCreateV2(view(owner),resolve,NULL,&plan);assert(state>XL6_OWNERSHIP_PLAN_BUSY);}
    do{
        assert(++turns<12);assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
        assert(xl6OwnershipPlanFinalize(plan));state=xl6OwnershipPlanTryCommit(plan);
    }while(state==XL6_OWNERSHIP_PLAN_PREPARATIONS||state==XL6_OWNERSHIP_PLAN_FINALIZERS);
    assert(state==XL6_OWNERSHIP_PLAN_READY&&xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    for(unsigned round=0;round<25;++round)for(mode=0;mode<6;++mode){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
        unsigned f=finalizers,p=notifications;added=failed_once=busy=false;Node* owner=cycle();XL6_OwnershipPlan* plan=NULL;
        for(malformed=1;malformed<=4;++malformed){unsigned c=counts,t=traces;
            assert(xl6OwnershipPlanTryCreateV2(view(owner),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ERROR&&!plan);
            assert(counts==c&&traces==t);xrtClearError();}
        malformed=0;
        assert(xl6OwnershipPlanTryCreateV2(view(owner),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
        assert(xl6OwnershipPlanFinalize(plan)&&finalizers==f);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
        busy=true;assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_BUSY&&notifications==p&&finalizers==f);busy=false;
        if(mode==0){fail_snapshot_after_notify=true;
            assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_ERROR&&notifications==p+1&&finalizers==f);
            assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtClearError();}
        if(mode==3||mode==4){assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_FAILED&&notifications==p&&finalizers==f);
            assert((xrtGetError()!=NULL)==(mode==4));xrtClearError();}
        if(mode==1){assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY&&escaped);
            assert(xl6OwnershipPlanFinalize(plan)&&finalizers==f);
            assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);
            assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);plan=NULL;
            drop(escaped);escaped=NULL;
        }
        collect(owner,plan);assert(finalizers==f+2&&notifications==p+((mode==2||mode==5)?2u:1u));
        xrtClearError();xrtMemDebugSnapshot(&after);
        assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
            before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
    }
    assert(allocations==500&&frees==500&&notifications==200&&finalizers==300&&probes==200);
    puts("Ownership preparation: 150 actual cycles, 600 pre-trace descriptor refusals, 150 busy paths, 50 explicit preparation failures, 25 resurrections, 50 new preparation duties, 200 notifications before 300 finalizers, 200 cross-thread freezes and zero live delta");
    puts("Preparation snapshot recovery: 25 real allocation failures after completed notification, distinct infrastructure outcome, no repeated semantic duty");return 0;
}
