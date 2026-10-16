#define XRT_MODULE_MEMORY
#define XRT_MODULE_MUTEX
#define XRT_MODULE_THREAD
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include "xl6_continuation_executor.h"
#include "xl6_ownership_scope.h"
#include "xl6_ownership_catalog.h"
#include <stdlib.h>

typedef struct XL6_ContinuationWorker {
    struct XL6_ContinuationWorker* next;
    XL6_ContinuationExecutor* executor;
    xthread* thread;
    uint64_t id;
    size_t blockingDepth;
    XL6_ContinuationWaitSlot wait;
    bool exited;
} XL6_ContinuationWorker;
struct XL6_ContinuationExecutor {
    xmutex* gate;
    XL6_ContinuationQueue* queue;
    XL6_ContinuationWorker* workers;
    size_t workerCount, blockedCount, maxWorkers;
    size_t references, entries;
    const void* claim;
    bool closed, guaranteed, initialized, entrySealed, joining, joined, destroying, retired, cleared;
};
static void executor_lock(XL6_ContinuationExecutor* executor)
{ if (!xrtMutexLock(executor->gate)) abort(); }
static void executor_unlock(XL6_ContinuationExecutor* executor)
{ if (!xrtMutexUnlock(executor->gate)) abort(); }
static void executor_begin(XL6_ContinuationExecutor* executor,xrtownershipscope* mutation)
{if(!xrtOwnershipMutationBegin(mutation))abort();executor_lock(executor);}
static void executor_end(XL6_ContinuationExecutor* executor,xrtownershipscope* mutation)
{executor_unlock(executor);if(!xrtOwnershipScopeEnd(mutation))abort();}
static bool executor_enter_mode(XL6_ContinuationExecutor* executor,bool terminalOwner)
{
    xrtownershipscope mutation={0};executor_begin(executor,&mutation);
    if(!executor->references||executor->references==SIZE_MAX||executor->entries==SIZE_MAX)abort();
    /* Ownership-plan Clear quarantines the graph but deliberately does not
     * consume its real creator slot. The owning parent's terminal Destroy is
     * therefore the one API entry which must remain admissible after Clear.
     * entrySealed still rejects duplicate/late owners once retirement wins. */
    bool entered=!executor->entrySealed&&!executor->joining&&!executor->destroying&&
        (!executor->cleared||terminalOwner);
    if(entered){++executor->references;++executor->entries;}
    executor_end(executor,&mutation);return entered;
}
static bool executor_enter(XL6_ContinuationExecutor* executor)
{return executor_enter_mode(executor,false);}
static bool executor_enter_terminal_owner(XL6_ContinuationExecutor* executor)
{return executor_enter_mode(executor,true);}
static void executor_drop_locked(XL6_ContinuationExecutor* executor,xrtownershipscope* mutation)
{
    if(!executor->references)abort();
    bool last=--executor->references==0;
    if(last&&(!executor->retired||executor->entries||executor->queue||executor->workers))abort();
    executor_unlock(executor);
    if(last){if(!xrtMutexDestroy(executor->gate))abort();xrtFree(executor);}
    if(!xrtOwnershipScopeEnd(mutation))abort();
}
static void executor_drop(const void* data)
{
    XL6_ContinuationExecutor* executor=(XL6_ContinuationExecutor*)data;xrtownershipscope mutation={0};
    executor_begin(executor,&mutation);executor_drop_locked(executor,&mutation);
}
static void executor_leave(XL6_ContinuationExecutor* executor)
{
    xrtownershipscope mutation={0};executor_begin(executor,&mutation);
    if(!executor->entries)abort();
    --executor->entries;executor_drop_locked(executor,&mutation);
}
static int32_t executor_worker(void* data)
{
    XL6_ContinuationWorker* worker = (XL6_ContinuationWorker*)data;
    XL6_ContinuationExecutor* executor = worker->executor;
    xrtownershipscope mutation={0};executor_begin(executor,&mutation);
    worker->id = xrtThreadCurrentId();
    executor_end(executor,&mutation);
    /* The executor uniquely owns this worker/control allocation until join.
     * The thread borrows it; its queue reference is held in the real wait
     * slot. No detached thread root or guessed idle-reference subtraction. */
    while (xl6ContinuationWaitRunOwned(executor->queue,&worker->wait)) {
        executor_begin(executor,&mutation);
        if (worker->blockingDepth != 0) abort();
        executor_end(executor,&mutation);
        xrtClearError();
    }
    executor_begin(executor,&mutation);worker->id=0;worker->exited=true;executor_end(executor,&mutation);
    return 0;
}
/* Called with mutation/gate held: a worker cannot observe partial setup. */
static bool executor_add_worker(XL6_ContinuationExecutor* executor)
{
    XL6_ContinuationWorker* worker = (XL6_ContinuationWorker*)xrtCalloc(1, sizeof(*worker));
    if (worker == NULL) return false;
    worker->executor = executor;
    worker->thread = xrtThreadCreate(executor_worker, worker, 0);
    if (worker->thread == NULL) { xrtFree(worker); return false; }
    worker->next = executor->workers; executor->workers = worker;
    ++executor->workerCount;
    return true;
}
XL6_ContinuationExecutor* xl6ContinuationExecutorCreate(size_t capacity, size_t initialWorkers, size_t maxWorkers)
{
    XL6_ContinuationExecutor* executor;
    size_t i;
    if (capacity == 0 || initialWorkers == 0 || initialWorkers > maxWorkers) return NULL;
    executor = (XL6_ContinuationExecutor*)xrtCalloc(1, sizeof(*executor));
    if (executor == NULL) return NULL;
    executor->references=1;
    executor->queue = xl6ContinuationQueueCreate(capacity);
    executor->gate = xrtMutexCreate();
    if (executor->queue == NULL || executor->gate == NULL) {
        xerror* error = xrtTakeError();
        xl6ContinuationQueueClose(executor->queue);
        xl6ContinuationQueueDestroy(executor->queue);
        if (executor->gate != NULL) xrtMutexDestroy(executor->gate);
        xrtFree(executor); xrtSetErrorTake(error); return NULL;
    }
    executor->maxWorkers = maxWorkers;
    xrtownershipscope mutation={0};executor_begin(executor,&mutation);
    for (i = 0; i < initialWorkers; ++i) if (!executor_add_worker(executor)) break;
    executor->initialized=i==initialWorkers;executor_end(executor,&mutation);
    if (i != initialWorkers) {
        xerror* error = xrtTakeError();
        xl6ContinuationExecutorClose(executor);
        if (!xl6ContinuationExecutorDestroy(executor)) abort();
        xrtSetErrorTake(error); return NULL;
    }
    return executor;
}
bool xl6ContinuationExecutorIsIdle(XL6_ContinuationExecutor* executor)
{
    if(!executor)return true;
    if(!executor_enter(executor))return true;
    bool idle=xl6ContinuationQueueIsIdle(executor->queue);executor_leave(executor);return idle;
}
XL6_ContinuationExecutor* xl6ContinuationExecutorCreateGuaranteed(size_t capacity, size_t initialWorkers)
{
    XL6_ContinuationExecutor* executor = xl6ContinuationExecutorCreate(capacity, initialWorkers, capacity);
    if(executor){xrtownershipscope mutation={0};executor_begin(executor,&mutation);executor->guaranteed=true;executor_end(executor,&mutation);}
    return executor;
}
XL6_ContinuationAdmission xl6ContinuationExecutorReserveOwned(
    XL6_ContinuationExecutor* executor, XL6_ContinuationTicket* ticket,
    XL6_ContinuationProc run, XL6_ContinuationProc release, void* data, const XL6_CodeOwner* owner)
{
    XL6_ContinuationAdmission admission;
    bool provisioned = true;
    xerror* prior;
    if (executor == NULL) return XL6_CONTINUATION_INVALID;
    if (!executor_enter(executor)) return XL6_CONTINUATION_CLOSED;
    prior = xrtTakeError();
    /* Host acquire/drop must never run under the executor gate: module
     * retirement can reenter Close/Destroy with the API lock held. */
    admission = xl6ContinuationReserveOwned(executor->queue, ticket, run, release, data, owner);
    if (admission != XL6_CONTINUATION_ADMITTED || !executor->guaranteed) goto done;
    xrtownershipscope mutation={0};executor_begin(executor,&mutation);
    {
        size_t needed = xl6ContinuationQueueActivationCount(executor->queue);
        if (needed > executor->maxWorkers) abort(); /* capacity == maxWorkers */
        while (executor->workerCount < needed)
            if (!executor_add_worker(executor)) { provisioned = false; break; }
    }
    executor_end(executor,&mutation);
    if (!provisioned) {
        xerror* primary = xrtTakeError();
        if (!xl6ContinuationAbort(ticket)) abort(); /* private, no Watch/events */
        xrtClearError(); xrtSetErrorTake(primary);
        admission = XL6_CONTINUATION_RESOURCE_FAILURE;
    }
done:
    if (prior != NULL) { xrtClearError(); xrtSetErrorTake(prior); }
    executor_leave(executor);
    return admission;
}
XL6_ContinuationAdmission xl6ContinuationExecutorReserve(
    XL6_ContinuationExecutor* executor, XL6_ContinuationTicket* ticket,
    XL6_ContinuationProc run, XL6_ContinuationProc release, void* data)
{
    return xl6ContinuationExecutorReserveOwned(executor, ticket, run, release, data, NULL);
}
static XL6_ContinuationWorker* executor_current_worker(XL6_ContinuationExecutor* executor)
{
    XL6_ContinuationWorker* worker;
    uint64_t id = xrtThreadCurrentId();
    for (worker = executor->workers; worker != NULL; worker = worker->next)
        if (worker->id == id) return worker;
    return NULL;
}
XL6_ContinuationBlockingAdmission xl6ContinuationBlockingEnter(XL6_ContinuationExecutor* executor)
{
    XL6_ContinuationWorker* worker;
    XL6_ContinuationBlockingAdmission result = XL6_CONTINUATION_BLOCKING_ADMITTED;
    if (executor == NULL || !executor_enter(executor)) return XL6_CONTINUATION_BLOCKING_RESOURCE_FAILURE;
    xrtownershipscope mutation={0};executor_begin(executor,&mutation);
    worker = executor_current_worker(executor);
    if (worker != NULL) {
        if (!executor->guaranteed && worker->blockingDepth == 0 && executor->workerCount - executor->blockedCount == 1) {
            if (executor->workerCount == executor->maxWorkers) result = XL6_CONTINUATION_BLOCKING_LIMIT;
            else if (!executor_add_worker(executor)) result = XL6_CONTINUATION_BLOCKING_RESOURCE_FAILURE;
        }
        if (result == XL6_CONTINUATION_BLOCKING_ADMITTED) {
            if (worker->blockingDepth++ == 0) ++executor->blockedCount;
        }
    }
    executor_end(executor,&mutation);executor_leave(executor);
    return result;
}
void xl6ContinuationBlockingLeave(XL6_ContinuationExecutor* executor)
{
    XL6_ContinuationWorker* worker;
    if (executor == NULL || !executor_enter(executor)) abort();
    xrtownershipscope mutation={0};executor_begin(executor,&mutation);
    worker = executor_current_worker(executor);
    if (worker != NULL) {
        if (worker->blockingDepth == 0) abort();
        if (--worker->blockingDepth == 0) --executor->blockedCount;
    }
    executor_end(executor,&mutation);executor_leave(executor);
}
void xl6ContinuationExecutorClose(XL6_ContinuationExecutor* executor)
{
    if (executor == NULL) return;
    if(!executor_enter(executor))return;
    xrtownershipscope mutation={0};executor_begin(executor,&mutation);
    executor->closed=true;executor_end(executor,&mutation);
    xl6ContinuationQueueClose(executor->queue);
    executor_leave(executor);
}
/* Only joined workers may release the uniquely owned control allocations.
 * The queue owner slot is consumed exactly once. Native/plan executor pins
 * may keep the terminal shell and its gate alive beyond resource retirement. */
