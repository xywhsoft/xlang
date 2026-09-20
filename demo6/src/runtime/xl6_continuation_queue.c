#define XRT_MODULE_MEMORY
#define XRT_MODULE_MUTEX
#define XRT_MODULE_COND
#include "lib/xrt/xrt.h"
#include "xl6_continuation_queue.h"
#include "xl6_ownership_scope.h"
#include <stdlib.h>

enum { COMMITTED = XL6_CONTINUATION_COMMITTED, READY = XL6_CONTINUATION_READY,
    TRANSPORT_RELEASED = XL6_CONTINUATION_TRANSPORT_RELEASED, RUNNING = XL6_CONTINUATION_RUNNING };
struct XL6_ContinuationQueue {
    xmutex* lock;
    xcond* ready;
    XL6_ContinuationTicket *head, *tail;
    XL6_ContinuationTicket* owned;
    size_t capacity, active, live;
    size_t references, ownedCount, readers, parked;
    const void* claim;
    bool closed, shutdownPrepared, cleared;
};
static void lock_queue(XL6_ContinuationQueue* queue)
{ if (!xrtMutexLock(queue->lock)) abort(); }
static void unlock_queue(XL6_ContinuationQueue* queue)
{ if (!xrtMutexUnlock(queue->lock)) abort(); }
static void own_ticket(XL6_ContinuationQueue* queue,XL6_ContinuationTicket* ticket)
{
    ticket->ownedPrevious=NULL;ticket->ownedNext=queue->owned;
    if(queue->owned)queue->owned->ownedPrevious=ticket;
    queue->owned=ticket;++queue->ownedCount;
}
static void disown_ticket(XL6_ContinuationQueue* queue,XL6_ContinuationTicket* ticket)
{
    if(ticket->ownedPrevious)ticket->ownedPrevious->ownedNext=ticket->ownedNext;
    else {if(queue->owned!=ticket)abort();queue->owned=ticket->ownedNext;}
    if(ticket->ownedNext)ticket->ownedNext->ownedPrevious=ticket->ownedPrevious;
    ticket->ownedNext=ticket->ownedPrevious=NULL;
    if(!queue->ownedCount)abort();
    --queue->ownedCount;
}
static void queue_drop_body(XL6_ContinuationQueue* queue)
{
    lock_queue(queue);if(!queue->references)abort();
    if(--queue->references){unlock_queue(queue);return;}
    if(!queue->closed||queue->active||queue->live||queue->owned||queue->ownedCount||queue->readers||queue->parked)abort();
    unlock_queue(queue);
    if(!xrtCondDestroy(queue->ready)||!xrtMutexDestroy(queue->lock))abort();
    xrtFree(queue);
}

static bool continuation_ownership_count(const void* data, size_t* count)
{
    const XL6_ContinuationTicket* ticket = (const XL6_ContinuationTicket*)data;
    size_t expected;
    if (ticket == NULL || count == NULL || ticket->queue == NULL ||
        ticket->ownershipTrace == NULL ||
        ticket->predecessor != NULL || ticket->successor != NULL ||
        (ticket->flags != COMMITTED && ticket->flags != (COMMITTED | READY | TRANSPORT_RELEASED)))
        return false;
    if (ticket->codeLease != NULL) {
        xrtownershipref lease = xl6CodeLeaseOwnership(ticket->codeLease);
        size_t references;
        if (!lease.Ops->Count(lease.Data, &references)) return false;
    }
    expected = 1 + (ticket->transportOwned && !(ticket->flags & TRANSPORT_RELEASED) ? 1u : 0u);
    if (ticket->ownershipRefs != expected) return false;
    *count = ticket->ownershipRefs;
    return true;
}
static bool continuation_ownership_trace(const void* data, xrtownershipvisitor visit, void* context)
{
    const XL6_ContinuationTicket* ticket = (const XL6_ContinuationTicket*)data;
    size_t count;
    if (visit == NULL || !continuation_ownership_count(data, &count)) return false;
    if (ticket->codeLease != NULL && !visit(xl6CodeLeaseOwnership(ticket->codeLease), context)) return false;
    return ticket->ownershipTrace(ticket->data, visit, context);
}
static const xrtownershipops continuation_ownership_ops = {
    continuation_ownership_count, continuation_ownership_trace
};
xrtownershipref xl6ContinuationOwnership(const XL6_ContinuationTicket* ticket)
{
    if (ticket != NULL && ticket->storage != NULL) return ticket->storage->Ownership(ticket->data);
    return (xrtownershipref){ticket, ticket != NULL ? &continuation_ownership_ops : NULL};
}

