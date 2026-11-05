#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_foreign_view.h"
#include <assert.h>
#include <stdio.h>

typedef struct Owner { unsigned refs,acquires,releases; bool closed; } Owner;
typedef struct Payload { unsigned destroyed; } Payload;
static int acquire(void* raw)
{Owner* owner=raw;++owner->acquires;if(owner->closed)return 0;++owner->refs;return 1;}
static void release(void* raw)
{Owner* owner=raw;assert(owner->refs);--owner->refs;++owner->releases;}
static bool destroy(void* raw)
{Payload* value=raw;assert(!value->destroyed);++value->destroyed;return true;}
static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG,
    "test.code.continuation",0,1,NULL,NULL,destroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
static bool watchRefused(void* raw,void* storage,void(*invalidate)(void*),void* user)
{(void)raw;(void)storage;(void)invalidate;(void)user;return false;}
static void unwatch(void* storage){(void)storage;assert(!"failed Watch was never attached");}
static bool begin(void* raw,void* storage){(void)raw;(void)storage;return true;}
static bool end(void* storage){(void)storage;return true;}
static bool idle(const void* raw){(void)raw;return false;}
static bool watchChild(void* raw,void* storage,void(*invalidate)(void*),void* user)
{(void)raw;(void)storage;(void)invalidate;(void)user;return true;}
static void unwatchChild(void* storage){(void)storage;}
static int32 workerQuery(void* raw)
{
    XL6_CodeLease* output=(XL6_CodeLease*)(uintptr_t)1;
    assert(!xl6ForeignViewRetainCodeLeaseV1(raw,&output) && !output);
    xrtClearError();return 0;
}
static int32 workerDrop(void* raw)
{assert(xl6ForeignViewReleaseV1(raw));return 0;}
static void runWorker(int32(*proc)(void*),void* raw)
{
    xthread* thread=xrtThreadCreate(proc,raw,0);assert(thread);
    assert(xrtThreadWaitFor(thread,10000)==XWAIT_OK);xrtThreadDestroy(thread);
}
static void ordinaryAndClosedContinuation(void)
{
    Owner owner={0};Payload original={0},continued={0};
    XL6_CodeOwner code={sizeof(code),&owner,acquire,release};
    XL6_ForeignView* view=xl6ForeignViewCreateRootV1(&original,&policy,&code);assert(view);
    assert(owner.refs==1 && owner.acquires==1);
    XL6_CodeLease* lease=NULL;
    xerror* previous=xrtErrorCreate(XERR_STATE,"test",0,"prior diagnostic");assert(previous);
    xrtSetErrorTake(previous);xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ForeignViewRetainCodeLeaseV1(view,&lease) && lease);
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount);
    assert(xrtGetError()==previous && owner.acquires==1 && owner.refs==1);xrtClearError();
    runWorker(workerQuery,view);
    assert(xl6ForeignViewReleaseV1(view) && original.destroyed==1);
    assert(owner.refs==1 && !owner.releases);owner.closed=true;
    view=xl6ForeignViewCreateRootFromLeaseV1(&continued,&policy,NULL,lease);assert(view);
    assert(owner.acquires==1 && owner.refs==1);
    xl6CodeLeaseRelease(lease);lease=NULL;
    runWorker(workerDrop,view);
    assert(!continued.destroyed && owner.refs==1 && xl6ForeignViewPendingV1(0));
    assert(xl6ForeignViewDrainV1() && continued.destroyed==1);
    assert(!owner.refs && owner.acquires==1 && owner.releases==1);
}
static void rejectedAndClaimed(void)
{
    Payload payload={0};XL6_CodeLease* output=(XL6_CodeLease*)(uintptr_t)1;
    assert(!xl6ForeignViewRetainCodeLeaseV1(NULL,&output) && !output);xrtClearError();
    XL6_ForeignViewDescriptorV1 rootPolicy=policy;
    rootPolicy.callBytes=1;rootPolicy.CallBegin=begin;rootPolicy.CallEnd=end;rootPolicy.Busy=idle;
    XL6_ForeignView* view=xl6ForeignViewCreateRootFromLeaseV1(&payload,&rootPolicy,NULL,NULL);assert(view);
    assert(!xl6ForeignViewRetainCodeLeaseV1(view,NULL));xrtClearError();
    assert(xl6ForeignViewRetainCodeLeaseV1(view,&output) && !output);
    XL6_ForeignViewDescriptorV1 childPolicy=policy;
    childPolicy.family="test.code.child";childPolicy.watchBytes=1;
    childPolicy.Watch=watchChild;childPolicy.Unwatch=unwatchChild;
    unsigned nativeChild=0;unsigned char callStorage=0;XL6_ForeignViewCallV1 call={0};
    assert(xl6ForeignViewCallBeginV1(view,&call,&callStorage,sizeof(callStorage)));
    XL6_ForeignView* child=xl6ForeignViewBorrowChildV1(&nativeChild,view,&childPolicy,NULL);assert(child);
    assert(xl6ForeignViewCallEndV1(&call));output=(XL6_CodeLease*)(uintptr_t)1;
    assert(!xl6ForeignViewRetainCodeLeaseV1(child,&output) && !output);xrtClearError();
    assert(xl6ForeignViewReleaseV1(child));
    xrtownershipref ref=xl6ForeignViewOwnershipV1(view);
    const XL6_OwnershipDomainV1* domain=xl6OwnershipDomainForeignViewV1Get();
    const xrtownershipadapterv1* adapter=NULL;
    const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finish=NULL;
    unsigned token=0;xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(domain->Resolve(ref,NULL,&adapter,&preparation,&finish) && adapter);
    assert(adapter->Claim(ref.Data,&token));assert(xrtOwnershipScopeEnd(&freeze));
    output=(XL6_CodeLease*)(uintptr_t)1;
    assert(!xl6ForeignViewRetainCodeLeaseV1(view,&output) && !output);xrtClearError();
    adapter->Restore(ref.Data,&token);
    assert(xl6ForeignViewCloseV1(view) && payload.destroyed==1);
    output=(XL6_CodeLease*)(uintptr_t)1;
    assert(!xl6ForeignViewRetainCodeLeaseV1(view,&output) && !output);xrtClearError();
    assert(xl6ForeignViewReleaseV1(view));
}
static void failurePrefixes(void)
{
    Owner owner={0};XL6_CodeOwner code={sizeof(code),&owner,acquire,release};XL6_CodeLease* parent=NULL;
    assert(xl6CodeLeaseAcquire(&code,&parent)==XL6_CODE_LEASE_ACQUIRED && parent);owner.closed=true;
    unsigned prefixes=0;
    for(size_t point=0;point<64;++point){
        Payload payload={0};assert(xrtMemDebugFailAfter(point));
        XL6_ForeignView* view=xl6ForeignViewCreateRootFromLeaseV1(&payload,&policy,NULL,parent);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
        assert(owner.refs==1 && owner.acquires==1 && !owner.releases);
        if(view){assert(!hit && xl6ForeignViewReleaseV1(view) && payload.destroyed==1);prefixes=(unsigned)point+1;break;}
        assert(hit && !payload.destroyed);
    }
    assert(prefixes);
    Payload payload={0};XL6_ForeignViewDescriptorV1 refused=policy;
    refused.watchBytes=1;refused.Watch=watchRefused;refused.Unwatch=unwatch;
    refused.callBytes=1;refused.CallBegin=begin;refused.CallEnd=end;refused.Busy=idle;
    assert(!xl6ForeignViewCreateRootFromLeaseV1(&payload,&refused,NULL,parent));xrtClearError();
    assert(!payload.destroyed && owner.refs==1 && !owner.releases);
    XL6_OwnershipDomainV1 invalid={sizeof(invalid),NULL,NULL,NULL};
    assert(!xl6ForeignViewCreateRootFromLeaseV1(&payload,&policy,&invalid,parent));xrtClearError();
    assert(!payload.destroyed && owner.refs==1 && !owner.releases);
    xl6CodeLeaseRelease(parent);assert(!owner.refs && owner.releases==1);
    printf("continuation allocation prefixes=%u\n",prefixes);
}
int main(void)
{
    ordinaryAndClosedContinuation();rejectedAndClaimed();failurePrefixes();
    assert(!xl6ForeignViewPendingV1(0) && !xrtGetError());
    puts("code continuation: real lease, no reopened admission, worker retirement, claim/close rejection, rollback PASS");
    return 0;
}