static void executor_retire(XL6_ContinuationExecutor* executor)
{
    xrtownershipscope mutation={0};executor_begin(executor,&mutation);
    if(executor->retired){executor_end(executor,&mutation);return;}
    if(!executor->closed||!executor->joined||executor->joining||executor->destroying)abort();
    executor->destroying=true;
    XL6_ContinuationWorker* worker=executor->workers;XL6_ContinuationQueue* queue=executor->queue;
    executor->workers=NULL;executor->queue=NULL;executor->workerCount=0;executor_end(executor,&mutation);
    while (worker != NULL) {
        XL6_ContinuationWorker* next = worker->next;
        if(!worker->exited||worker->wait.queue||worker->wait.parked||worker->blockingDepth)abort();
        xrtThreadDestroy(worker->thread); xrtFree(worker); worker = next;
    }
    if(!xl6ContinuationQueueDestroy(queue))abort();
    executor_begin(executor,&mutation);executor->retired=true;executor->destroying=false;executor_end(executor,&mutation);
}
static bool executor_destroy_entered(XL6_ContinuationExecutor* executor,bool completed)
{
    xrtownershipscope mutation={0};executor_begin(executor,&mutation);
    /* This Destroy owns one entry. A reentrant host acquire/drop or another
     * live API call still uses the queue/worker resources, not just the shell. */
    bool ready=(completed||executor->entries==1)&&executor->closed&&!executor->entrySealed&&
        !executor->joining&&!executor->destroying&&
        (!executor->claim||executor->cleared)&&executor_current_worker(executor)==NULL;
    bool retired=executor->retired;XL6_ContinuationQueue* queue=executor->queue;executor_end(executor,&mutation);
    if(!ready||(!retired&&!(completed?xl6ContinuationQueueWaitCompleted(queue):xl6ContinuationQueueIsIdle(queue))))return false;
    /* Seal API admission in the SAME gate transaction as the final entry
     * check. No IsIdle/Reserve/Close call can enter between this decision and
     * queue retirement. Existing entries make the transition fail closed. */
    executor_begin(executor,&mutation);
    retired=executor->retired;
    ready=executor->entries==1&&executor->closed&&!executor->entrySealed&&
        !executor->joining&&!executor->destroying&&(!executor->claim||executor->cleared)&&
        executor_current_worker(executor)==NULL&&(retired||executor->queue==queue);
    if(ready){executor->entrySealed=true;if(!retired)executor->joining=true;}
    executor_end(executor,&mutation);
    if(!ready)return false;
    if(!retired){
        for(XL6_ContinuationWorker* worker=executor->workers;worker;worker=worker->next)
            if(xrtThreadWait(worker->thread)!=XWAIT_OK)abort();
        executor_begin(executor,&mutation);executor->joining=false;executor->joined=true;executor_end(executor,&mutation);
        executor_retire(executor);
    }
    executor_drop(executor); /* consume the caller's actual owner slot */
    return true;
}
bool xl6ContinuationExecutorDestroy(XL6_ContinuationExecutor* executor)
{
    if(!executor)return true;
    if(!executor_enter_terminal_owner(executor))return false;
    bool destroyed=executor_destroy_entered(executor,false);executor_leave(executor);return destroyed;
}
bool xl6ContinuationExecutorDestroyCompleted(XL6_ContinuationExecutor* executor)
{
    if(!executor)return true;
    if(!executor_enter_terminal_owner(executor))return false;
    bool destroyed=executor_destroy_entered(executor,true);executor_leave(executor);return destroyed;
}
static bool executor_count(const void* data,size_t* count)
{
    const XL6_ContinuationExecutor* executor=data;
    if(!executor||!count||!executor->references||!executor->initialized||executor->cleared||
        executor->entries||executor->joining||executor->destroying||executor->blockedCount)return false;
    if(executor->retired){
        if(executor->queue||executor->workers||executor->workerCount)return false;
    }else{
        if(!executor->queue)return false;
        size_t workers=0;
        for(const XL6_ContinuationWorker* worker=executor->workers;worker;worker=worker->next){
            if(workers++==executor->workerCount||worker->executor!=executor||!worker->thread||worker->blockingDepth)return false;
            if(worker->exited){if(worker->wait.queue||worker->wait.parked||worker->id)return false;}
            else if(executor->joined||!worker->id||!worker->wait.parked||worker->wait.queue!=executor->queue)return false;
        }
        if(workers!=executor->workerCount)return false;
    }
    *count=executor->references;return true;
}
static bool executor_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const XL6_ContinuationExecutor* executor=data;size_t count;
    if(!visit||!executor_count(data,&count))return false;
    if(executor->queue&&!visit(xl6ContinuationQueueOwnership(executor->queue),context))return false;
    /* Worker/control allocations are uniquely contained resources, retained
     * until join, not independent refcounted language objects. Each parked
     * wait slot owns ANOTHER real queue reference acquired by WaitRunOwned. */
    for(const XL6_ContinuationWorker* worker=executor->workers;worker;worker=worker->next)
        if(worker->wait.queue&&!visit(xl6ContinuationQueueOwnership(worker->wait.queue),context))return false;
    return true;
}
static const xrtownershipops executor_ops={executor_count,executor_trace};
static bool executor_busy(const void* data)
{
    const XL6_ContinuationExecutor* executor=data;
    if(!executor || !executor->references || !executor->initialized || executor->cleared || executor->retired)return false;
    if(executor->entries || executor->joining || executor->destroying || executor->blockedCount)return true;
    if(!executor->queue)return false;
    bool busy=false;size_t workers=0;
    for(const XL6_ContinuationWorker* worker=executor->workers;worker;worker=worker->next){
        if(workers++==executor->workerCount || worker->executor!=executor || !worker->thread)return false;
        if(worker->exited){if(worker->wait.queue || worker->wait.parked || worker->id)return false;}
        else if(executor->joined)return false;
        else if(!worker->id || worker->blockingDepth || !worker->wait.parked || worker->wait.queue!=executor->queue)busy=true;
    }
    return workers==executor->workerCount && busy;
}
const XL6_OwnershipQuiescenceV1* xl6ContinuationExecutorQuiescenceV1Get(void)
{
    static const XL6_OwnershipQuiescenceV1 policy={sizeof(policy),&executor_ops,executor_busy};return &policy;
}
xrtownershipref xl6ContinuationExecutorOwnership(const XL6_ContinuationExecutor* executor)
{return (xrtownershipref){executor,executor?&executor_ops:NULL};}
static bool executor_hold(const void* data)
{
    XL6_ContinuationExecutor* executor=(XL6_ContinuationExecutor*)data;xrtownershipscope mutation={0};bool held=false;
    executor_begin(executor,&mutation);
    if(executor->references&&executor->references<SIZE_MAX&&!executor->cleared){++executor->references;held=true;}
    executor_end(executor,&mutation);return held;
}
static bool executor_claim(const void* data,const void* token)
{
    XL6_ContinuationExecutor* executor=(XL6_ContinuationExecutor*)data;
    if(!token||executor->cleared||(executor->claim&&executor->claim!=token))return false;
    executor->claim=token;return true;
}
static void executor_restore(const void* data,const void* token)
{
    XL6_ContinuationExecutor* executor=(XL6_ContinuationExecutor*)data;
    if(!token||executor->claim!=token||executor->cleared)abort();
    executor->claim=NULL;
}
static bool executor_prepared(const void* data)
{
    const XL6_ContinuationExecutor* executor=data;
    return executor->closed&&executor->joined&&!executor->joining&&!executor->destroying;
}
static xrtownershipprepareresult executor_prepare(const void* data,const void* token)
{
    XL6_ContinuationExecutor* executor=(XL6_ContinuationExecutor*)data;xrtownershipscope freeze={0},mutation={0};
    if(!xrtOwnershipFreezeTryBegin(&freeze))return XRT_OWNERSHIP_PREPARE_BUSY;
    if(!token||executor->claim!=token||executor->cleared)abort();
    bool ready=executor_prepared(executor),exited=!executor->entries&&!executor->joining&&!executor->destroying&&
        xl6ContinuationQueuePreparedFor(executor->queue,token);
    for(const XL6_ContinuationWorker* worker=executor->workers;worker;worker=worker->next)
        if(!worker->exited||worker->wait.queue)exited=false;
    if(!ready&&exited){executor_begin(executor,&mutation);executor->joining=true;executor_end(executor,&mutation);}
    if(!xrtOwnershipScopeEnd(&freeze))abort();
    if(ready)return XRT_OWNERSHIP_PREPARE_READY;
    if(!exited)return XRT_OWNERSHIP_PREPARE_BUSY;
    /* Queue preparation already closed new admission. Accepted tasks and
     * their original cleanup must finish before workers can exit. Join is a
     * zero-timeout native probe outside mutation/freeze and all gates. */
    bool joined=true,failed=false;
    for(XL6_ContinuationWorker* worker=executor->workers;worker;worker=worker->next){
        xwaitresult result=xrtThreadWaitFor(worker->thread,0);
        if(result!=XWAIT_OK){joined=false;if(result!=XWAIT_TIMEOUT)failed=true;}
    }
    executor_begin(executor,&mutation);executor->joining=false;executor->closed=true;executor->joined=joined;
    executor_end(executor,&mutation);
    return failed?XRT_OWNERSHIP_PREPARE_FAILED:joined?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void executor_clear(const void* data,const void* token)
{
    XL6_ContinuationExecutor* executor=(XL6_ContinuationExecutor*)data;
    if(!token||executor->claim!=token||executor->cleared||executor->entries||!executor_prepared(executor))abort();
    executor->cleared=true;
}
static bool executor_finish(const void* data,const void* token)
{
    XL6_ContinuationExecutor* executor=(XL6_ContinuationExecutor*)data;
    if(!token||executor->claim!=token||!executor->cleared)abort();
    executor_retire(executor);return true;
}
const xrtownershipadapterv1* xl6ContinuationExecutorOwnershipAdapterV1(xrtownershipref ref,
    const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),executor_hold,executor_drop,executor_claim,executor_restore,NULL,executor_clear,executor_finish};
    static const xrtownershippreparationv1 policy={sizeof(policy),&adapter,executor_prepared,executor_prepare};
    size_t count;
    if(!preparation||ref.Ops!=&executor_ops||!executor_count(ref.Data,&count))return NULL;
    *preparation=&policy;return &adapter;
}
