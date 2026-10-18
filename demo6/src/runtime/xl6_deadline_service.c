#include "xl6_deadline_service.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>
#include <string.h>

typedef XL6_DeadlineServiceV1 Service;
typedef XL6_DeadlineTicketV1 Ticket;
struct XL6_DeadlineServiceV1 {
    xmutex* gate;
    xcond* changed;
    xthread* thread;
    Ticket* tickets;
    size_t capacity,count,bindings,active,references,entries;
    uint64_t workerId;
    xerror* error;
    const void* claim;
    bool initialized,closed,failed,parked,exited,joining,joined,sealed,cleared,retired;
    Ticket* heap[];
};
static const xrtownershipops serviceOps;
static void begin(Service* service,xrtownershipscope* mutation)
{if(!xrtOwnershipMutationBegin(mutation)||!xrtMutexLock(service->gate))abort();}
static void end(Service* service,xrtownershipscope* mutation)
{if(!xrtMutexUnlock(service->gate)||!xrtOwnershipScopeEnd(mutation))abort();}
static void drop_locked(Service* service,xrtownershipscope* mutation)
{
    if(!service->references)abort();
    bool last=--service->references==0;
    if(last&&(!service->retired||service->entries||service->tickets||service->thread||service->changed||service->error))abort();
    if(!xrtMutexUnlock(service->gate))abort();
    if(last){if(!xrtMutexDestroy(service->gate))abort();xrtFree(service);}
    if(!xrtOwnershipScopeEnd(mutation))abort();
}
static bool enter(Service* service,bool terminal,bool detach)
{
    if(!service)return false;
    xrtownershipscope mutation={0};begin(service,&mutation);
    bool ok=service->references&&service->references<SIZE_MAX&&service->entries<SIZE_MAX&&
        !service->retired&&(detach||(!service->sealed&&!service->joining&&(!service->cleared||terminal)));
    if(ok){++service->references;++service->entries;}end(service,&mutation);return ok;
}
static void leave(Service* service)
{xrtownershipscope mutation={0};begin(service,&mutation);if(!service->entries)abort();--service->entries;drop_locked(service,&mutation);}
static void signal_changed(Service* service)
{if(!xrtCondBroadcast(service->changed))abort();}
static void swap(Service* service,size_t a,size_t b)
{
    Ticket* value=service->heap[a];service->heap[a]=service->heap[b];service->heap[b]=value;
    service->heap[a]->heapIndex=a;service->heap[b]->heapIndex=b;
}
static void up(Service* service,size_t at)
{while(at){size_t parent=(at-1)/2;if(service->heap[parent]->due<=service->heap[at]->due)break;swap(service,parent,at);at=parent;}}
static void down(Service* service,size_t at)
{
    while(at<service->count/2){size_t child=at*2+1;
        if(child+1<service->count&&service->heap[child+1]->due<service->heap[child]->due)++child;
        if(service->heap[at]->due<=service->heap[child]->due)break;
        swap(service,at,child);at=child;
    }
}
static void remove_heap(Service* service,Ticket* ticket)
{
    size_t at=ticket->heapIndex;if(at>=service->count||service->heap[at]!=ticket)abort();
    --service->count;
    if(at!=service->count){service->heap[at]=service->heap[service->count];service->heap[at]->heapIndex=at;
        if(at&&service->heap[at]->due<service->heap[(at-1)/2]->due)up(service,at);else down(service,at);}
    service->heap[service->count]=NULL;ticket->heapIndex=SIZE_MAX;
}
/* Consumes the service's ONE target owner and the ticket's ONE service lease.
 * Caller drops Data outside the gate and MUST NOT touch ticket afterwards. */
