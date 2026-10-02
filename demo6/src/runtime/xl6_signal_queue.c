#include "src/runtime/xl6_text_thread.h"
#include <stdlib.h>
#include <string.h>

typedef struct SignalRecord {
    xsignal code;
    int32 systemCode;
    uint64 count, total;
    xtime time;
    bool coalesced, saturated;
} SignalRecord;
typedef struct SignalQueue {
    XL6_TextThreadOwner owner;
    xmutex* lock;
    xcond* changed;
    xsignalwatch* watch;
    xsignal code;
    size_t capacity, head, used;
    bool closed, prepared;
    SignalRecord pending;
    SignalRecord records[];
} SignalQueue;
static void lock(SignalQueue* q) { if (!xrtMutexLock(q->lock)) abort(); }
static void unlock(SignalQueue* q) { if (!xrtMutexUnlock(q->lock)) abort(); }
static SignalQueue* check(void* handle)
{
    SignalQueue* q=handle;
    if (!q) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return xl6TextThreadCheck(&q->owner) ? q : NULL;
}
/* XRT's dispatcher calls only this resident recorder. It never retains or
 * invokes script, allocates payloads, or waits for a consumer. The mutex
 * protects only bounded metadata copies, never user code or snapshot alloc. */
static void record(xsignalwatch* watch, const xsignalevent* event, void* data)
{
    SignalQueue* q=data;
    (void)watch;
    lock(q);
    if (!q->closed) {
        SignalRecord item={event->Code,event->SystemCode,event->Count,event->Total,event->Time,false,false};
        if (q->used==q->capacity) {
            SignalRecord* last=&q->records[(q->head+q->used-1)%q->capacity];
            item.coalesced=true;
            item.saturated=last->saturated || UINT64_MAX-last->count<item.count;
            item.count=item.saturated ? UINT64_MAX : last->count+item.count;
            *last=item;
        } else {
            q->records[(q->head+q->used)%q->capacity]=item;
            ++q->used;
        }
        if (!xrtCondSignal(q->changed)) abort();
    }
    unlock(q);
}
/* Do not hold our mutex while Off waits for the recorder. The caller owns the
 * entire allocation and serializes consumer/destruction entries. A native
 * watch owns only a borrowed Data slot until this synchronous retirement. */
