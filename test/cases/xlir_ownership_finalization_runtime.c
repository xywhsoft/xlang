#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>

typedef struct Node {
    volatile int32 refs;
    struct Node *next,*extra;
    const void* claim;
    uint64 creator;
    unsigned kind,calls,duties,waits;
    bool done,cleared;
} Node;
static unsigned allocated,freed,counts,traces,clears,steps,order[64],orderCount;
static unsigned failureMode,mode;
static bool failedOnce,failSnapshot,reenter;
static Node *busyNode,*escaped;
static const xrtownershipfinalizationv1* selected;
static const xrtownershipfinalizationv1* conflict;
static XL6_OwnershipCatalog* catalog;
static void end(xrtownershipscope* scope){assert(xrtOwnershipScopeEnd(scope));}
static Node* make(unsigned kind)
{Node* n=xrtCalloc(1,sizeof(*n));assert(n);n->refs=1;n->kind=kind;n->creator=xrtThreadCurrentId();++allocated;return n;}
static Node* retain(Node* n){if(n)assert(xrtRefRetain(&n->refs)>0);return n;}
static void drop(const void* data)
{
    Node* n=(Node*)data;if(!n)return;
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    int32 refs=xrtRefRelease(&n->refs);assert(refs>=0);
    if(refs){end(&mutation);return;}
    Node *a=n->next,*b=n->extra;++freed;xrtFree(n);end(&mutation);drop(a);drop(b);
}
static bool count(const void* data,size_t* result)
{const Node* n=data;++counts;assert(!n->cleared);*result=(size_t)n->refs;return n->refs>0;}
static bool trace(const void*,xrtownershipvisitor,void*);
static const xrtownershipops ops={count,trace};
static xrtownershipref ref(const Node* n){return (xrtownershipref){n,n?&ops:NULL};}
static bool trace(const void* data,xrtownershipvisitor visit,void* user)
{const Node* n=data;++traces;return visit(ref(n->next),user)&&visit(ref(n->extra),user);}
static bool hold(const void* data){return retain((Node*)data)!=NULL;}
static bool claim(const void* data,const void* token)
{Node* n=(Node*)data;if(n->claim&&n->claim!=token)return false;n->claim=token;return true;}
static void restore(const void* data,const void* token)
{Node* n=(Node*)data;assert(n->claim==token&&!n->cleared);n->claim=NULL;}
static void clear(const void* data,const void* token)
{Node* n=(Node*)data;assert(n->claim==token&&n->done&&!n->cleared);n->cleared=true;++clears;}
static bool finish(const void* data,const void* token)
{
    Node* n=(Node*)data;assert(n->claim==token&&n->cleared);
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    Node *a=n->next,*b=n->extra;n->next=n->extra=NULL;end(&mutation);drop(a);drop(b);return true;
}
static bool legacy_reject(const void* data,const void* token){(void)data;(void)token;return false;}
static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,restore,legacy_reject,clear,finish};
static bool can_step(const void* data,const void* token)
{const Node* n=data;return token&&n->claim==token&&!n->cleared&&n->creator==xrtThreadCurrentId();}
static bool step(const void* data,const void* token,bool* complete)
{
    Node* n=(Node*)data;assert(can_step(n,token));++n->calls;++steps;
    assert(orderCount<64);order[orderCount++]=n->kind;
    if(reenter){bool failed=false;assert(xl6OwnershipPlanFinalizeStepV1((XL6_OwnershipPlan*)token,&failed)==XL6_OWNERSHIP_STEP_ERROR&&!failed);
        assert(xl6OwnershipPlanTryCommit((XL6_OwnershipPlan*)token)==XL6_OWNERSHIP_PLAN_ERROR);xrtClearError();}
    if(n->done){*complete=true;return true;}
    if(n->kind==0&&mode==1&&!n->duties){escaped=retain(n->next);}
    if(n->kind==0&&mode==2&&!n->extra){Node* c=make(2);c->next=retain(n);n->extra=c;}
    if(n->kind==0&&mode==3&&!n->duties){busyNode=n;++n->duties;*complete=false;return true;}
    if(busyNode==n)busyNode=NULL;
    if(n->waits){--n->waits;++n->duties;*complete=false;return true;}
    if(n->kind==0&&failureMode&&!failedOnce){
        failedOnce=true;++n->duties;n->done=failureMode>=3;*complete=n->done;
        if(failureMode==2||failureMode==4)xrtSetErrorKind(XERR_RANGE);
        if(failSnapshot){failSnapshot=false;assert(xrtMemDebugFailAfter(0));}
        return false;
    }
    ++n->duties;n->done=true;*complete=true;
    if(n->kind==0&&failSnapshot){failSnapshot=false;assert(xrtMemDebugFailAfter(0));}
    return true;
}
static const xrtownershipfinalizationv1 capability={sizeof(capability),XRT_OWNERSHIP_FINALIZATION_V1_TAG,&adapter,can_step,step};
static const xrtownershipfinalizationv1 other={sizeof(other),XRT_OWNERSHIP_FINALIZATION_V1_TAG,&adapter,can_step,step};
static const xrtownershipfinalizationv1 invalid[]={
    {0,XRT_OWNERSHIP_FINALIZATION_V1_TAG,&adapter,can_step,step},
    {sizeof(capability),0,&adapter,can_step,step},
    {sizeof(capability),XRT_OWNERSHIP_FINALIZATION_V1_TAG,NULL,can_step,step},
    {sizeof(capability),XRT_OWNERSHIP_FINALIZATION_V1_TAG,&adapter,NULL,step},
    {sizeof(capability),XRT_OWNERSHIP_FINALIZATION_V1_TAG,&adapter,can_step,NULL}};