static bool xl6OwnershipBody_xl6ContinuationTraceBind(XL6_ContinuationTicket* ticket, xrtownershiptrace trace)
{
    if (ticket == NULL || ticket->queue == NULL || trace == NULL || ticket->flags != 0 ||
        ticket->transportOwned || ticket->ownershipRefs != 1 || ticket->ownershipTrace != NULL || ticket->storage != NULL) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    ticket->ownershipTrace = trace;
    return true;
}
bool xl6ContinuationTraceBind(XL6_ContinuationTicket* ticket, xrtownershiptrace trace)
{
    XL6_OWNERSHIP_RETURN(bool, xl6OwnershipBody_xl6ContinuationTraceBind(ticket, trace));
}

static XL6_ContinuationQueue* xl6OwnershipBody_xl6ContinuationQueueCreate(size_t capacity)
{
    XL6_ContinuationQueue* queue;
    if (capacity == 0) return NULL;
    queue = (XL6_ContinuationQueue*)xrtCalloc(1, sizeof(*queue));
    if (queue == NULL) return NULL;
    queue->lock = xrtMutexCreate();
    if (queue->lock == NULL) { xrtFree(queue); return NULL; }
    queue->ready = xrtCondCreate();
    if (queue->ready == NULL) { xrtMutexDestroy(queue->lock); xrtFree(queue); return NULL; }
    queue->capacity = capacity;queue->references=1;
    return queue;
}
XL6_ContinuationQueue* xl6ContinuationQueueCreate(size_t capacity)
{
    XL6_OWNERSHIP_RETURN(XL6_ContinuationQueue*, xl6OwnershipBody_xl6ContinuationQueueCreate(capacity));
}
static XL6_ContinuationAdmission xl6OwnershipBody_xl6ContinuationReserve(
    XL6_ContinuationQueue* queue, XL6_ContinuationTicket* ticket,
    XL6_ContinuationProc run, XL6_ContinuationProc release, void* data)
{
    XL6_ContinuationAdmission result = XL6_CONTINUATION_ADMITTED;
    if (queue == NULL || ticket == NULL || run == NULL || release == NULL || ticket->queue != NULL)
        return XL6_CONTINUATION_INVALID;
    lock_queue(queue);
    if (queue->closed) result = XL6_CONTINUATION_CLOSED;
    else if (queue->active == queue->capacity || queue->live == SIZE_MAX) result = XL6_CONTINUATION_FULL;
    else {
        ticket->queue = queue; ticket->next = NULL;
        ticket->run = run; ticket->release = release; ticket->data = data;
        ticket->predecessor = ticket->successor = NULL;
        ticket->codeLease = NULL;
        ticket->ownershipRefs = 1; ticket->ownershipTrace = NULL; ticket->storage = NULL;
        ticket->flags = 0; ticket->transportOwned = false; ++queue->active; ++queue->live;
        own_ticket(queue,ticket);
    }
    unlock_queue(queue);
    return result;
}
XL6_ContinuationAdmission xl6ContinuationReserve(
    XL6_ContinuationQueue* queue, XL6_ContinuationTicket* ticket,
    XL6_ContinuationProc run, XL6_ContinuationProc release, void* data)
{
    XL6_OWNERSHIP_RETURN(XL6_ContinuationAdmission, xl6OwnershipBody_xl6ContinuationReserve(queue, ticket, run, release, data));
}
static XL6_ContinuationAdmission xl6OwnershipBody_xl6ContinuationReserveOwned(
    XL6_ContinuationQueue* queue, XL6_ContinuationTicket* ticket,
    XL6_ContinuationProc run, XL6_ContinuationProc release, void* data, const XL6_CodeOwner* owner)
{
    XL6_ContinuationAdmission result;
    XL6_CodeLease* lease = NULL;
    XL6_CodeLeaseAdmission admission;
    if (owner == NULL) return xl6ContinuationReserve(queue, ticket, run, release, data);
    if (queue == NULL || ticket == NULL || ticket->queue != NULL || run == NULL || release == NULL ||
        owner->size < sizeof(*owner) || owner->acquire == NULL || owner->release == NULL ||
        (xl6CodeOwnerGraphOps(owner) != NULL && owner->context == NULL))
        return XL6_CONTINUATION_INVALID;
    /* A root activation is a new execution admission, even when this caller
     * has an accepted scope. Only ReserveSuccessor inherits after Close. */
    admission = xl6CodeLeaseAcquire(owner, &lease);
    if (admission != XL6_CODE_LEASE_ACQUIRED) return admission == XL6_CODE_LEASE_CLOSED
        ? XL6_CONTINUATION_CLOSED : admission == XL6_CODE_LEASE_RESOURCE_FAILURE
        ? XL6_CONTINUATION_RESOURCE_FAILURE : XL6_CONTINUATION_INVALID;
    result = xl6ContinuationReserve(queue, ticket, run, release, data);
    if (result == XL6_CONTINUATION_ADMITTED) {
        ticket->codeLease = lease;
    } else xl6CodeLeaseRelease(lease);
    return result;
}
XL6_ContinuationAdmission xl6ContinuationReserveOwned(
    XL6_ContinuationQueue* queue, XL6_ContinuationTicket* ticket,
    XL6_ContinuationProc run, XL6_ContinuationProc release, void* data, const XL6_CodeOwner* owner)
{
    XL6_OWNERSHIP_RETURN(XL6_ContinuationAdmission, xl6OwnershipBody_xl6ContinuationReserveOwned(queue, ticket, run, release, data, owner));
}
static XL6_ContinuationAdmission xl6OwnershipBody_xl6ContinuationReserveSuccessor(
    XL6_ContinuationTicket* predecessor, XL6_ContinuationTicket* successor,
    XL6_ContinuationProc run, XL6_ContinuationProc release, void* data)
{
    XL6_ContinuationQueue* queue;
    XL6_ContinuationAdmission result = XL6_CONTINUATION_INVALID;
    if (predecessor == NULL || successor == NULL || predecessor == successor ||
        predecessor->queue == NULL || successor->queue != NULL || run == NULL || release == NULL)
        return result;
    queue = predecessor->queue;
    lock_queue(queue);
    if ((predecessor->flags & RUNNING) != 0 && predecessor->successor == NULL && queue->live != SIZE_MAX) {
        successor->queue = queue; successor->next = NULL;
        successor->run = run; successor->release = release; successor->data = data;
        successor->predecessor = predecessor; successor->successor = NULL; successor->flags = 0;
        successor->transportOwned = false;
        successor->ownershipRefs = 1; successor->ownershipTrace = NULL; successor->storage = NULL;
        successor->codeLease = predecessor->codeLease;
        predecessor->successor = successor; ++queue->live;
        own_ticket(queue,successor);
        result = XL6_CONTINUATION_ADMITTED;
    }
    unlock_queue(queue);
    return result;
}
XL6_ContinuationAdmission xl6ContinuationReserveSuccessor(
    XL6_ContinuationTicket* predecessor, XL6_ContinuationTicket* successor,
    XL6_ContinuationProc run, XL6_ContinuationProc release, void* data)
{
    XL6_OWNERSHIP_RETURN(XL6_ContinuationAdmission, xl6OwnershipBody_xl6ContinuationReserveSuccessor(predecessor, successor, run, release, data));
}
/* Called under queue lock, exactly when the final prerequisite is satisfied.
 * A successor additionally waits for physical predecessor retirement. */
