#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Native strong-owning allocations with actual slots, not estimated roots.
 * They exercise the generic transaction independently of module adapters. */
typedef struct Node {
    volatile int32 refs;
    struct Node *first,*second;
    const XL6_OwnershipPlan* claim;
    unsigned kind;
    unsigned preparationPhase,preparationCalls;
    bool finalized,cleared;
} Node;
static Node* escaped;
static unsigned allocations,frees,finalizers,clears,finishes,mode,admissions;
static bool deny;
static unsigned boundaryDepth;
static void mutation_begin(xrtownershipscope* scope){assert(xrtOwnershipMutationBegin(scope));}
static void mutation_end(xrtownershipscope* scope){assert(xrtOwnershipScopeEnd(scope));}
static Node* make(unsigned kind)
{Node* n=xrtCalloc(1,sizeof(*n));assert(n);n->refs=1;n->kind=kind;++allocations;return n;}
static Node* retain(Node* n){if(n)assert(xrtRefRetain(&n->refs)>0);return n;}
static void drop(const void* data)
{
    Node* n=(Node*)data;Node *first,*second;xrtownershipscope mutation={0};
    if(!n)return;
    mutation_begin(&mutation);
    int32 count=xrtRefRelease(&n->refs);assert(count>=0);
    if(count){mutation_end(&mutation);return;}
    first=n->first;second=n->second;n->first=n->second=NULL;++frees;xrtFree(n);mutation_end(&mutation);
    drop(second);drop(first);
}
static bool count_node(const void* data,size_t* count)
{const Node* n=data;if(n->refs<=0 || n->cleared)return false;*count=n->refs;return true;}
static bool trace_node(const void* data,xrtownershipvisitor visit,void* context);
static const xrtownershipops node_ops={count_node,trace_node};
static xrtownershipref view(const Node* n){return (xrtownershipref){n,n?&node_ops:NULL};}
static bool trace_node(const void* data,xrtownershipvisitor visit,void* context)
{const Node* n=data;size_t count;return count_node(data,&count) && visit(view(n->first),context) && visit(view(n->second),context);}
static bool hold(const void* data){return retain((Node*)data)!=NULL;}
static bool claim(const void* data,const void* plan)
{Node* n=(Node*)data;if(n->claim && n->claim!=plan)return false;n->claim=plan;return true;}
static void restore(const void* data,const void* plan)
{Node* n=(Node*)data;assert(n->claim==plan && !n->cleared);n->claim=NULL;}
static bool finalize(const void* data,const void* plan)
{
    Node* n=(Node*)data; xrtownershipscope proof={0};
    assert(!boundaryDepth);
    assert(n->claim==plan && !n->cleared && xrtOwnershipFreezeTryBegin(&proof));mutation_end(&proof);
    if(n->finalized)return true;
    n->finalized=true;++finalizers;
    if(n->kind==1 && mode==1){assert(!escaped);escaped=retain(n);}
    if(n->kind==1 && mode==2){
        xrtownershipscope mutation={0};mutation_begin(&mutation);
        Node* child=make(2);child->first=retain(n->first);assert(!n->first->second);n->first->second=child;
        mutation_end(&mutation);
    }
    if(n->kind==1 && mode==3){xrtSetErrorKind(XERR_STATE);return false;}
    return true;
}
static void clear(const void* data,const void* plan)
{
    Node* n=(Node*)data;Node *first=n->first,*second=n->second;xrtownershipscope busy={0};
    assert(n->claim==plan && n->finalized && !n->cleared && !xrtOwnershipFreezeTryBegin(&busy));
    n->first=n->second=NULL;n->cleared=true;++clears;
    /* Each target still has the plan's real hold; no final release can occur. */
    if(first)assert(first->refs>=2);
    if(second)assert(second->refs>=2);
    drop(second);drop(first);
}
static bool finish(const void* data,const void* plan)
{
    const Node* n=data;xrtownershipscope proof={0};
    assert(n->claim==plan && n->cleared && xrtOwnershipFreezeTryBegin(&proof));mutation_end(&proof);++finishes;return true;
}
static const XL6_OwnershipAdapterV1 adapter={sizeof(adapter),hold,drop,claim,restore,finalize,clear,finish};
static const XL6_OwnershipAdapterV1* resolve(xrtownershipref ref,void* context)
{(void)context;++admissions;return !deny && ref.Ops==&node_ops?&adapter:NULL;}
static Node* cycle(void)
{
    Node* owner=make(0),*object=make(1);
    owner->first=retain(object);object->first=retain(owner);drop(object);drop(owner);
    assert(owner->refs==1 && owner->first->refs==1);return owner;
}
static void end_plan(XL6_OwnershipPlan* plan)
{assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);}
static void collect(Node* root)
{
    XL6_OwnershipPlan* plan=NULL;XL6_OwnershipPlanResult result;
    assert(xl6OwnershipPlanTryCreate(view(root),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    do{assert(xl6OwnershipPlanFinalize(plan));result=xl6OwnershipPlanTryCommit(plan);}
    while(result==XL6_OWNERSHIP_PLAN_FINALIZERS);
    assert(result==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));end_plan(plan);
}
static void check_balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes && after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
}
static void transaction(unsigned selected)
{
    xmemdebugsnapshot before;XL6_OwnershipPlan* plan=NULL;Node* root;
    mode=selected;xrtMemDebugSnapshot(&before);root=cycle();
    if(selected==0 || selected==2)collect(root);
    else{
        unsigned ran=finalizers;
        assert(xl6OwnershipPlanTryCreate(view(root),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
        bool ok=xl6OwnershipPlanFinalize(plan);
        if(selected==1){assert(ok && escaped && xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED && !root->cleared && !escaped->cleared);}
        else assert(!ok && xrtGetError() && xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ERROR);
        end_plan(plan);assert(root->refs==1 && root->first->first==root && finalizers-ran==2);
        if(escaped){drop(escaped);escaped=NULL;}xrtClearError();mode=0;collect(root);
        assert(finalizers-ran==2); /* physical finalizers are never repeated */
    }
    check_balance(&before);
}
static unsigned fault_sweep(void)
{
    unsigned faults=0;
    for(unsigned round=0;round<20;++round)for(unsigned point=0;point<64;++point){
        xmemdebugsnapshot before;XL6_OwnershipPlan* plan=NULL;xrtMemDebugSnapshot(&before);Node* root=cycle();
        assert(xrtMemDebugFailAfter(point));XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCreate(view(root),resolve,NULL,&plan);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(failed){assert(result==XL6_OWNERSHIP_PLAN_ERROR && !plan && root->refs==1 && root->first->refs==1 && !root->claim && !root->first->claim);++faults;xrtClearError();}
        else{assert(result==XL6_OWNERSHIP_PLAN_FINALIZERS);end_plan(plan);}
        mode=0;collect(root);check_balance(&before);if(!failed)break;assert(point!=63);
    }
    return faults;
}
/* A pinned/claimed asynchronous preparation can make its own read admission
 * BUSY until a continuation polls it. Real TaskPool/TLS join exercises this
 * transition too; this small physical self-cycle makes it deterministic. */
static bool transition_trace(const void*,xrtownershipvisitor,void*);
static bool transition_count(const void* raw,size_t* output)
{const Node* node=raw;return node->preparationPhase!=1&&count_node(raw,output);}
static const xrtownershipops transition_ops={transition_count,transition_trace};
static xrtownershipref transition_view(const Node* node)
{return (xrtownershipref){node,node?&transition_ops:NULL};}
static bool transition_trace(const void* raw,xrtownershipvisitor visit,void* context)
{const Node* node=raw;size_t count;return transition_count(raw,&count)&&visit(transition_view(node->first),context)&&visit(transition_view(node->second),context);}
static bool transition_ready(const void* raw){return ((const Node*)raw)->preparationPhase==2;}
static xrtownershipprepareresult transition_prepare(const void* raw,const void* token)
{
    Node* node=(Node*)raw;xrtownershipscope proof={0},mutation={0};
    assert(node->claim==token&&xrtOwnershipFreezeTryBegin(&proof));mutation_end(&proof);
    mutation_begin(&mutation);++node->preparationCalls;
    node->preparationPhase=node->preparationPhase?2:1;
    mutation_end(&mutation);
    return node->preparationPhase==2?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static bool transition_probe(xrtownershipref ref,void* context,const xrtownershipadapterv1** selected,
    const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** completion);
static XL6_OwnershipAdmissionV1 transition_admit(xrtownershipref ref,void* context,const xrtownershipadapterv1** selected,
    const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** completion)
{
    *selected=NULL;*preparation=NULL;*completion=NULL;
    const Node* node=ref.Data;
    if(ref.Ops!=&transition_ops||!node||node->kind==99)return XL6_OWNERSHIP_ADMISSION_REFUSED;
    if(node->preparationPhase==1)return XL6_OWNERSHIP_ADMISSION_BUSY;
    return transition_probe(ref,context,selected,preparation,completion)?XL6_OWNERSHIP_ADMISSION_ADMITTED:XL6_OWNERSHIP_ADMISSION_REFUSED;
}
static bool transition_probe(xrtownershipref ref,void* context,const xrtownershipadapterv1** selected,
    const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** completion)
{
    static const xrtownershipadapterv1 lifecycle={sizeof(lifecycle),hold,drop,claim,restore,finalize,clear,finish};
    static const xrtownershippreparationv1 policy={sizeof(policy),&lifecycle,transition_ready,transition_prepare};
    (void)context;(void)completion;
    if(ref.Ops!=&transition_ops)return false;
    *selected=&lifecycle;*preparation=&policy;return true;
}
static void transition_round(unsigned change)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Node* root=make(0);
    root->first=retain(root);drop(root);XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV6(transition_view(root),transition_admit,NULL,false,NULL,&plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
    xrtownershipscope mutation={0};mutation_begin(&mutation);root->preparationPhase=1;mutation_end(&mutation);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_BUSY&&root->preparationCalls==0); /* BUSY grants no NEW duty */
    mutation_begin(&mutation);root->preparationPhase=0;mutation_end(&mutation);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_BUSY&&root->preparationPhase==1&&root->preparationCalls==1);
    assert(!root->cleared&&xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_BUSY);
    if(change==1)escaped=retain(root); /* a real external root, not a guessed RC */
    if(change==2){mutation_begin(&mutation);root->second=make(99);mutation_end(&mutation);}
    XL6_OwnershipStepResult resumed=xl6OwnershipPlanPrepare(plan);
    assert(root->preparationPhase==2&&root->preparationCalls==2); /* continuation must run despite prior BUSY */
    if(change==2){
        assert(resumed==XL6_OWNERSHIP_STEP_ERROR&&!root->cleared);xrtClearError();end_plan(plan);
        mutation_begin(&mutation);Node* unknown=root->second;root->second=NULL;mutation_end(&mutation);drop(unknown);
    }else if(change==1){
        assert(resumed==XL6_OWNERSHIP_STEP_READY&&xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED&&!root->cleared);
        end_plan(plan);drop(escaped);escaped=NULL;
    }else{
        assert(resumed==XL6_OWNERSHIP_STEP_READY&&xl6OwnershipPlanFinalize(plan));
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY&&xl6OwnershipPlanFinish(plan));end_plan(plan);check_balance(&before);return;
    }
    plan=NULL;
    assert(xl6OwnershipPlanTryCreateV6(transition_view(root),transition_admit,NULL,false,NULL,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    assert(xl6OwnershipPlanFinalize(plan)&&xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY&&xl6OwnershipPlanFinish(plan));
    end_plan(plan);check_balance(&before);
}
typedef struct NativeBoundary {
    bool busy;unsigned enters,leaves,discoveries;Node* extra;
} NativeBoundary;
static bool boundary_enter(void* raw)
{
    NativeBoundary* b=raw;assert(!boundaryDepth);
    if(b->busy)return false;
    ++boundaryDepth;++b->enters;return true;
}
static void boundary_leave(void* raw)
{NativeBoundary* b=raw;assert(boundaryDepth==1);--boundaryDepth;++b->leaves;}
static bool boundary_discover(xrtownershipvisitor visit,void* context,void* raw)
{
    NativeBoundary* b=raw;assert(boundaryDepth==1);++b->discoveries;
    return !b->extra||visit(view(b->extra),context);
}
static XL6_OwnershipAdmissionV1 boundary_probe(xrtownershipref ref,void* context,
    const XL6_OwnershipAdapterV1** selected,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** completion)
{
    (void)context;assert(boundaryDepth==1);*selected=NULL;*preparation=NULL;*completion=NULL;
    if(ref.Ops!=&node_ops)return XL6_OWNERSHIP_ADMISSION_REFUSED;
    *selected=&adapter;return XL6_OWNERSHIP_ADMISSION_ADMITTED;
}
static void native_boundary_round(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);mode=0;
    Node* root=cycle();NativeBoundary b={0};b.extra=cycle();
    XL6_OwnershipBoundaryV1 boundary={sizeof(boundary),boundary_enter,boundary_leave,boundary_discover,&b};
    XL6_OwnershipBoundaryV1 invalid=boundary;--invalid.size;
    XL6_OwnershipPlan* plan=(XL6_OwnershipPlan*)(uintptr_t)1;
    assert(xl6OwnershipPlanTryCreateV7(view(root),boundary_probe,NULL,false,NULL,&invalid,&plan)==XL6_OWNERSHIP_PLAN_ERROR);
    assert(plan==(XL6_OwnershipPlan*)(uintptr_t)1&&!b.enters);xrtClearError();
    b.busy=true;xrtSetErrorKind(XERR_STATE);const xerror* prior=xrtGetError();
    assert(xl6OwnershipPlanTryCreateV7(view(root),boundary_probe,NULL,false,NULL,&boundary,&plan)==XL6_OWNERSHIP_PLAN_BUSY);
    assert(plan==(XL6_OwnershipPlan*)(uintptr_t)1&&prior==xrtGetError()&&!b.enters);xrtClearError();
    b.busy=false;plan=NULL;
    assert(xl6OwnershipPlanTryCreateV7(view(root),boundary_probe,NULL,false,NULL,&boundary,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    assert(b.discoveries&&b.enters==b.leaves&&!boundaryDepth);
    b.busy=true;assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_BUSY);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_BUSY);
    b.busy=false;assert(xl6OwnershipPlanFinalize(plan));
    b.busy=true;assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_BUSY);b.busy=false;
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY&&xl6OwnershipPlanFinish(plan));end_plan(plan);
    assert(b.enters==b.leaves&&!boundaryDepth);check_balance(&before);
    for(unsigned point=0;point<40;++point){
        xrtMemDebugSnapshot(&before);root=cycle();b=(NativeBoundary){0};plan=NULL;
        assert(xrtMemDebugFailAfter(point));
        XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCreateV7(view(root),boundary_probe,NULL,false,NULL,&boundary,&plan);
        xrtMemDebugFailClear();xrtClearError();
        assert(!boundaryDepth&&b.enters==b.leaves);
        if(status==XL6_OWNERSHIP_PLAN_ERROR){assert(!plan);collect(root);}
        else {assert(status==XL6_OWNERSHIP_PLAN_FINALIZERS&&plan);end_plan(plan);collect(root);}
        assert(b.enters==b.leaves&&!boundaryDepth);check_balance(&before);
    }
}
int main(void)
{
    xmemdebugsnapshot before;unsigned faults;assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
    for(unsigned n=0;n<100;++n)for(unsigned kind=0;kind<4;++kind)transaction(kind);
    for(unsigned n=0;n<100;++n){
        Node* root=cycle();XL6_OwnershipPlan* plan=NULL;xrtownershipscope busy={0};
        mutation_begin(&busy);assert(xl6OwnershipPlanTryCreate(view(root),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_BUSY && !plan);mutation_end(&busy);
        deny=true;assert(xl6OwnershipPlanTryCreate(view(root),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan);deny=false;xrtClearError();mode=0;collect(root);
    }
    for(unsigned n=0;n<32;++n)for(unsigned change=0;change<3;++change)transition_round(change);
    native_boundary_round();
    faults=fault_sweep();check_balance(&before);assert(allocations==frees && clears==finishes);
    puts("Ownership preparation continuation: 96 real self-cycles across BUSY-to-ready, late external root and unknown child; fresh graph proof still rejects Clear");
    printf("Ownership plans: 400 physical cycles across commit/resurrection/new-node/finalizer-error paths, 100 busy and 100 admission refusals, %u real preparation faults; %u allocations retired, zero live delta; adapter engine, not module acceptance\n",faults,frees);
    return 0;
}