static const void* clear_ticket(Service* service,Ticket* ticket,const XL6_DeadlineTargetV1** policy)
{
    if(ticket->bound||ticket->dispatching||ticket->heapIndex!=SIZE_MAX||service->references<2)abort();
    if(ticket->previous)ticket->previous->next=ticket->next;
    else {if(service->tickets!=ticket)abort();service->tickets=ticket->next;}
    if(ticket->next)ticket->next->previous=ticket->previous;
    const void* data=ticket->data;*policy=ticket->policy;
    ticket->previous=ticket->next=NULL;ticket->heapIndex=0;ticket->due=0;
    ticket->policy=NULL;ticket->data=NULL;ticket->fired=false;--service->references;
    /* Keep once=1 until the next serialized Bind. A late duplicate Detach
     * cannot acquire a stale pointer after the service lease has ended. */
    xrtAtomicPtrStore(&ticket->owner,NULL,XMEMORY_RELEASE);return data;
}
static void drop_data(const XL6_DeadlineTargetV1* policy,const void* data)
{
    if(!data)return;
    xerror* previous=xrtTakeError();policy->Drop(data);xrtClearError();xrtSetErrorTake(previous);
}
void xl6DeadlineDetachV1(Ticket* ticket)
{
    if(!ticket||xrtAtomic32Exchange(&ticket->once,1,XMEMORY_ACQ_REL))return;
    Service* service=xrtAtomicPtrLoad(&ticket->owner,XMEMORY_ACQUIRE);if(!service)return;
    /* The once-winner's ticket lease keeps service alive through entry. */
    if(!enter(service,false,true))abort();
    xrtownershipscope mutation={0};const void* data=NULL;const XL6_DeadlineTargetV1* policy=NULL;
    begin(service,&mutation);
    if(xrtAtomicPtrLoad(&ticket->owner,XMEMORY_RELAXED)!=service||!ticket->bound||!service->bindings)abort();
    ticket->bound=false;--service->bindings;
    if(!ticket->dispatching){if(ticket->heapIndex!=SIZE_MAX)remove_heap(service,ticket);
        data=clear_ticket(service,ticket,&policy);}
    signal_changed(service);end(service,&mutation);drop_data(policy,data);leave(service);
}
bool xl6DeadlineTicketIsIdleV1(const Ticket* ticket)
{return !ticket||xrtAtomicPtrLoad(&ticket->owner,XMEMORY_ACQUIRE)==NULL;}
xrtownershipref xl6DeadlineServiceOwnershipV1(const Service* service)
{return (xrtownershipref){service,service?&serviceOps:NULL};}
xrtownershipref xl6DeadlineTicketServiceOwnershipV1(const Ticket* ticket)
{return xl6DeadlineServiceOwnershipV1(ticket?xrtAtomicPtrLoad(&ticket->owner,XMEMORY_RELAXED):NULL);}
static void failure(Service* service,xerror* error)
{
    if(service->failed){xrtErrorFree(error);return;}
    service->failed=true;service->closed=true;service->error=error;
}
static int32_t worker(void* data)
{
    Service* service=data;xwaitresult waited=XWAIT_OK;xerror* waitError=NULL;
    for(;;){xrtownershipscope mutation={0};begin(service,&mutation);
        service->parked=false;service->workerId=xrtThreadCurrentId();
        if(waited==XWAIT_ERROR){failure(service,waitError);waitError=NULL;}
        if(service->closed&&!service->count){service->exited=true;service->workerId=0;end(service,&mutation);return 0;}
        Ticket* ticket=service->count?service->heap[0]:NULL;
        if(ticket&&(service->failed||ticket->due<=xrtClock())){
            remove_heap(service,ticket);ticket->dispatching=true;++service->active;
            const XL6_DeadlineTargetV1* policy=ticket->policy;const void* target=ticket->data;
            XL6_DeadlineResultV1 result=service->failed?XL6_DEADLINE_FAILED_V1:XL6_DEADLINE_EXPIRED_V1;
            const xerror* error=service->error;end(service,&mutation);
            policy->Notify(target,result,error);xrtClearError();
            begin(service,&mutation);ticket->dispatching=false;
            const void* released=NULL;const XL6_DeadlineTargetV1* releasePolicy=NULL;
            if(!ticket->bound)released=clear_ticket(service,ticket,&releasePolicy);else ticket->fired=true;
            /* Keep the API/worker phase non-quiescent through mechanical Data
             * release. Data may uniquely contain the just-cleared ticket. */
            end(service,&mutation);drop_data(releasePolicy,released);
            begin(service,&mutation);--service->active;end(service,&mutation);
            waited=XWAIT_OK;continue;
        }
        xdeadline due=ticket?ticket->due:XRT_DEADLINE_NEVER;service->parked=true;
        /* Native CondWait atomically unlocks/relocks gate. No mutation admission
         * survives sleeping. Release returned gate BEFORE entering the next
         * mutation, then recheck all predicates; notifications are not state. */
        if(!xrtOwnershipScopeEnd(&mutation))abort();
        waited=xrtCondWaitUntil(service->changed,service->gate,due);
        if(waited==XWAIT_ERROR)waitError=xrtTakeError();
        else xrtClearError();
        if(!xrtMutexUnlock(service->gate))abort();
    }
}
Service* xl6DeadlineServiceCreateV1(size_t capacity)
{
    if(!capacity||capacity>(SIZE_MAX-sizeof(Service))/sizeof(Ticket*)){xrtSetErrorKind(XERR_RANGE);return NULL;}
    Service* service=xrtCalloc(1,sizeof(*service)+capacity*sizeof(Ticket*));if(!service)return NULL;
    service->references=1;service->capacity=capacity;service->gate=xrtMutexCreate();
    /* Stop at the first failure. A later allocation/init must not replace a
     * real native diagnostic or turn false/NULL into an unrelated error. */
    if(service->gate)service->changed=xrtCondCreate();
    if(service->gate&&service->changed){xrtownershipscope mutation={0};begin(service,&mutation);
        service->thread=xrtThreadCreate(worker,service,0);service->initialized=service->thread!=NULL;end(service,&mutation);
        if(service->initialized)return service;}
    xerror* error=xrtTakeError();
    if(service->changed&&!xrtCondDestroy(service->changed))abort();
    if(service->gate&&!xrtMutexDestroy(service->gate))abort();
    xrtFree(service);xrtClearError();xrtSetErrorTake(error);return NULL;
}
bool xl6DeadlineBindTakeV1(Service* service,Ticket* ticket,xdeadline due,const XL6_DeadlineTargetV1* policy,const void** input)
{
    if(!service||!ticket||due==XRT_DEADLINE_NEVER||!policy||policy->size!=sizeof(*policy)||
        !policy->Notify||!policy->Drop||!policy->Ownership||!input||!*input||!xl6DeadlineTicketIsIdleV1(ticket)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    xrtownershipref owned=policy->Ownership(*input);
    if(!owned.Data||!owned.Ops||!owned.Ops->Count||!owned.Ops->Trace){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!enter(service,false,false)){xrtSetErrorKind(XERR_STATE);return false;}
    xrtownershipscope mutation={0};begin(service,&mutation);bool ok=false;xl6_error_result outcome={true,NULL};
    if(service->failed){outcome.ok=false;outcome.error=xrtErrorRef(service->error);}
    else if(service->closed||service->claim)xrtSetErrorKind(XERR_STATE);
    else if(service->bindings==service->capacity||service->references==SIZE_MAX)xrtSetErrorKind(XERR_RANGE);
    else {
        ticket->due=due;ticket->policy=policy;ticket->data=*input;ticket->bound=true;
        ticket->dispatching=false;ticket->fired=false;ticket->previous=NULL;ticket->next=service->tickets;
        if(ticket->next)ticket->next->previous=ticket;
        service->tickets=ticket;ticket->heapIndex=service->count;service->heap[service->count++]=ticket;
        ++service->bindings;++service->references;xrtAtomic32Store(&ticket->once,0,XMEMORY_RELAXED);
        xrtAtomicPtrStore(&ticket->owner,service,XMEMORY_RELEASE);up(service,ticket->heapIndex);
        *input=NULL;signal_changed(service);ok=true;
    }
    end(service,&mutation);leave(service);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);
    return ok;
}
void xl6DeadlineServiceCloseV1(Service* service)
{
    if(!service||!enter(service,false,false))return;
    xrtownershipscope mutation={0};begin(service,&mutation);service->closed=true;signal_changed(service);end(service,&mutation);leave(service);
}
bool xl6DeadlineServiceCheckV1(Service* service)
{
    if(!enter(service,false,false)){xrtSetErrorKind(XERR_STATE);return false;}
    xrtownershipscope mutation={0};begin(service,&mutation);
    xl6_error_result result={!service->failed,service->failed?xrtErrorRef(service->error):NULL};
    end(service,&mutation);leave(service);if(!result.ok)xl6ErrorScopePropagateFailure(result);return result.ok;
}
bool xl6DeadlineFromMillisecondsV1(int64_t milliseconds,xdeadline* output)
{
    if(!output||(uintptr_t)output%_Alignof(xdeadline)||(uintptr_t)output>UINTPTR_MAX-sizeof(*output)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    if(milliseconds<0||(uint64_t)milliseconds>(UINT64_MAX-1)/1000){xrtSetErrorKind(XERR_RANGE);return false;}
    uint64_t now=xrtClock(),delta=(uint64_t)milliseconds*1000;
    if(delta>=UINT64_MAX-now){xrtSetErrorKind(XERR_RANGE);return false;}
    *output=now+delta;return true;
}
static bool destroy(Service* service,bool wait)
{
    if(!service)return true;
    if(!enter(service,true,false))return false;
    xrtownershipscope mutation={0};begin(service,&mutation);
    bool ready=service->entries==1&&(!service->claim||service->cleared)&&!service->bindings&&!service->count&&
        (wait||(!service->active&&!service->tickets&&service->exited))&&service->workerId!=xrtThreadCurrentId();
    if(!ready){end(service,&mutation);leave(service);return false;}
    service->closed=true;service->joining=true;service->sealed=true;signal_changed(service);xthread* thread=service->thread;
    bool alreadyJoined=service->joined;end(service,&mutation);xwaitresult joined=alreadyJoined?XWAIT_OK:xrtThreadWait(thread);
    if(joined!=XWAIT_OK){begin(service,&mutation);service->joining=false;service->sealed=false;end(service,&mutation);leave(service);return false;}
    xrtThreadDestroy(thread);begin(service,&mutation);
    if(!service->exited||service->active||service->tickets||service->bindings||service->entries!=1)abort();
    service->thread=NULL;service->joining=false;service->joined=true;service->retired=true;
    xcond* condition=service->changed;service->changed=NULL;xerror* error=service->error;service->error=NULL;
    if(service->references<2)abort();
    --service->references;end(service,&mutation);
    if(!xrtCondDestroy(condition))abort();
    xrtErrorFree(error);leave(service);return true;
}
bool xl6DeadlineServiceDestroyV1(Service* service) {return destroy(service,false);}
bool xl6DeadlineServiceDestroyCompletedV1(Service* service) {return destroy(service,true);}
static bool count(const void* data,size_t* output)
{
    const Service* service=data;
    if(!service||!output||!service->references||!service->initialized||service->cleared||service->entries||
        service->joining||service->active||(!service->retired&&!service->exited&&!service->parked))return false;
    *output=service->references;return true;
}
static bool trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const Service* service=data;size_t refs;if(!visit||!count(data,&refs))return false;
    if(!visit(xrtErrorOwnership(service->error),context))return false;
    for(const Ticket* ticket=service->tickets;ticket;ticket=ticket->next)
        if(!visit(ticket->policy->Ownership(ticket->data),context))return false;
    return true;
}
static const xrtownershipops serviceOps={count,trace};
static bool busy(const void* data)
{
    const Service* service=data;
    return service&&service->references&&service->initialized&&!service->retired&&!service->cleared&&
        (service->entries||service->joining||service->active||(!service->exited&&!service->parked));
}
const xrtownershipquiescencev1* xl6DeadlineServiceQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 policy={sizeof(policy),&serviceOps,busy};return &policy;}
static bool hold(const void* data)
{
    Service* service=(Service*)data;xrtownershipscope mutation={0};begin(service,&mutation);
    bool ok=service->references&&service->references<SIZE_MAX&&!service->cleared;
    if(ok)++service->references;
    end(service,&mutation);return ok;
}
static void drop(const void* data)
{Service* service=(Service*)data;xrtownershipscope mutation={0};begin(service,&mutation);drop_locked(service,&mutation);}
static bool claim(const void* data,const void* token)
{
    Service* service=(Service*)data;
    if(!token||service->cleared||service->bindings||service->active||service->entries||
        (service->claim&&service->claim!=token))return false;
    service->claim=token;return true;
}
static void restore(const void* data,const void* token)
{Service* service=(Service*)data;if(service->claim!=token||service->cleared)abort();service->claim=NULL;}
static bool prepared(const void* data)
{const Service* service=data;return service->closed&&service->joined&&!service->joining&&!service->bindings&&!service->tickets&&!service->entries;}
static xrtownershipprepareresult prepare(const void* data,const void* token)
{
    Service* service=(Service*)data;xrtownershipscope freeze={0},mutation={0};
    if(!xrtOwnershipFreezeTryBegin(&freeze))return XRT_OWNERSHIP_PREPARE_BUSY;
    if(!token||service->claim!=token||service->cleared)abort();
    bool ready=prepared(service),joining=!ready&&!service->bindings&&!service->active&&!service->tickets&&!service->entries&&!service->joining;
    if(joining){begin(service,&mutation);service->closed=true;signal_changed(service);end(service,&mutation);}
    if(!xrtOwnershipScopeEnd(&freeze))abort();
    if(ready)return XRT_OWNERSHIP_PREPARE_READY;
    if(!joining)return XRT_OWNERSHIP_PREPARE_BUSY;
    /* Preparation joins only an empty source, outside Freeze, preserving its
     * creator and all diagnostic pins. Final owner destruction stays separate. */
    begin(service,&mutation);bool exited=service->exited;
    if(exited)service->joining=true;
    end(service,&mutation);if(!exited)return XRT_OWNERSHIP_PREPARE_BUSY;
    xwaitresult joined=xrtThreadWaitFor(service->thread,0);
    begin(service,&mutation);service->joining=false;
    if(joined==XWAIT_OK)service->joined=true;
    end(service,&mutation);if(joined==XWAIT_ERROR)return XRT_OWNERSHIP_PREPARE_FAILED;
    if(joined!=XWAIT_OK)return XRT_OWNERSHIP_PREPARE_BUSY;
    return XRT_OWNERSHIP_PREPARE_READY;
}
static void clear(const void* data,const void* token)
{Service* service=(Service*)data;if(service->claim!=token||!prepared(service)||service->cleared)abort();service->cleared=true;}
const xrtownershipadapterv1* xl6DeadlineServiceOwnershipAdapterV1(xrtownershipref ref,const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,restore,NULL,clear,NULL};
    static const xrtownershippreparationv1 policy={sizeof(policy),&adapter,prepared,prepare};size_t refs;
    if(!preparation||ref.Ops!=&serviceOps||!count(ref.Data,&refs))return NULL;
    *preparation=&policy;return &adapter;
}