static bool load(XL6_OwnershipCatalog* c)
{return xl6OwnershipCatalogAddFinalizationV1(c,selected)&&xl6OwnershipCatalogAddFinalizationV1(c,selected)&&
    (!conflict||xl6OwnershipCatalogAddFinalizationV1(c,conflict));}
static bool resolve(xrtownershipref r,const XL6_OwnershipPoliciesV1* p,const xrtownershipadapterv1** a,
    const xrtownershippreparationv1** prep,const xrtownershipfinishv1** completion)
{(void)p;if(r.Ops!=&ops)return false;*a=&adapter;*prep=NULL;*completion=NULL;return true;}
static bool reads(xrtownershipref r){return r.Ops==&ops;}
static const XL6_OwnershipDomainV1 domain={sizeof(domain),load,resolve,reads};
static XL6_OwnershipAdmissionV1 probe(xrtownershipref r,void* c,const xrtownershipadapterv1** a,
    const xrtownershippreparationv1** p,const xrtownershipfinishv1** f)
{
    *a=NULL;*p=NULL;*f=NULL;
    if(r.Data==busyNode)return XL6_OWNERSHIP_ADMISSION_BUSY;
    return xl6OwnershipCatalogProbeV1(r,c,a,p,f);
}
static Node* cycle(void)
{Node* a=make(0),*b=make(1);a->next=retain(b);b->next=retain(a);drop(b);drop(a);return a;}
static XL6_OwnershipPlan* create(Node* n)
{XL6_OwnershipPlan* p=NULL;assert(xl6OwnershipPlanTryCreateV8(ref(n),probe,NULL,xl6OwnershipCatalogResolveFinalizationV1,false,catalog,NULL,&p)==XL6_OWNERSHIP_PLAN_FINALIZERS);return p;}
static void collect(XL6_OwnershipPlan* p)
{
    for(unsigned i=0;i<12;++i){bool failed=false;
        XL6_OwnershipStepResult s=xl6OwnershipPlanFinalizeStepV1(p,&failed);assert(!failed&&s!=XL6_OWNERSHIP_STEP_ERROR&&s!=XL6_OWNERSHIP_STEP_FAILED);
        if(s!=XL6_OWNERSHIP_STEP_READY)continue;
        XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCommit(p);
        if(status==XL6_OWNERSHIP_PLAN_FINALIZERS)continue;
        assert(status==XL6_OWNERSHIP_PLAN_READY);assert(xl6OwnershipPlanFinishV2(p)==XL6_OWNERSHIP_STEP_READY);
        assert(xl6OwnershipPlanTryDestroy(p)==XL6_OWNERSHIP_PLAN_READY);return;
    }assert(!"semantic continuation did not finish");
}
static void reset(void){mode=failureMode=0;failedOnce=failSnapshot=reenter=false;busyNode=escaped=NULL;orderCount=0;}
static void waits(void)
{
    reset();Node* a=cycle(),*b=a->next;a->waits=2;XL6_OwnershipPlan* p=create(a);bool failed=false;
    assert(xl6OwnershipPlanFinalizeStepV1(p,&failed)==XL6_OWNERSHIP_STEP_WAITING&&!failed);
    assert(a->calls==1&&b->calls==1&&b->done&&!a->done&&order[0]==0&&order[1]==1);
    assert(xl6OwnershipPlanTryCommit(p)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    collect(p);
}
static void failed(bool done,bool diagnostic)
{
    reset();failureMode=done?(diagnostic?4:3):(diagnostic?2:1);
    Node* a=cycle(),*b=a->next;XL6_OwnershipPlan* p=create(a);bool bad=false;
    XL6_OwnershipStepResult status=xl6OwnershipPlanFinalizeStepV1(p,&bad);
    assert(bad&&status==XL6_OWNERSHIP_STEP_WAITING&&a->calls==1&&!b->calls&&a->done==done);
    assert((xrtGetError()!=NULL)==diagnostic);xrtClearError();
    if(!done){
        for(unsigned i=0;i<4;++i){assert(xl6OwnershipPlanFinalizeStepV1(p,&bad)==XL6_OWNERSHIP_STEP_WAITING&&!bad);assert(a->calls==1&&!b->calls);}
        assert(xl6OwnershipPlanRetryFinalizationV1(p));
    }
    collect(p);
}
static void snapshot_failure(bool semantic)
{
    reset();failureMode=semantic?3:0;failSnapshot=true;Node* a=cycle();XL6_OwnershipPlan* p=create(a);bool bad=false;
    assert(xl6OwnershipPlanFinalizeStepV1(p,&bad)==XL6_OWNERSHIP_STEP_ERROR&&bad==semantic);
    assert(a->done&&a->calls==1&&xrtMemDebugFailTriggered());
    if(semantic)assert(!xrtGetError());
    xrtMemDebugFailClear();xrtClearError();collect(p);
}
static void new_peer(void)
{
    reset();mode=2;Node* a=cycle();XL6_OwnershipPlan* p=create(a);bool bad=false;
    assert(xl6OwnershipPlanFinalizeStepV1(p,&bad)==XL6_OWNERSHIP_STEP_WAITING&&!bad);
    assert(a->extra&&!a->extra->calls&&orderCount==2&&order[0]==0&&order[1]==1);collect(p);
}
static void resurrect(void)
{
    reset();mode=1;Node* a=cycle();XL6_OwnershipPlan* p=create(a);bool bad=false;
    assert(xl6OwnershipPlanFinalizeStepV1(p,&bad)==XL6_OWNERSHIP_STEP_READY&&!bad);
    assert(escaped&&a->done&&!a->next->calls);
    assert(xl6OwnershipPlanTryCommit(p)==XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6OwnershipPlanTryDestroy(p)==XL6_OWNERSHIP_PLAN_READY);
    unsigned duties=a->duties;drop(escaped);escaped=NULL;mode=0;p=create(a);collect(p);(void)duties;
}
static void read_busy(void)
{
    reset();mode=3;Node* a=cycle();XL6_OwnershipPlan* p=create(a);bool bad=false;
    assert(xl6OwnershipPlanFinalizeStepV1(p,&bad)==XL6_OWNERSHIP_STEP_WAITING&&!bad);
    assert(a->calls==1&&!a->next->calls&&busyNode==a);
    assert(xl6OwnershipPlanTryRefresh(p)==XL6_OWNERSHIP_PLAN_BUSY);
    collect(p);assert(!busyNode);
}
static void descriptors(void)
{
    const XL6_OwnershipDomainV1* domains[]={&domain};
    for(unsigned i=0;i<5;++i){selected=&invalid[i];unsigned c=counts,t=traces;
        assert(!xl6OwnershipCatalogCreateV1(domains,1));assert(counts==c&&traces==t);xrtClearError();}
    selected=&capability;conflict=&other;assert(!xl6OwnershipCatalogCreateV1(domains,1));xrtClearError();conflict=NULL;
    catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
    reset();Node* a=cycle();XL6_OwnershipPlan* p=create(a);
    /* Legacy plans receive the same fail-closed adapter but no continuation. */
    assert(xl6OwnershipPlanTryDestroy(p)==XL6_OWNERSHIP_PLAN_READY);
    p=NULL;assert(xl6OwnershipPlanTryCreateV7(ref(a),probe,NULL,false,catalog,NULL,&p)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    assert(xl6OwnershipPlanFinalizeV2(p)==XL6_OWNERSHIP_STEP_FAILED&&!a->calls);xrtClearError();
    assert(xl6OwnershipPlanTryDestroy(p)==XL6_OWNERSHIP_PLAN_READY);
    collect(create(a));
}
static void ambient_error(void)
{
    for(unsigned which=0;which<3;++which){
        reset();failureMode=which?which:0;Node* a=cycle();XL6_OwnershipPlan* p=create(a);
        xrtSetErrorKind(XERR_ARGUMENT);const xerror* ambient=xrtGetError();assert(ambient);bool failed=false;
        XL6_OwnershipStepResult status=xl6OwnershipPlanFinalizeStepV1(p,&failed);
        assert(failed==(which!=0));
        if(!which){assert(status==XL6_OWNERSHIP_STEP_READY&&xrtGetError()==ambient);}
        else if(which==1)assert(!xrtGetError());
        else assert(xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_RANGE);
        xrtClearError();if(which)assert(xl6OwnershipPlanRetryFinalizationV1(p));collect(p);
    }
}
static int32 worker(void* data)
{
    bool failed=false;XL6_OwnershipStepResult status=xl6OwnershipPlanFinalizeStepV1(data,&failed);
    assert(status==XL6_OWNERSHIP_STEP_WAITING&&!failed&&!xrtGetError());return 0;
}
static void affinity(void)
{
    reset();Node* a=cycle();XL6_OwnershipPlan* p=create(a);
    xthread* thread=xrtThreadCreate(worker,p,0);assert(thread);
    assert(xrtThreadWaitFor(thread,10000)==XWAIT_OK);xrtThreadDestroy(thread);
    assert(!a->calls&&!a->next->calls);collect(p);
}
static void legacy_wait(void)
{
    reset();Node* a=cycle();a->waits=1;XL6_OwnershipPlan* p=create(a);
    assert(xl6OwnershipPlanFinalizeV2(p)==XL6_OWNERSHIP_STEP_FAILED);
    assert(a->calls==1&&a->next->calls==1&&!a->done&&a->next->done);collect(p);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    descriptors();
    for(unsigned i=0;i<20;++i){waits();failed(false,false);failed(false,true);failed(true,false);failed(true,true);snapshot_failure(false);snapshot_failure(true);new_peer();resurrect();read_busy();}
    ambient_error();affinity();legacy_wait();
    reset();reenter=true;collect(create(cycle()));
    xl6OwnershipCatalogDestroyV1(catalog);xrtClearError();xrtMemDebugSnapshot(&after);
    assert(allocated==freed&&clears==freed);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    printf("Semantic finalization: %u nodes, %u bounded steps, exact WAIT/failure/completion, explicit failed retry, graph refresh/resurrection/new peers, legacy refusal and zero live delta\n",freed,steps);
    return 0;
}
