#include <__xl_runtime/xl6_foreign_view.h>
#include <__xl_runtime/xrt_decl.h>
#include <assert.h>
#include <stdio.h>

typedef struct Watch Watch;
struct Watch {
    Watch* next;
    Watch** previous;
    void (*invalidate)(void*);
    void* user;
};

typedef struct {
    Watch* watches;
    unsigned owners;
    unsigned activity;
    int value;
} Parent;

typedef struct { Parent* parent; } Scope;

static unsigned attached, detached, consumed;

static bool watch(void* raw,void* storage,void (*invalidate)(void*),void* user)
{
    Parent* parent=raw;
    Watch* link=storage;
    assert(!link->previous && !link->next && parent->owners);
    *link=(Watch){parent->watches,&parent->watches,invalidate,user};
    if (link->next) link->next->previous=&link->next;
    parent->watches=link;
    ++attached;
    return true;
}

static void unwatch(void* storage)
{
    Watch* link=storage;
    assert(link->previous);
    *link->previous=link->next;
    if (link->next) link->next->previous=link->previous;
    *link=(Watch){0};
    ++detached;
}

static void replaceNode(Parent* parent,int value)
{
    while (parent->watches) {
        Watch* link=parent->watches;
        void (*notify)(void*)=link->invalidate;
        void* user=link->user;
        unwatch(link);
        notify(user);
    }
    parent->value=value;
}

static bool destroy(void* raw)
{
    Parent* parent=raw;
    assert(parent->owners);
    ++consumed;
    if (!--parent->owners) replaceNode(parent,0);
    return true;
}

static bool begin(void* raw,void* storage)
{
    Parent* parent=raw;
    Scope* scope=storage;
    assert(parent->owners);
    ++parent->activity;
    scope->parent=parent;
    return true;
}

static bool end(void* storage)
{
    Scope* scope=storage;
    assert(scope->parent && scope->parent->activity);
    --scope->parent->activity;
    scope->parent=NULL;
    return true;
}

static const XL6_ForeignViewDescriptorV1 ownerPolicy={
    .size=sizeof(ownerPolicy),.abiTag=XL6_FOREIGN_VIEW_V1_TAG,.family="observer.root",
    .watchAlignment=1,.Destroy=destroy,.callBytes=sizeof(Scope),.callAlignment=_Alignof(Scope),
    .CallBegin=begin,.CallEnd=end,.flags=XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY
};
static const XL6_ForeignViewDescriptorV1 observerPolicy={
    .size=sizeof(observerPolicy),.abiTag=XL6_FOREIGN_VIEW_V1_TAG,.family="observer.child",
    .watchBytes=sizeof(Watch),.watchAlignment=_Alignof(Watch),.Watch=watch,.Unwatch=unwatch,.callAlignment=1
};

static XL6_ForeignView* getChildWithLease(XL6_ForeignView* owner,const XL6_CodeOwner* code)
{
    Scope native={0};
    XL6_ForeignViewCallV1 call={0};
    assert(xl6ForeignViewCallBeginV1(owner,&call,&native,sizeof(native)));
    XL6_ForeignView* child=xl6ForeignViewBorrowChildV1(call.raw,owner,&observerPolicy,code);
    /* Failed admission leaves the surrounding native scope closable. */
    xerror* failure=xrtTakeError();
    assert(xl6ForeignViewCallEndV1(&call));
    xrtSetErrorTake(failure);
    return child;
}

static XL6_ForeignView* getChild(XL6_ForeignView* owner)
{
    return getChildWithLease(owner,NULL);
}

static void checkExpired(XL6_ForeignView* child)
{
    Scope native={0};
    XL6_ForeignViewCallV1 call={0};
    assert(!xl6ForeignViewCallBeginV1(child,&call,&native,sizeof(native)));
    xrtClearError();
}

static int32 releaseOnWorker(void* raw)
{
    assert(!xl6ForeignViewCloseV1(raw));
    xrtClearError();
    assert(xl6ForeignViewReleaseV1(raw));
    return 0;
}