static void publish_ticket(XL6_ContinuationQueue* queue, XL6_ContinuationTicket* ticket)
{
    if (ticket->flags != (COMMITTED | READY | TRANSPORT_RELEASED) || ticket->predecessor != NULL) return;
    if (queue->tail != NULL) queue->tail->next = ticket;
    else queue->head = ticket;
    queue->tail = ticket;
    if (!xrtCondSignal(queue->ready)) abort();
}
static void xl6OwnershipBody_signal_ticket(XL6_ContinuationTicket* ticket, unsigned flag)
{
    XL6_ContinuationQueue* queue = ticket->queue;
    if (queue == NULL) abort();
    lock_queue(queue);
    if ((ticket->flags & (flag | RUNNING)) != 0) abort();
    if (flag == COMMITTED && ticket->storage != NULL && ticket->storage->Commit != NULL)
        ticket->storage->Commit(ticket->data);
    if (flag == TRANSPORT_RELEASED && ticket->transportOwned) {
        if (ticket->ownershipRefs != 2) abort();
        --ticket->ownershipRefs;
        /* The registration/queue reference is still real and cannot retire
         * before all events. Thus this Drop cannot run cleanup or free data. */
        if (ticket->storage != NULL) ticket->storage->Release(ticket->data);
    }
    ticket->flags |= flag;
    publish_ticket(queue, ticket);
    unlock_queue(queue);
}
static void signal_ticket(XL6_ContinuationTicket* ticket, unsigned flag)
{
    XL6_OWNERSHIP_RETURN_VOID(xl6OwnershipBody_signal_ticket(ticket, flag));
}
void xl6ContinuationCommit(XL6_ContinuationTicket* ticket) { signal_ticket(ticket, COMMITTED); }
void xl6ContinuationReady(XL6_ContinuationTicket* ticket) { signal_ticket(ticket, READY); }
void xl6ContinuationTransportReleased(XL6_ContinuationTicket* ticket) { signal_ticket(ticket, TRANSPORT_RELEASED); }
static bool xl6OwnershipBody_xl6ContinuationAbort(XL6_ContinuationTicket* ticket)
{
    XL6_ContinuationQueue* queue;
    bool aborted = false;
    XL6_CodeLease* lease = NULL;
    if (ticket == NULL || ticket->queue == NULL) return false;
    queue = ticket->queue;
    lock_queue(queue);
    if (ticket->flags == 0 && !ticket->transportOwned) {
        if (ticket->ownershipRefs != 1) abort();
        if (ticket->storage != NULL && ticket->storage->Abort != NULL) ticket->storage->Abort(ticket->data);
        ticket->ownershipRefs = 0; ticket->ownershipTrace = NULL;
        disown_ticket(queue,ticket);
        if (ticket->predecessor != NULL) {
            ticket->predecessor->successor = NULL; ticket->predecessor = NULL;
        } else {
            lease = ticket->codeLease;
            if (lease == NULL) --queue->active;
        }
        ticket->codeLease = NULL;
        ticket->queue = NULL;
        if (lease == NULL) --queue->live;
        aborted = true;
        if (queue->closed && queue->live == 0 && !xrtCondBroadcast(queue->ready)) abort();
    }
    unlock_queue(queue);
    if (lease != NULL) {
        /* The ticket is detached, but the resident call still occupies an
         * execution credit. A host drop hook can block or reenter retirement. */
        xl6CodeLeaseRelease(lease);
        lock_queue(queue); --queue->active; --queue->live;
        if (queue->closed && queue->live == 0 && !xrtCondBroadcast(queue->ready)) abort();
        unlock_queue(queue);
    }
    return aborted;
}
bool xl6ContinuationAbort(XL6_ContinuationTicket* ticket)
{
    XL6_OWNERSHIP_RETURN(bool, xl6OwnershipBody_xl6ContinuationAbort(ticket));
}
/* Waiting holds neither ownership admission nor a selected ticket. Recheck
 * after reacquiring admission BEFORE the queue lock: another consumer can
 * legitimately take the notification first. No queue-to-domain lock upgrade. */
