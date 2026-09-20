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
    bool finalized,cleared;
} Node;
static Node* escaped;
static unsigned allocations,frees,finalizers,clears,finishes,mode,admissions;
static bool deny;
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
int main(void)
{
    xmemdebugsnapshot before;unsigned faults;assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
    for(unsigned n=0;n<100;++n)for(unsigned kind=0;kind<4;++kind)transaction(kind);
    for(unsigned n=0;n<100;++n){
        Node* root=cycle();XL6_OwnershipPlan* plan=NULL;xrtownershipscope busy={0};
        mutation_begin(&busy);assert(xl6OwnershipPlanTryCreate(view(root),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_BUSY && !plan);mutation_end(&busy);
        deny=true;assert(xl6OwnershipPlanTryCreate(view(root),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan);deny=false;xrtClearError();mode=0;collect(root);
    }
    faults=fault_sweep();check_balance(&before);assert(allocations==frees && clears==finishes);
    printf("Ownership plans: 400 physical cycles across commit/resurrection/new-node/finalizer-error paths, 100 busy and 100 admission refusals, %u real preparation faults; %u allocations retired, zero live delta; adapter engine, not module acceptance\n",faults,frees);
    return 0;
}
