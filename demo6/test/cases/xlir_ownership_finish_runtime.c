#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>

typedef struct Node {
    volatile int32 refs;
    struct Node* next;
    const void* claim;
    unsigned kind, calls;
    bool cleared;
} Node;
static unsigned allocated, freed, clears, counts, traces, badDescriptor, reentries;
static bool replaceDescriptor, legacy, reenter;
static XL6_OwnershipStepResult answers[2];
static void end_scope(xrtownershipscope* s) { assert(xrtOwnershipScopeEnd(s)); }
static Node* hold_node(Node* n) { assert(xrtRefRetain(&n->refs)>0); return n; }
static void drop(const void* data)
{
    Node* n=(Node*)data; xrtownershipscope mutation={0}; Node* next;
    assert(xrtOwnershipMutationBegin(&mutation));
    int32 count=xrtRefRelease(&n->refs); assert(count>=0);
    if(count){end_scope(&mutation);return;}
    next=n->next; ++freed; xrtFree(n); end_scope(&mutation);
    if(next)drop(next);
}
static bool count_node(const void* data,size_t* count)
{ const Node* n=data; ++counts; assert(!n->cleared); *count=(size_t)n->refs; return n->refs>0; }
static bool trace_node(const void*,xrtownershipvisitor,void*);
static const xrtownershipops ops={count_node,trace_node};
static xrtownershipref view(const Node* n) { return (xrtownershipref){n,n?&ops:NULL}; }
static bool trace_node(const void* data,xrtownershipvisitor visit,void* context)
{ const Node* n=data; ++traces; assert(!n->cleared); return visit(view(n->next),context); }
static bool hold(const void* data) { return hold_node((Node*)data)!=NULL; }
static bool claim(const void* data,const void* token)
{ Node* n=(Node*)data; if(n->claim && n->claim!=token)return false; n->claim=token; return true; }
static void restore(const void* data,const void* token)
{ Node* n=(Node*)data; assert(n->claim==token && !n->cleared); n->claim=NULL; }
static void clear(const void* data,const void* token)
{
    Node* n=(Node*)data; Node* next=n->next;
    assert(n->claim==token && !n->cleared && next->refs>=2);
    n->next=NULL; n->cleared=true; ++clears; drop(next);
}
static XL6_OwnershipStepResult finish(const void* data,const void* token)
{
    Node* n=(Node*)data; xrtownershipscope proof={0};
    assert(n->claim==token && n->cleared && n->refs==1);
    assert(xrtOwnershipFreezeTryBegin(&proof)); end_scope(&proof); ++n->calls;
    if(reenter){
        XL6_OwnershipPlan* plan=(XL6_OwnershipPlan*)token;
        assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_ERROR);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_ERROR);
        xrtClearError(); ++reentries;
    }
    return answers[n->kind];
}
static bool finish_bool(const void* data,const void* token)
{ return finish(data,token)==XL6_OWNERSHIP_STEP_READY; }
static const XL6_OwnershipAdapterV1 adapter={sizeof(adapter),hold,drop,claim,restore,NULL,clear,finish_bool};
static const XL6_OwnershipFinishV1 completion={sizeof(completion),&adapter,finish};
static const XL6_OwnershipFinishV1 replacement={sizeof(replacement),&adapter,finish};
static const XL6_OwnershipFinishV1 invalid[]={
    {0,&adapter,finish},{sizeof(completion),NULL,finish},{sizeof(completion),&adapter,NULL}};