static XL6_ContinuationTicket* take_ticket(XL6_ContinuationQueue* queue, bool wait, XL6_ContinuationWaitSlot* slot)
{
    for (;;) {
        XL6_ContinuationTicket* ticket;
        bool finished;
        xrtownershipscope mutation = {0};
        if (!xrtOwnershipMutationBegin(&mutation)) abort();
        lock_queue(queue);
        if(slot&&slot->parked){
            if(!queue->parked||queue->readers==SIZE_MAX)abort();
            --queue->parked;++queue->readers;slot->parked=false;
        }
        ticket = queue->head;
        finished = queue->closed && queue->live == 0;
        if (ticket != NULL) {
            queue->head = ticket->next;
            if (queue->head == NULL) queue->tail = NULL;
            ticket->flags |= RUNNING;
            disown_ticket(queue,ticket); /* base reference now belongs to run_one */
        }
        if(slot&&!ticket&&wait&&!finished){
            if(!queue->readers||queue->parked==SIZE_MAX)abort();
            --queue->readers;++queue->parked;slot->parked=true;
        }
        unlock_queue(queue);
        if (!xrtOwnershipScopeEnd(&mutation)) abort();
        if (ticket != NULL || !wait || finished) return ticket;
        lock_queue(queue);
        while (queue->head == NULL && !(queue->closed && queue->live == 0))
            if (xrtCondWait(queue->ready, queue->lock) != XWAIT_OK) abort();
        unlock_queue(queue);
    }
}
static bool run_one(XL6_ContinuationQueue* queue, bool wait, XL6_ContinuationWaitSlot* slot)
{
    XL6_ContinuationTicket *ticket, *successor;
    XL6_ContinuationProc run, release;
    void* data; XL6_CodeLease* lease; XL6_CodeScope scope = {0};
    const XL6_ContinuationStorageV2* storage;
    xrtownershipscope retirement = {0};
    if (queue == NULL) return false;
    ticket = take_ticket(queue, wait, slot);
    if (ticket == NULL) return false;
    run = ticket->run; release = ticket->release; data = ticket->data; storage = ticket->storage;
    lease = ticket->codeLease;
    xl6CodeScopeEnter(&scope, lease);
    run(data);
    /* Detach the physical frame before cleanup. The resident local now owns
     * its lease; the scope is an additional real activity reference. Both the
     * retiring ticket and an unpublished successor reject inspection. */
    if (!xrtOwnershipMutationBegin(&retirement)) abort();
    /* No producer may access the ticket after the three-event handoff. */
    successor = ticket->successor;
    if (ticket->ownershipRefs != 1) abort();
    ticket->ownershipRefs = 0;
    ticket->queue = NULL;
    if (storage != NULL) ticket->codeLease = NULL; /* Now owned by the resident runner. */
    if (!xrtOwnershipScopeEnd(&retirement)) abort();
    /* User cleanup can wait or reenter. Do not hold the mutation admission or
     * queue lock across it. Participating frame adapters detach slots through
     * their storage contract and free physical storage in a short admission.
     * The queue alone cannot certify arbitrary native release implementations. */
    release(data);
    if (storage != NULL) storage->Release(data);
    /* The ticket may now be freed. Keep the queue execution credit through
     * the resident scope's last release and any reentrant owner drop. A
     * successor cannot run until this scope has ended; its inherited owning
     * lease is already committed, so Leave cannot release its host pin. */
    if (successor == NULL) xl6CodeLeaseRelease(lease);
    xl6CodeScopeLeave(&scope);
    if (!xrtOwnershipMutationBegin(&retirement)) abort();
    lock_queue(queue);
    if (successor != NULL) {
        successor->predecessor = NULL;
        publish_ticket(queue, successor);
    } else --queue->active;
    --queue->live;
    if (queue->closed && queue->live == 0 && !xrtCondBroadcast(queue->ready)) abort();
    unlock_queue(queue);
    if (!xrtOwnershipScopeEnd(&retirement)) abort();
    return true;
}
static bool enter_run(XL6_ContinuationQueue* queue,bool wait,XL6_ContinuationWaitSlot* slot)
{
    if(!queue)return false;
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    lock_queue(queue);
    if(!queue->references||queue->cleared||queue->references==SIZE_MAX||queue->readers==SIZE_MAX)abort();
    if(slot){if(slot->queue||slot->parked)abort();slot->queue=queue;}
    ++queue->references;++queue->readers;unlock_queue(queue);
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    bool result=run_one(queue,wait,slot);
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    lock_queue(queue);if(!queue->readers)abort();--queue->readers;
    if(slot){if(slot->queue!=queue||slot->parked)abort();slot->queue=NULL;}
    unlock_queue(queue);
    queue_drop_body(queue);if(!xrtOwnershipScopeEnd(&mutation))abort();return result;
}
bool xl6ContinuationRunOne(XL6_ContinuationQueue* queue) { return enter_run(queue, false, NULL); }
bool xl6ContinuationWaitRunOne(XL6_ContinuationQueue* queue) { return enter_run(queue, true, NULL); }
bool xl6ContinuationWaitRunOwned(XL6_ContinuationQueue* queue,XL6_ContinuationWaitSlot* slot)
{if(!slot)abort();return enter_run(queue,true,slot);}
bool xl6ContinuationQueueIsIdle(XL6_ContinuationQueue* queue)
{
    bool idle;
    if (queue == NULL) return true;
    lock_queue(queue); idle = queue->live == 0; unlock_queue(queue);
    return idle;
}
size_t xl6ContinuationQueueActivationCount(XL6_ContinuationQueue* queue)
{
    size_t active;
    if (queue == NULL) return 0;
    lock_queue(queue); active = queue->active; unlock_queue(queue);
    return active;
}
static void xl6OwnershipBody_xl6ContinuationQueueClose(XL6_ContinuationQueue* queue)
{
    if (queue == NULL) return;
    lock_queue(queue); queue->closed = true;
    if (!xrtCondBroadcast(queue->ready)) abort();
    unlock_queue(queue);
}
void xl6ContinuationQueueClose(XL6_ContinuationQueue* queue)
{
    XL6_OWNERSHIP_RETURN_VOID(xl6OwnershipBody_xl6ContinuationQueueClose(queue));
}
static bool xl6OwnershipBody_xl6ContinuationQueueDestroy(XL6_ContinuationQueue* queue)
{
    bool ready;
    if (queue == NULL) return true;
    lock_queue(queue);
    ready = queue->closed && queue->live == 0;
    if (ready && queue->active != 0) abort();
    unlock_queue(queue);
    if (!ready) return false;
    queue_drop_body(queue);
    return true;
}
bool xl6ContinuationQueueDestroy(XL6_ContinuationQueue* queue)
{
    XL6_OWNERSHIP_RETURN(bool, xl6OwnershipBody_xl6ContinuationQueueDestroy(queue));
}