static bool closeQueue(SignalQueue* q)
{
    lock(q);
    xsignalwatch* watch=q->watch;
    q->watch=NULL; q->closed=true; q->used=0; q->prepared=false;
    if (!xrtCondBroadcast(q->changed)) abort();
    unlock(q);
    bool ok=!watch || xrtSignalOff(watch);
    xrtSignalFree(watch);
    return ok;
}
void xl6SignalQueueDestroyV1(void* handle)
{
    SignalQueue* q=handle;
    if (!q) return;
    /* Destruction is no-raise, including rollback on a constructor OOM.
     * Explicit close remains checked. Never replace an ambient exception. */
    xerror* prior=xrtTakeError();
    if (q->lock && q->changed) (void)closeQueue(q);
    if (q->changed && !xrtCondDestroy(q->changed)) abort();
    if (q->lock && !xrtMutexDestroy(q->lock)) abort();
    xl6TextThreadDrop(&q->owner); xrtFree(q);
    xrtClearError(); xrtSetErrorTake(prior);
}
void* xl6SignalQueueCreateV1(int64_t value, bool once, int64_t capacity)
{
    if (value<INT_MIN || value>INT_MAX || capacity<1 || capacity>65536 ||
        (uint64_t)capacity>(SIZE_MAX-sizeof(SignalQueue))/sizeof(SignalRecord)) {
        xrtSetErrorKind(XERR_RANGE);return NULL;
    }
    SignalQueue* q=xrtCalloc(1,sizeof(*q)+(size_t)capacity*sizeof(SignalRecord));
    if (!q) return NULL;
    q->capacity=(size_t)capacity; q->code=(xsignal)value;
    if (!xl6TextThreadInit(&q->owner)) goto fail;
    q->lock=xrtMutexCreate(); if (!q->lock) goto fail;
    q->changed=xrtCondCreate(); if (!q->changed) goto fail;
    q->watch=once ? xrtSignalOnce(q->code,record,q) : xrtSignalOn(q->code,record,q);
    if (!q->watch) goto fail;
    return q;
fail:
    xl6SignalQueueDestroyV1(q);return NULL;
}
void xl6SignalQueueCheckOwnerV1(void* handle) { (void)check(handle); }
bool xl6SignalQueueCloseV1(void* handle)
{ SignalQueue* q=check(handle);return q && closeQueue(q); }
bool xl6SignalQueueActiveV1(void* handle)
{
    SignalQueue* q=check(handle);if(!q)return false;
    lock(q); bool active=q->watch && xrtSignalActive(q->watch);unlock(q);return active;
}
int64_t xl6SignalQueueCodeV1(void* handle)
{ SignalQueue* q=check(handle);return q ? (int64_t)q->code : 0; }
int64_t xl6SignalQueuePendingV1(void* handle)
{
    SignalQueue* q=check(handle);if(!q)return 0;
    lock(q);size_t n=q->used+(q->prepared?1:0);unlock(q);return (int64_t)n;
}
bool xl6SignalQueueClosedV1(void* handle)
{ SignalQueue* q=check(handle);if(!q)return false;lock(q);bool closed=q->closed;unlock(q);return closed; }
bool xl6SignalQueuePrepareV1(void* handle, int64_t timeoutMs)
{
    SignalQueue* q=check(handle);if(!q)return false;
    if (timeoutMs < -1 || (timeoutMs>=0 && (uint64_t)timeoutMs>UINT64_MAX/1000)) {
        xrtSetErrorKind(XERR_RANGE);return false;
    }
    uint64 now=xrtClock(), delta=timeoutMs<0 ? 0 : (uint64_t)timeoutMs*1000;
    if (timeoutMs>=0 && delta>=UINT64_MAX-now) {xrtSetErrorKind(XERR_RANGE);return false;}
    xdeadline deadline=timeoutMs<0 ? XRT_DEADLINE_NEVER : now+delta;
    lock(q);
    for (;;) {
        if (q->prepared) {unlock(q);return true;}
        if (q->used) {
            q->pending=q->records[q->head];q->head=(q->head+1)%q->capacity;
            --q->used;q->prepared=true;unlock(q);return true;
        }
        if (q->closed) {unlock(q);return false;}
        if (!xrtSignalHealthy()) {unlock(q);return false;}
        if (!xrtSignalActive(q->watch)) {
            /* Once becomes inactive BEFORE its selected recorder runs.
             * Synchronize that in-flight callback without our mutex, then
             * recheck its event before reporting the stream exhausted. */
            xsignalwatch* watch=q->watch;unlock(q);
            bool ok=xrtSignalOff(watch);lock(q);
            if (!ok) {unlock(q);return false;}
            if (q->used) continue;
            unlock(q);return false;
        }
        if (!timeoutMs || xrtDeadlineExpired(deadline)) {unlock(q);return false;}
        /* Restore/ignore/shutdown are process-global XRT operations and do
         * not signal this private cond. Recheck disposition/health at most
         * every 10ms; use one absolute deadline across spurious wakes. */
        uint64 remaining=xrtDeadlineRemaining(deadline);
        xwaitresult result=xrtCondWaitFor(q->changed,q->lock,remaining<10000?remaining:10000);
        if (result!=XWAIT_OK && result!=XWAIT_TIMEOUT) {unlock(q);return false;}
    }
}
void* xl6SignalQueueSnapshotV1(void* handle)
{
    SignalQueue* q=check(handle);if(!q)return NULL;
    SignalRecord value;
    lock(q);
    if (!q->prepared || q->closed) {unlock(q);xrtSetErrorKind(XERR_STATE);return NULL;}
    value=q->pending;unlock(q);
    SignalRecord* copy=xrtMalloc(sizeof(*copy));if(copy)*copy=value;return copy;
}
void xl6SignalQueueConsumeV1(void* handle)
{
    SignalQueue* q=check(handle);if(!q)return;
    lock(q);
    if (!q->prepared || q->closed) {unlock(q);xrtSetErrorKind(XERR_STATE);return;}
    q->prepared=false;unlock(q);
}
void xl6SignalEventDestroyV1(void* handle) { xrtFree(handle); }
static SignalRecord* eventCheck(void* handle)
{ if(!handle)xrtSetErrorKind(XERR_ARGUMENT);return handle; }
int64_t xl6SignalEventCodeV1(void* handle) { SignalRecord* e=eventCheck(handle);return e?e->code:0; }
int64_t xl6SignalEventSystemCodeV1(void* handle) { SignalRecord* e=eventCheck(handle);return e?e->systemCode:0; }
uint64_t xl6SignalEventCountV1(void* handle) { SignalRecord* e=eventCheck(handle);return e?e->count:0; }
uint64_t xl6SignalEventTotalV1(void* handle) { SignalRecord* e=eventCheck(handle);return e?e->total:0; }
int64_t xl6SignalEventTimeV1(void* handle) { SignalRecord* e=eventCheck(handle);return e?e->time:0; }
bool xl6SignalEventCoalescedV1(void* handle) { SignalRecord* e=eventCheck(handle);return e&&e->coalesced; }
bool xl6SignalEventSaturatedV1(void* handle) { SignalRecord* e=eventCheck(handle);return e&&e->saturated; }
xstrview xl6SignalEventNameV1(void* handle)
{
    SignalRecord* e=eventCheck(handle);if(!e)return xl6TextFailure;
    const char* name=xrtSignalName(e->code);return xl6TextOwn(xrtStrDup(name),strlen(name));
}