static bool resolve(xrtownershipref ref,void* context,const XL6_OwnershipAdapterV1** lifecycle,
    const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** completed)
{
    (void)context; if(ref.Ops!=&ops)return false;
    *lifecycle=&adapter; *preparation=NULL;
    *completed=badDescriptor?&invalid[badDescriptor-1]:legacy?NULL:replaceDescriptor?&replacement:&completion;
    return true;
}
static Node* cycle(void)
{
    Node* a=xrtCalloc(1,sizeof(*a)),*b=xrtCalloc(1,sizeof(*b)); assert(a&&b);
    a->refs=b->refs=1;b->kind=1;allocated+=2;
    a->next=hold_node(b);b->next=hold_node(a);drop(b);drop(a);return a;
}
static XL6_OwnershipPlan* commit(Node* root)
{
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV3(view(root),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_ERROR);xrtClearError();
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);return plan;
}
static void complete(XL6_OwnershipPlan* plan)
{
    answers[0]=answers[1]=XL6_OWNERSHIP_STEP_READY;
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
}
static void busy_cycle(void)
{
    Node* a=cycle(),*b=a->next; unsigned priorFrees=freed; XL6_OwnershipPlan* plan=commit(a);
    answers[0]=answers[1]=XL6_OWNERSHIP_STEP_BUSY;
    xrtSetErrorKind(XERR_STATE);const xerror* error=xrtGetError();assert(error);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_BUSY);
    assert(!xrtMemDebugFailTriggered() && xrtGetError()==error);xrtMemDebugFailClear();xrtClearError();
    assert(a->calls==1 && b->calls==1 && freed==priorFrees);
    answers[1]=XL6_OWNERSHIP_STEP_READY;
    for(unsigned n=0;n<8;++n)assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_BUSY);
    assert(a->calls==9 && b->calls==2 && freed==priorFrees);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_ERROR);xrtClearError();
    answers[0]=XL6_OWNERSHIP_STEP_READY;
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(a->calls==10 && b->calls==2 && freed==priorFrees);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY && a->calls==10 && b->calls==2);
    complete(plan);assert(freed==priorFrees+2);
}
static void failed_cycle(bool old)
{
    legacy=old;Node* a=cycle(),*b=a->next;XL6_OwnershipPlan* plan=commit(a);
    answers[0]=XL6_OWNERSHIP_STEP_FAILED;answers[1]=XL6_OWNERSHIP_STEP_READY;
    assert(xrtMemDebugFailAfter(0));
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_FAILED);
    assert(!xrtGetError() && !xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(a->calls==1 && b->calls==0);complete(plan);legacy=false;
}
static void invalid_cycle(void)
{
    Node* a=cycle();XL6_OwnershipPlan* plan=commit(a);
    answers[0]=(XL6_OwnershipStepResult)99;
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_FAILED && xrtGetError());
    xrtClearError();complete(plan);
}
static void descriptors(void)
{
    Node* a=cycle();XL6_OwnershipPlan* plan=NULL;
    for(badDescriptor=1;badDescriptor<=3;++badDescriptor){
        unsigned c=counts,t=traces;
        assert(xl6OwnershipPlanTryCreateV3(view(a),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan);
        assert(counts==c && traces==t && a->refs==1 && !a->claim);xrtClearError();
    }
    badDescriptor=0;
    assert(xl6OwnershipPlanTryCreateV3(view(a),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    replaceDescriptor=true;unsigned c=counts,t=traces;
    assert(xl6OwnershipPlanTryRefresh(plan)==XL6_OWNERSHIP_PLAN_ERROR);
    assert(counts==c && traces==t && a->refs==2 && a->claim==plan);xrtClearError();
    replaceDescriptor=false;assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    complete(commit(a));
}
int main(void)
{
    xmemdebugsnapshot before,after;assert(xrtMemDebugEnable(true));
    xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
    for(unsigned i=0;i<100;++i){busy_cycle();failed_cycle(false);failed_cycle(true);invalid_cycle();descriptors();}
    reenter=true;Node* a=cycle();complete(commit(a));reenter=false;
    xrtClearError();xrtMemDebugSnapshot(&after);
    assert(allocated==freed && clears==freed && reentries==2);
    assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    printf("Ownership completion: 501 real cycles, 900 BUSY turns, 200 diagnostic-free failures, 100 invalid outcomes, 400 pre-trace descriptor refusals, 2 rejected reentries; exact pins, no repeated completed duty, zero live delta\n");
    return 0;
}