static bool queue_count(const void* data,size_t* count)
{
    const XL6_ContinuationQueue* queue=data;
    if(!queue||!count||!queue->references||queue->cleared||queue->readers||
        queue->ownedCount!=queue->live||queue->active!=queue->live)return false;
    size_t owned=0;const XL6_ContinuationTicket* previous=NULL;
    for(const XL6_ContinuationTicket* ticket=queue->owned;ticket;ticket=ticket->ownedNext){
        if(owned++==queue->ownedCount||ticket->queue!=queue||ticket->ownedPrevious!=previous||
            ticket->predecessor||ticket->successor||!ticket->storage||
            (ticket->flags!=COMMITTED&&ticket->flags!=(COMMITTED|READY|TRANSPORT_RELEASED)))return false;
        previous=ticket;
    }
    if(owned!=queue->ownedCount)return false;
    *count=queue->references;return true;
}
static bool queue_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const XL6_ContinuationQueue* queue=data;size_t count;
    if(!visit||!queue_count(data,&count))return false;
    for(const XL6_ContinuationTicket* ticket=queue->owned;ticket;ticket=ticket->ownedNext)
        if(!visit(ticket->storage->Ownership(ticket->data),context))return false;
    return true;
}
static const xrtownershipops queue_ops={queue_count,queue_trace};
xrtownershipref xl6ContinuationQueueOwnership(const XL6_ContinuationQueue* queue)
{return (xrtownershipref){queue,queue?&queue_ops:NULL};}
static bool queue_hold(const void* data)
{
    XL6_ContinuationQueue* queue=(XL6_ContinuationQueue*)data;xrtownershipscope mutation={0};bool held=false;
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    lock_queue(queue);
    if(queue->references&&queue->references<SIZE_MAX&&!queue->cleared){++queue->references;held=true;}
    unlock_queue(queue);if(!xrtOwnershipScopeEnd(&mutation))abort();return held;
}
static void queue_drop(const void* data)
{XL6_OWNERSHIP_RETURN_VOID(queue_drop_body((XL6_ContinuationQueue*)data));}
static bool queue_claim(const void* data,const void* token)
{
    XL6_ContinuationQueue* queue=(XL6_ContinuationQueue*)data;
    if(!token||queue->cleared||(queue->claim&&queue->claim!=token))return false;
    queue->claim=token;return true;
}
static void queue_restore(const void* data,const void* token)
{
    XL6_ContinuationQueue* queue=(XL6_ContinuationQueue*)data;
    if(!token||queue->claim!=token||queue->cleared)abort();
    queue->claim=NULL;
}
static bool queue_prepared(const void* data) {return ((const XL6_ContinuationQueue*)data)->shutdownPrepared;}
static xrtownershipprepareresult queue_prepare(const void* data,const void* token)
{
    XL6_ContinuationQueue* queue=(XL6_ContinuationQueue*)data;xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    lock_queue(queue);
    if(!token||queue->claim!=token||queue->cleared)abort();
    queue->closed=queue->shutdownPrepared=true;
    if(!xrtCondBroadcast(queue->ready))abort();
    unlock_queue(queue);if(!xrtOwnershipScopeEnd(&mutation))abort();
    return XRT_OWNERSHIP_PREPARE_READY;
}
static void queue_clear(const void* data,const void* token)
{
    XL6_ContinuationQueue* queue=(XL6_ContinuationQueue*)data;
    if(!token||queue->claim!=token||queue->cleared||!queue->shutdownPrepared||queue->live||queue->active||queue->ownedCount||queue->readers||queue->parked)abort();
    queue->cleared=true;
}
const xrtownershipadapterv1* xl6ContinuationQueueOwnershipAdapterV1(xrtownershipref ref,
    const XL6_ContinuationStorageV2* const* storages,size_t count,const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),queue_hold,queue_drop,queue_claim,queue_restore,NULL,queue_clear,NULL};
    static const xrtownershippreparationv1 policy={sizeof(policy),&adapter,queue_prepared,queue_prepare};
    const XL6_ContinuationQueue* queue=ref.Data;size_t references;
    if(!preparation||ref.Ops!=&queue_ops||!queue||(!storages&&count)||!queue_count(queue,&references))return NULL;
    for(const XL6_ContinuationTicket* ticket=queue->owned;ticket;ticket=ticket->ownedNext){
        size_t i=0;for(;i<count;++i)if(ticket->storage==storages[i])break;
        if(i==count||!ticket->storage->Ownership)return NULL;
    }
    *preparation=&policy;return &adapter;
}
bool xl6ContinuationQueuePrepareOwns(const XL6_ContinuationTicket* ticket,const void* token)
{
    if(!ticket||!token||!ticket->queue)return false;
    const XL6_ContinuationQueue* queue=ticket->queue;
    if(!xl6ContinuationQueuePreparedFor(queue,token))return false;
    for(const XL6_ContinuationTicket* owned=queue->owned;owned;owned=owned->ownedNext)
        if(owned==ticket)return true;
    return false;
}
bool xl6ContinuationQueuePreparedFor(const XL6_ContinuationQueue* queue,const void* token)
{return queue&&token&&queue->claim==token&&queue->shutdownPrepared&&!queue->cleared;}