static void checkOwnership(void)
{
    unsigned previousConsumed=consumed;
    Parent parent={.owners=2,.value=7};
    XL6_ForeignView* owner=xl6ForeignViewCreateRootV1(&parent,&ownerPolicy,NULL);
    XL6_ForeignView* peer=xl6ForeignViewCreateRootV1(&parent,&ownerPolicy,NULL);
    assert(owner && peer);
    assert(!xl6ForeignViewCreateRootV1(&parent,&observerPolicy,NULL));
    xrtClearError();
    XL6_ForeignView* child=getChild(owner);
    assert(child && parent.watches && parent.owners==2);

    unsigned initialWatches=attached;
    assert(xrtMemDebugFailAfter(0));
    for (unsigned i=0;i<1000;++i) {
        XL6_ForeignView* alias=getChild(owner);
        assert(alias==child && xl6ForeignViewReleaseV1(alias));
    }
    assert(attached==initialWatches && !xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();

    Scope native={0};
    XL6_ForeignViewCallV1 call={0};
    assert(xl6ForeignViewCallBeginV1(owner,&call,&native,sizeof(native)));
    assert(xl6ForeignViewReleaseV1(child));
    child=xl6ForeignViewBorrowChildV1(&parent,owner,&observerPolicy,NULL);
    assert(child && attached==initialWatches);
    assert(xl6ForeignViewCallEndV1(&call) && parent.watches);
    assert(xl6ForeignViewCallBeginV1(owner,&call,&native,sizeof(native)));
    assert(xl6ForeignViewCloseV1(child) && parent.watches);
    checkExpired(child);
    assert(xl6ForeignViewCallEndV1(&call) && !parent.watches && parent.value==7);
    assert(xl6ForeignViewReleaseV1(child));

    child=getChild(owner);
    assert(child && parent.watches);
    xthread* worker=xrtThreadCreate(releaseOnWorker,child,0);
    assert(worker && xrtThreadWaitFor(worker,10000)==XWAIT_OK);
    xrtThreadDestroy(worker);
    assert(parent.watches && xl6ForeignViewDrainV1() && !parent.watches);

    child=getChild(owner);
    XL6_ForeignView* peerChild=getChild(peer);
    assert(child && peerChild && child!=peerChild && parent.watches->next);
    assert(xl6ForeignViewCloseV1(owner));
    assert(parent.owners==1 && parent.watches && !parent.watches->next);
    checkExpired(child);
    assert(xl6ForeignViewReleaseV1(owner) && xl6ForeignViewReleaseV1(child));
    replaceNode(&parent,13);
    checkExpired(peerChild);
    XL6_ForeignView* replacement=getChild(peer);
    assert(replacement && replacement!=peerChild && parent.value==13);
    assert(xl6ForeignViewReleaseV1(peerChild));
    assert(xl6ForeignViewReleaseV1(peer) && parent.owners==1);
    assert(xl6ForeignViewReleaseV1(replacement) && !parent.owners && !parent.watches);
    assert(consumed==previousConsumed+2);
}

static void checkAllocation(void)
{
    unsigned previousConsumed=consumed;
    unsigned failures=0;
    for (int allowance=0;allowance<12;++allowance) {
        Parent parent={.owners=1,.value=42};
        XL6_ForeignView* owner=xl6ForeignViewCreateRootV1(&parent,&ownerPolicy,NULL);
        assert(owner && xrtMemDebugFailAfter(allowance));
        XL6_ForeignView* child=getChild(owner);
        bool failed=xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        xrtClearError();
        if (!child) {
            ++failures;
            assert(failed && !parent.watches);
        }
        assert(xl6ForeignViewReleaseV1(child) && !parent.watches && parent.value==42);
        assert(xl6ForeignViewReleaseV1(owner) && !parent.owners);
    }
    assert(failures>=2 && failures<12);
    assert(consumed==previousConsumed+12);
}

typedef enum { ReplaceOnAcquire, PublishOnAcquire, ReplaceOnRelease, CloseOnRelease } Reentry;
typedef struct {
    Parent* parent;
    XL6_ForeignView* owner;
    XL6_ForeignView* nested;
    Reentry action;
    unsigned acquires;
    unsigned releases;
} LeaseHost;

static int acquireChildCode(void* raw)
{
    LeaseHost* host=raw;
    assert(++host->acquires==1 && host->parent->watches);
    if (host->action==ReplaceOnAcquire) replaceNode(host->parent,91);
    else {
        host->nested=getChild(host->owner);
        assert(host->nested && host->parent->watches->next);
    }
    return 1;
}

static void releaseChildCode(void* raw)
{
    LeaseHost* host=raw;
    assert(++host->releases==1);
    if (host->action==ReplaceOnRelease) {
        assert(host->parent->watches && !host->parent->watches->next);
        replaceNode(host->parent,92);
    }
    if (host->action==CloseOnRelease) {
        assert(host->parent->watches && !host->parent->watches->next);
        assert(xl6ForeignViewCloseV1(host->nested));
    }
}

static void checkReentry(Reentry action)
{
    Parent parent={.owners=1,.value=42};
    LeaseHost host={.parent=&parent,.action=action};
    host.owner=xl6ForeignViewCreateRootV1(&parent,&ownerPolicy,NULL);
    assert(host.owner);
    XL6_CodeOwner code={sizeof(code),&host,acquireChildCode,releaseChildCode};
    XL6_ForeignView* child=getChildWithLease(host.owner,&code);
    assert(host.acquires==1 && host.releases==1);
    if (action==PublishOnAcquire) {
        assert(child==host.nested && parent.watches && !parent.watches->next);
    } else {
        assert(!child && xrtGetError() && !parent.watches);
        xrtClearError();
        if (host.nested) checkExpired(host.nested);
        child=getChild(host.owner);
        assert(child && parent.watches);
    }
    assert(xl6ForeignViewReleaseV1(host.nested));
    assert(xl6ForeignViewReleaseV1(child) && !parent.watches);
    assert(xl6ForeignViewReleaseV1(host.owner) && !parent.owners);
}

static void checkLifetimes(void)
{
    checkOwnership();
    checkAllocation();
    checkReentry(ReplaceOnAcquire);
    checkReentry(PublishOnAcquire);
    checkReentry(ReplaceOnRelease);
    checkReentry(CloseOnRelease);
}

int main(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    checkLifetimes();
    xmemdebugsnapshot before,after;
    xrtMemDebugSnapshot(&before);
    checkLifetimes();
    xrtMemDebugSnapshot(&after);
    assert(attached==detached);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    puts("OBSERVED_CHILD_OK");
    return 0;
}
