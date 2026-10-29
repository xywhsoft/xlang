#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#define XRT_MODULE_FUTURE
#include "src/common/xl6_xrt.h"
#include "xl6_continuation_future.h"
#include "xl6_continuation_executor.h"
#include "xl6_future_wait.h"
#include "xl6_ownership_scope.h"
#include "../api/xl_foreign_error.h"
#include <stddef.h>

/* One physical allocation, including the embedded ticket and Watch. Its
 * actual owners are the private caller/queue, current producer, Watch and native/plan
 * references. Code residency belongs to this allocation independently of the
 * queue's accepted execution credit, which can be transferred to a successor. */
typedef struct ContinuationProducer ContinuationProducer;
typedef struct ContinuationFrame {
    xatomic64 gate;
    const XL6_ContinuationFrameLayoutV2* layout;
    const XL6_ContinuationFrameLayoutV3* layoutV3;
    XL6_CodeLease* lease;
    ContinuationProducer* producer;
    size_t references, bytes;
    const void* claim;
    bool bound, active, retired, cleared;
    bool committed, watchInitialized, readyReceived, readyIssued, cancelling;
    max_align_t alignment;
    unsigned char data[];
} ContinuationFrame;
struct ContinuationProducer {
    xatomic64 gate;
    size_t references;
    ContinuationFrame* current;
    xpromise* output; /* borrowed identity, owned by the current activation's outer slot */
    const void* claim;
    bool cleared, cancelRequested, cancelObserved;
};
static const xrtownershipops producer_ops;
static void detach_producer(const void*);
static ContinuationFrame* header(const void* data)
{ return data ? (ContinuationFrame*)((unsigned char*)data - offsetof(ContinuationFrame,data)) : NULL; }
typedef struct TransportOffsets {size_t ticket,watch,source,outer;} TransportOffsets;
static TransportOffsets offsets(const ContinuationFrame* frame)
{
    if(frame->layoutV3){const XL6_ContinuationFrameLayoutV3* l=frame->layoutV3;return (TransportOffsets){l->ticket,l->watch,l->source,l->outer};}
    const XL6_ContinuationFrameLayoutV2* l=frame->layout;return (TransportOffsets){l->ticket,l->watch,l->source,l->outer};
}
static XL6_ContinuationTicket* frame_ticket(const ContinuationFrame* frame)
{ return (XL6_ContinuationTicket*)(frame->data+offsets(frame).ticket); }
static xfuture** frame_source(ContinuationFrame* frame)
{ return (xfuture**)(frame->data+offsets(frame).source); }
static xpromise** frame_outer(ContinuationFrame* frame)
{ return (xpromise**)(frame->data+offsets(frame).outer); }
static void begin(ContinuationFrame* frame,xrtownershipscope* mutation)
{
    if(!xrtOwnershipMutationBegin(mutation))abort();
    for(;;){uint64 expected=0;
        if(xrtAtomic64CompareExchange(&frame->gate,&expected,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED))return;
        xrtThreadYield();
    }
}
static void unlock(ContinuationFrame* frame) { xrtAtomic64Store(&frame->gate,0,XMEMORY_RELEASE); }
static void end(ContinuationFrame* frame,xrtownershipscope* mutation)
{ unlock(frame);if(!xrtOwnershipScopeEnd(mutation))abort(); }
static bool overlap(size_t a,size_t n,size_t b,size_t m) { return a<b+m && b<a+n; }
static bool validate_transport(size_t bytes,const size_t positions[4],
    const XL6_ActivationCaptureLayoutV1* old,const XL6_ActivationCaptureLayoutV2* native)
{
    if(native ? old || !xl6ActivationCaptureRuntimeV2Get()->Validate(native,bytes) :
        !xl6ActivationCaptureRuntimeV1Get()->Validate(old,bytes))return false;
    size_t count=native?native->count:old->count;
    const size_t sizes[]={sizeof(XL6_ContinuationTicket),sizeof(xfuturewatch),sizeof(xfuture*),sizeof(xpromise*)};
    const size_t alignments[]={_Alignof(XL6_ContinuationTicket),_Alignof(xfuturewatch),_Alignof(xfuture*),_Alignof(xpromise*)};
    for(size_t i=0;i<4;++i){
        if(positions[i]>bytes||sizes[i]>bytes-positions[i]||positions[i]%alignments[i])return false;
        for(size_t j=0;j<i;++j)if(overlap(positions[i],sizes[i],positions[j],sizes[j]))return false;
        for(size_t j=0;j<count;++j){
            const XL6_CaptureSlot* value=native?&native->slots[j].value.value:&old->slots[j].value;
            size_t live=native?native->slots[j].initialized:old->slots[j].initialized;
            if(overlap(positions[i],sizes[i],value->offset,value->bytes)||
                (live!=SIZE_MAX&&overlap(positions[i],sizes[i],live,1)))return false;
        }
    }
    return true;
}
static bool validate(const XL6_ContinuationFrameLayoutV2* l,size_t bytes)
{return l && l->size==sizeof(*l) && validate_transport(bytes,(const size_t[]){l->ticket,l->watch,l->source,l->outer},l->captures,NULL);}
static bool validate_v3(const XL6_ContinuationFrameLayoutV3* l,size_t bytes)
{return l && l->size==sizeof(*l) && l->captures && validate_transport(bytes,(const size_t[]){l->ticket,l->watch,l->source,l->outer},NULL,l->captures);}
static bool validate_frame(const ContinuationFrame* f)
{return f->layoutV3 ? !f->layout && validate_v3(f->layoutV3,f->bytes) : validate(f->layout,f->bytes);}
static xl6_error_result cleanup_captures(ContinuationFrame* f,xl6_error_result incoming)
{
    return f->layoutV3 ? xl6ActivationCaptureRuntimeV2Get()->Cleanup(f->layoutV3->captures,f->data,f->bytes,incoming) :
        xl6ActivationCaptureRuntimeV1Get()->Cleanup(f->layout->captures,f->data,incoming);
}
static bool count_frame(const void* data,size_t* count)
{
    const ContinuationFrame* frame=data;
    if(!frame||!count||!frame->references||!frame->bound||frame->active||frame->cancelling||frame->cleared)return false;
    const XL6_ContinuationTicket* ticket=frame_ticket(frame);
    if(!frame->retired){
        if(!xl6ContinuationTicketInspectableV1(ticket))return false;
        size_t expected=1+(ticket->transportOwned&&!(ticket->flags&XL6_CONTINUATION_TRANSPORT_RELEASED)?1u:0u);
        if(ticket->ownershipRefs!=expected||frame->references<expected)return false;
    }
    if(frame->lease){xrtownershipref lease=xl6CodeLeaseOwnership(frame->lease);size_t refs;
        if(!lease.Ops->Count(lease.Data,&refs))return false;}
    *count=frame->references;return true;
}
static bool trace_frame(const void* data,xrtownershipvisitor visit,void* context)
{
    const ContinuationFrame* frame=data;size_t count;
    if(!visit||!count_frame(data,&count))return false;
    if(frame->lease&&!visit(xl6CodeLeaseOwnership(frame->lease),context))return false;
    if(frame->producer&&!visit((xrtownershipref){frame->producer,&producer_ops},context))return false;
    if(frame->retired)return true;
    const XL6_ContinuationTicket* ticket=frame_ticket(frame);
    /* The queue owns a SECOND real lease, separate from physical storage. */
    return (!ticket->codeLease||visit(xl6CodeLeaseOwnership(ticket->codeLease),context))&&
        visit(xrtFutureOwnership(*(xfuture* const*)(frame->data+offsets(frame).source)),context)&&
        visit(xrtPromiseOwnership(*(xpromise* const*)(frame->data+offsets(frame).outer)),context)&&
        (frame->layoutV3 ? xl6ActivationCaptureRuntimeV2Get()->Trace(frame->layoutV3->captures,frame->data,frame->bytes,visit,context) :
            xl6ActivationCaptureRuntimeV1Get()->Trace(frame->layout->captures,frame->data,visit,context));
}
static const xrtownershipops frame_ops={count_frame,trace_frame};
static xrtownershipref ownership(const void* data)
{ return (xrtownershipref){header(data),data?&frame_ops:NULL}; }
static bool hold_header(const void* data)
{
    ContinuationFrame* frame=(ContinuationFrame*)data;xrtownershipscope mutation={0};bool held=false;
    begin(frame,&mutation);
    if(frame->references&&frame->references<SIZE_MAX&&!frame->cleared){++frame->references;held=true;}
    end(frame,&mutation);return held;
}
static bool hold_data(const void* data) { return data&&hold_header(header(data)); }
static void release_header(const void* data)
{
    ContinuationFrame* frame=(ContinuationFrame*)data;xrtownershipscope mutation={0};XL6_CodeLease* lease;
    if(!frame)return;
    begin(frame,&mutation);if(!frame->references)abort();
    if(--frame->references){end(frame,&mutation);return;}
    if(!frame->retired||frame->active||frame->producer||frame_ticket(frame)->queue||*frame_source(frame)||*frame_outer(frame))abort();
    lease=frame->lease;unlock(frame);xrtFree(frame);
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xl6CodeLeaseRelease(lease); /* may unmap descriptors, only resident code remains */
}
static void release_data(const void* data) { release_header(header(data)); }
static const XL6_ContinuationStorageV2 storage={hold_data,release_data,ownership,NULL,NULL,NULL,NULL,NULL};
static void retire(void* data);
static void* allocate_body(size_t bytes,const XL6_ContinuationFrameLayoutV2* layout,const XL6_ContinuationFrameLayoutV3* native)
{
    if(bytes>SIZE_MAX-sizeof(ContinuationFrame)||(native ? layout || !validate_v3(native,bytes) : !validate(layout,bytes))){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    ContinuationFrame* frame=xrtMalloc(sizeof(*frame)+bytes);if(!frame)return NULL;
    *frame=(ContinuationFrame){0};frame->references=1;frame->bytes=bytes;frame->layout=layout;frame->layoutV3=native;
    *frame_ticket(frame)=(XL6_ContinuationTicket){0};
    *(xfuturewatch*)(frame->data+offsets(frame).watch)=(xfuturewatch){0};
    *frame_source(frame)=NULL;*frame_outer(frame)=NULL;
    return frame->data;
}
static void* allocate(size_t bytes,const XL6_ContinuationFrameLayoutV2* layout)
{ XL6_OWNERSHIP_RETURN(void*,allocate_body(bytes,layout,NULL)); }
static void* allocate_v3(size_t bytes,const XL6_ContinuationFrameLayoutV3* layout)
{XL6_OWNERSHIP_RETURN(void*,allocate_body(bytes,NULL,layout));}
static bool bind_frame(void* data)
{
    if(!data){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    ContinuationFrame* frame=header(data);xrtownershipscope mutation={0};bool bound=false;
    begin(frame,&mutation);XL6_ContinuationTicket* ticket=frame_ticket(frame);
    if(!frame->producer&&!frame->active&&!frame->retired&&!frame->cleared&&frame->references&&
        ticket->queue&&ticket->flags==0&&!ticket->transportOwned&&ticket->ownershipRefs==1&&
        !ticket->storage&&!ticket->ownershipTrace&&ticket->data==data&&ticket->release==retire){
        /* A legal setup-failure catch can retry suspension in this SAME
         * private allocation after Abort. Its original metadata lease stays
         * owned; only the new queue reservation needs another execution pin. */
        if(!frame->bound)frame->lease=xl6CodeLeaseRetain(ticket->codeLease);
        frame->bound=true;
        frame->committed=frame->watchInitialized=frame->readyReceived=frame->readyIssued=false;
        ticket->storage=&storage;bound=true;
    }
    end(frame,&mutation);if(!bound)xrtSetErrorKind(XERR_ARGUMENT);return bound;
}
static xl6_error_result cleanup(void* data,xl6_error_result outcome)
{
    if(!data)return outcome;
    ContinuationFrame* frame=header(data);xrtownershipscope mutation={0};XL6_CodeScope code={0};
    begin(frame,&mutation);
    if(!frame->references||frame->active||frame->retired||frame->cleared)abort();
    frame->active=true;xl6CodeScopeEnter(&code,frame->lease);end(frame,&mutation);
    outcome=cleanup_captures(frame,outcome);
    begin(frame,&mutation);frame->active=false;end(frame,&mutation);
    xl6CodeScopeLeave(&code);return outcome;
}
static void retire(void* data)
{
    if(!data)return;
    ContinuationFrame* frame=header(data);xrtownershipscope mutation={0};XL6_CodeScope code={0};
    xl6_error_scope boundary={0};xl6_error_result outcome;xfuture* source;xpromise* outer;
    begin(frame,&mutation);
    if(!frame->references||frame->active||frame->retired||frame->cleared||frame_ticket(frame)->queue)abort();
    frame->active=true;xl6CodeScopeEnter(&code,frame->lease);end(frame,&mutation);
    if(!xl6ErrorScopeBeginCleanup(&boundary,false))abort();
    outcome=cleanup_captures(frame,(xl6_error_result){true,NULL});
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);
    /* Preserve semantic order: source destructor can still observe the output
     * promise while this frame is explicitly active and uninspectable. */
    begin(frame,&mutation);source=*frame_source(frame);*frame_source(frame)=NULL;end(frame,&mutation);
    xrtFutureDestroy(source);if(xrtGetError())xl6ErrorScopeCaptureFailure(&boundary);
    begin(frame,&mutation);outer=*frame_outer(frame);*frame_outer(frame)=NULL;end(frame,&mutation);
    xrtPromiseDestroy(outer);if(xrtGetError())xl6ErrorScopeCaptureFailure(&boundary);
    detach_producer(data);
    outcome=xl6ErrorScopeEnd(&boundary);
    begin(frame,&mutation);frame->active=false;frame->retired=true;end(frame,&mutation);
    xl6CodeScopeLeave(&code);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtSetErrorTake(outcome.error);
}
static void discard(void* data)
{
    if(!data)return;
    /* Discard cannot steal an execution reference. A failed private Reserve
     * must be aborted first; a committed ticket can retire only in RunOne. */
    ContinuationFrame* frame=header(data);
    if(frame_ticket(frame)->queue||(frame_ticket(frame)->flags&XL6_CONTINUATION_COMMITTED))abort();
    retire(data);release_data(data);
}
static void* retain(void* data) { return hold_data(data)?data:NULL; }
static void release(void* data) { release_data(data); }
const XL6_ContinuationFrameRuntimeV2* xl6ContinuationFrameRuntimeV2Get(void)
{
    static const XL6_ContinuationFrameRuntimeV2 api={allocate,bind_frame,cleanup,retire,discard,retain,release,ownership};return &api;
}
const XL6_ContinuationFrameRuntimeV5* xl6ContinuationFrameRuntimeV5Get(void)
{
    static const XL6_ContinuationFrameRuntimeV5 api={allocate_v3,bind_frame,cleanup,retire,discard,retain,release,ownership};return &api;
}
static bool claim(const void* data,const void* token)
{
    ContinuationFrame* frame=(ContinuationFrame*)data;
    if(!token||frame->cleared||(frame->claim&&frame->claim!=token))return false;
    frame->claim=token;return true;
}
static void restore(const void* data,const void* token)
{
    ContinuationFrame* frame=(ContinuationFrame*)data;
    if(!token||frame->claim!=token||frame->cleared)abort();
    frame->claim=NULL;
}
static bool prepared(const void* data)
{
    const ContinuationFrame* frame=data;
    return frame->retired&&!frame->active&&!frame->cancelling&&!frame->producer&&!frame_ticket(frame)->queue;
}
static xrtownershipprepareresult prepare(const void* data,const void* token)
{
    ContinuationFrame* frame=(ContinuationFrame*)data;xrtownershipscope mutation={0};bool ready;
    begin(frame,&mutation);if(!token||frame->claim!=token||frame->cleared)abort();
    ready=prepared(frame);end(frame,&mutation);
    return ready?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void clear(const void* data,const void* token)
{
    ContinuationFrame* frame=(ContinuationFrame*)data;
    if(!token||frame->claim!=token||frame->cleared||!prepared(frame))abort();
    frame->cleared=true;
}
static const xrtownershipadapterv1* producer_adapter(xrtownershipref,const xrtownershippreparationv1**);
const xrtownershipadapterv1* xl6ContinuationFrameOwnershipAdapterV1(xrtownershipref ref,
    const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold_header,release_header,claim,restore,NULL,clear,NULL};
    static const xrtownershippreparationv1 policy={sizeof(policy),&adapter,prepared,prepare};
    size_t count;const ContinuationFrame* frame=ref.Data;
    if(ref.Ops==&producer_ops)return producer_adapter(ref,preparation);
    if(!preparation||ref.Ops!=&frame_ops||!frame||!count_frame(frame,&count)||!validate_frame(frame))return NULL;
    *preparation=&policy;return &adapter;
}

/* Stable logical producer. It owns only the current physical frame, not a
 * chain of old suspensions. Queue execution references remain real owning
 * slots and require independent graph admission. Operations below own storage
 * and once-only cancellation delivery; preparation additionally requires a
 * frozen, claimed actual wait cycle. Source catch/finally remain in the
 * verified resume CFG, never synthesized by the ownership protocol. */
static void producer_begin(ContinuationProducer* producer,xrtownershipscope* mutation)
{
    if(!xrtOwnershipMutationBegin(mutation))abort();
    for(;;){uint64 expected=0;
        if(xrtAtomic64CompareExchange(&producer->gate,&expected,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED))return;
        xrtThreadYield();
    }
}
static void producer_unlock(ContinuationProducer* producer) {xrtAtomic64Store(&producer->gate,0,XMEMORY_RELEASE);}
static void producer_end(ContinuationProducer* producer,xrtownershipscope* mutation)
{producer_unlock(producer);if(!xrtOwnershipScopeEnd(mutation))abort();}
static bool producer_count(const void* data,size_t* count)
{
    const ContinuationProducer* producer=data;
    if(!producer||!count||!producer->references||producer->cleared)return false;
    *count=producer->references;return true;
}
static bool producer_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const ContinuationProducer* producer=data;size_t count;
    return visit&&producer_count(data,&count)&&(!producer->current||visit((xrtownershipref){producer->current,&frame_ops},context));
}
static const xrtownershipops producer_ops={producer_count,producer_trace};
static bool producer_hold(const void* data)
{
    ContinuationProducer* producer=(ContinuationProducer*)data;xrtownershipscope mutation={0};bool held=false;
    producer_begin(producer,&mutation);
    if(producer->references&&producer->references<SIZE_MAX&&!producer->cleared){++producer->references;held=true;}
    producer_end(producer,&mutation);return held;
}
static void producer_release(const void* data)
{
    ContinuationProducer* producer=(ContinuationProducer*)data;xrtownershipscope mutation={0};
    producer_begin(producer,&mutation);if(!producer->references)abort();
    if(--producer->references){producer_end(producer,&mutation);return;}
    if(producer->current)abort(); /* a current frame owns its own producer reference */
    producer_unlock(producer);xrtFree(producer);if(!xrtOwnershipScopeEnd(&mutation))abort();
}
static const xfutureproducerownershipv1 producer_policy={sizeof(producer_policy),producer_release};
static void producer_request_cancel(const void*);
static const xfutureproducercancellationv1 producer_cancellation={sizeof(producer_cancellation),producer_hold,producer_request_cancel};
const xfutureproducerownershipv1* xl6ContinuationProducerPolicyV1Get(void) {return &producer_policy;}
static void frame_ready(ContinuationFrame* frame)
{
    xrtownershipscope mutation={0};bool signal;
    begin(frame,&mutation);frame->readyReceived=true;
    signal=!frame->cancelling&&!frame->readyIssued;
    if(signal)frame->readyIssued=true;
    end(frame,&mutation);
    if(signal)xl6ContinuationReady(frame_ticket(frame));
}
static void frame_watch_ready(void* data) {frame_ready(data);}
static void ready_data(const void* data) {frame_ready(header(data));}
static void frame_watch_released(void* data) {xl6ContinuationTransportReleased(frame_ticket(data));}
static const xfuturewatchownershipv1 watch_policy={sizeof(watch_policy),frame_watch_ready,frame_watch_released,&frame_ops};
const xfuturewatchownershipv1* xl6ContinuationWatchPolicyV1Get(void) {return &watch_policy;}
static bool watch_init(const void* data,xfuturewatch* watch)
{
    ContinuationFrame* frame=header(data);
    if(!frame||!frame->producer||watch!=(xfuturewatch*)(frame->data+offsets(frame).watch)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    bool initialized=xrtFutureWatchInitOwnershipV1(watch,frame,&watch_policy);
    if(initialized)frame->watchInitialized=true; /* registration owner still private */
    return initialized;
}
static void producer_commit(const void* data)
{
    ContinuationFrame* frame=header(data);ContinuationProducer* producer=frame->producer;
    ContinuationFrame* previous;xrtownershipscope mutation={0};
    if(!producer)abort();
    producer_begin(producer,&mutation);previous=producer->current;
    if(previous!=frame){
        XL6_ContinuationTicket* ticket=frame_ticket(frame);
        if(!previous||!ticket->predecessor||frame_ticket(previous)!=ticket->predecessor||!hold_header(frame))abort();
        producer->current=frame;
    }
    producer_end(producer,&mutation);
    begin(frame,&mutation);frame->committed=true;end(frame,&mutation);
    /* ReserveSuccessor still protects the executing predecessor, so this
     * return cannot free storage or invoke its code-owner callback. */
    if(previous!=frame)release_header(previous);
}
static void detach_producer(const void* data)
{
    ContinuationFrame* frame=header(data);ContinuationProducer* producer;xrtownershipscope mutation={0};bool current;
    begin(frame,&mutation);producer=frame->producer;frame->producer=NULL;
    end(frame,&mutation);if(!producer)return;
    producer_begin(producer,&mutation);current=producer->current==frame;
    if(current){producer->current=NULL;producer->output=NULL;}
    producer_end(producer,&mutation);
    if(current)release_header(frame); /* caller/queue still owns the active base reference */
    producer_release(producer);
}
static void producer_published(const void*);
static const XL6_ContinuationStorageV2 produced_storage={hold_data,release_data,ownership,watch_init,producer_commit,detach_producer,ready_data,producer_published};
const XL6_ContinuationStorageV2* xl6ContinuationProducedStorageV2Get(void) {return &produced_storage;}
static bool bind_produced_body(void* data)
{
    if(!data){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    ContinuationFrame* frame=header(data);XL6_ContinuationTicket* ticket=frame_ticket(frame);
    if(!*frame_outer(frame)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!bind_frame(data))return false;
    ContinuationProducer* producer;
    if(ticket->predecessor){
        /* Match the exact resident storage identity BEFORE interpreting the
         * predecessor's data, even when a native caller provided that ticket. */
        if(ticket->predecessor->storage!=&produced_storage){xrtSetErrorKind(XERR_ARGUMENT);return false;}
        ContinuationFrame* previous=header(ticket->predecessor->data);
        producer=previous->producer;
        if(!producer||producer->current!=previous||producer->output!=*frame_outer(frame)||
            !producer_hold(producer)){xrtSetErrorKind(XERR_STATE);return false;}
    }else{
        producer=xrtCalloc(1,sizeof(*producer));if(!producer)return false;
        producer->references=1;
        producer->output=*frame_outer(frame);
        if(!hold_header(frame)){xrtFree(producer);xrtSetErrorKind(XERR_STATE);return false;}
        producer->current=frame;
        if(!xrtPromiseProducerBindTakeV2(*frame_outer(frame),(xrtownershipref){producer,&producer_ops},&producer_policy,&producer_cancellation)){
            producer->current=NULL;release_header(frame);producer_release(producer);return false;
        }
        if(!producer_hold(producer))abort(); /* frame owner, separate from the transferred output owner */
    }
    frame->producer=producer;ticket->storage=&produced_storage;return true;
}
static bool bind_produced(void* data) {XL6_OWNERSHIP_RETURN(bool,bind_produced_body(data));}
const XL6_ContinuationFrameRuntimeV3* xl6ContinuationFrameRuntimeV3Get(void)
{static const XL6_ContinuationFrameRuntimeV3 api={bind_produced};return &api;}
xfuture* xl6ContinuationFrameLaunchV1(void* data,XL6_ContinuationExecutor* executor,
    XL6_ContinuationProc run,const XL6_CodeOwner* owner)
{
    if(!data){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    ContinuationFrame* frame=header(data);XL6_ContinuationTicket* ticket=frame_ticket(frame);
    xfuture* output=NULL;
    if(!executor||!run){xrtSetErrorKind(XERR_ARGUMENT);goto failed;}
    *frame_outer(frame)=xrtPromiseCreate(&output,NULL);
    if(!*frame_outer(frame))goto failed;
    XL6_ContinuationAdmission status=xl6ContinuationExecutorReserveOwned(executor,ticket,run,retire,data,owner);
    if(status!=XL6_CONTINUATION_ADMITTED){
        if(xrtGetError()==NULL)xrtSetErrorKind(status==XL6_CONTINUATION_RESOURCE_FAILURE?XERR_MEMORY:
            status==XL6_CONTINUATION_FULL?XERR_RANGE:XERR_STATE);
        goto failed;
    }
    if(!bind_produced(data))goto failed;
    /* No source transport exists for entry. Commit is LAST: its publication
     * handshake may hand the frame to a worker and retire it before returning.
     * The locally owned output is the only object touched afterwards. */
    frame_ready(frame);
    xl6ContinuationTransportReleased(ticket);
    xl6ContinuationCommit(ticket);
    return output;
failed:
    {
        xl6_error_scope cleanupScope={0};xl6_error_result outcome;
        xerror* primary=xrtTakeError();
        if(!xl6ErrorScopeBegin(&cleanupScope))abort();
        xrtSetErrorTake(primary);xl6ErrorScopeCaptureFailure(&cleanupScope);
        if(ticket->queue&&!xl6ContinuationAbort(ticket))abort();
        discard(data);
        if(xrtGetError())xl6ErrorScopeCaptureFailure(&cleanupScope);
        xrtFutureDestroy(output);
        if(xrtGetError())xl6ErrorScopeCaptureFailure(&cleanupScope);
        outcome=xl6ErrorScopeEnd(&cleanupScope);xl6ErrorScopePropagateFailure(outcome);
    }
    return NULL;
}
static bool observe(void* data,xfutureresult* output)
{
    ContinuationFrame* frame=header(data);xfutureresult input={0};xrtownershipscope mutation={0};
    if(!frame||!output||!frame->producer||!(frame_ticket(frame)->flags&XL6_CONTINUATION_RUNNING)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    xcancel* cancel=xrtPromiseCancelToken(*frame_outer(frame));
    bool requested=cancel&&xrtCancelRequested(cancel);xrtCancelDestroy(cancel);
    ContinuationProducer* producer=frame->producer;bool cancelled=false;
    producer_begin(producer,&mutation);
    if(!producer->cancelObserved&&(requested||producer->cancelRequested)){
        producer->cancelObserved=true;input.State=XFUTURE_CANCELLED;cancelled=true;
    }
    producer_end(producer,&mutation);
    /* An interrupted source is deliberately still pending. Do not manufacture
     * an AGAIN diagnostic (or allocate one) before delivering cancellation. */
    bool valid=cancelled||xrtFutureResult(*frame_source(frame),&input);
    if(valid)*output=input;
    return valid;
}
static bool cancel_wait_claimed(void* data,const void* token)
{
    ContinuationFrame* frame=header(data);xrtownershipscope mutation={0};bool accepted=false,signal;
    if(!frame){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    begin(frame,&mutation);
    /* Commit publishes private Bind/Watch initialization through this gate.
     * Reject before reading those fields when a host races private setup. */
    if(!frame->committed||(token&&frame->claim!=token)||!frame->producer||!frame->watchInitialized||frame->readyIssued||
        frame->cancelling||frame->active||frame->retired||frame->cleared||frame->references==SIZE_MAX){
        end(frame,&mutation);return false;
    }
    /* Pin an additional REAL call-tail reference. Source Ready is deferred
     * while this operation decides and returns the Watch; execution ownership
     * stays with the queue throughout, including an already-released Watch. */
    ++frame->references;frame->cancelling=true;end(frame,&mutation);
    ContinuationProducer* producer=frame->producer;
    producer_begin(producer,&mutation);
    if(!producer->cancelObserved){producer->cancelRequested=true;accepted=true;}
    producer_end(producer,&mutation);
    if(accepted){
        xcancel* cancel=xrtPromiseCancelToken(*frame_outer(frame));
        /* Neither this function nor the Watch removal inherits mutation:
         * native cancellation observers and concurrent Watch tails may wait. */
        if(cancel){(void)xrtCancelRequest(cancel);xrtCancelDestroy(cancel);}
        xrtFutureWatchRemove(*frame_source(frame),(xfuturewatch*)(frame->data+offsets(frame).watch));
    }
    begin(frame,&mutation);
    if(accepted)frame->readyReceived=true;
    frame->cancelling=false;signal=frame->readyReceived&&!frame->readyIssued;
    if(signal)frame->readyIssued=true;
    end(frame,&mutation);
    if(signal)xl6ContinuationReady(frame_ticket(frame));
    release_header(frame);return accepted;
}
static bool cancel_wait(void* data) {return cancel_wait_claimed(data,NULL);}
/* The Future's explicit cancellation hook supplies a real producer reference.
 * Mark the logical request before taking a current-frame snapshot: an active
 * predecessor or provisional successor may refuse interruption, but its next
 * publication handshake cannot lose this request. No producer/queue gate is
 * held while synchronizing the source Watch, and no shared source is cancelled. */
static void producer_request_cancel(const void* data)
{
    ContinuationProducer* producer=(ContinuationProducer*)data;
    ContinuationFrame* frame=NULL;xrtownershipscope mutation={0};
    producer_begin(producer,&mutation);
    if(!producer->cancelObserved){
        producer->cancelRequested=true;
        if(producer->current&&hold_header(producer->current))frame=producer->current;
    }
    producer_end(producer,&mutation);
    if(frame){(void)cancel_wait(frame->data);release_header(frame);}
}
static void producer_published(const void* data)
{
    /* Queue PUBLISHING owns the entire frame through this call and forbids
     * its execution/Retire. The frame's producer reference is therefore real
     * and stable; this is not a borrowed pointer discovered through Trace. */
    ContinuationFrame* frame=header(data);ContinuationProducer* producer=frame->producer;
    xrtownershipscope mutation={0};bool requested;
    if(!producer)abort();
    producer_begin(producer,&mutation);
    requested=producer->cancelRequested&&!producer->cancelObserved;
    producer_end(producer,&mutation);
    if(requested)(void)cancel_wait(frame->data);
}
const XL6_ContinuationFrameRuntimeV4* xl6ContinuationFrameRuntimeV4Get(void)
{static const XL6_ContinuationFrameRuntimeV4 api={observe,cancel_wait};return &api;}
xl6_error_result xl6ContinuationFrameFinishV1(void* data,xl6_error_result incoming,bool* cancelled)
{
    if(!data||!cancelled)abort(); /* initialized, running frame contract */
    ContinuationFrame* frame=header(data);ContinuationProducer* producer=frame->producer;
    *cancelled=false;
    if(incoming.ok&&producer){
        xcancel* token=xrtPromiseCancelToken(*frame_outer(frame));
        bool requested=token&&xrtCancelRequested(token);xrtCancelDestroy(token);
        xrtownershipscope mutation={0};
        producer_begin(producer,&mutation);
        if(!producer->cancelObserved&&(requested||producer->cancelRequested)){
            producer->cancelObserved=true;*cancelled=true;
        }
        producer_end(producer,&mutation);
        if(*cancelled){
            /* No synthetic Error allocation: the explicit false/NULL primary
             * already protects cancellation against any destructor failure. */
            xrtErrorFree(incoming.error);incoming=(xl6_error_result){false,NULL};
        }
    }
    return cleanup(data,incoming);
}
static bool producer_claim(const void* data,const void* token)
{
    ContinuationProducer* producer=(ContinuationProducer*)data;
    if(!token||producer->cleared||(producer->claim&&producer->claim!=token))return false;
    producer->claim=token;return true;
}
static void producer_restore(const void* data,const void* token)
{
    ContinuationProducer* producer=(ContinuationProducer*)data;
    if(!token||producer->claim!=token||producer->cleared)abort();
    producer->claim=NULL;
}
static bool producer_prepared(const void* data)
{
    const ContinuationProducer* producer=data;
    /* The producer's preparation duty is delivery of cooperative shutdown.
     * The physical frame and produced Future separately block all graph
     * finalization/clear until normal execution and retirement have finished. */
    return !producer->current||producer->cancelRequested||producer->cancelObserved;
}
/* Describe this family's real wait under the shared frozen contract. Queue
 * preparation and the current physical frame still require independent
 * claims; the generic solver never learns our private layout or flags. */
bool xl6ContinuationFutureWaitV1(xrtownershipref ref,const void* token,XL6_FutureWaitV1* wait)
{
    const ContinuationProducer* producer=ref.Data;
    if(ref.Ops!=&producer_ops||!producer||!token||!wait||producer->cleared||producer->claim!=token||
        !producer->output||producer_prepared(producer))return false;
    ContinuationFrame* frame=producer->current;XL6_ContinuationTicket* ticket=frame_ticket(frame);
    if(frame->claim!=token||!frame->committed||!frame->watchInitialized||frame->readyReceived||frame->readyIssued||
        frame->cancelling||frame->active||frame->retired||frame->cleared||ticket->flags!=XL6_CONTINUATION_COMMITTED||
        !ticket->transportOwned||ticket->ownershipRefs!=2||!xl6ContinuationQueuePrepareOwns(ticket,token))return false;
    xfuture* source=*frame_source(frame);
    if(!source)return false;
    *wait=(XL6_FutureWaitV1){producer->output,source};return true;
}
static xrtownershipprepareresult producer_prepare(const void* data,const void* token)
{
    ContinuationProducer* producer=(ContinuationProducer*)data;xrtownershipscope freeze={0},mutation={0};
    if(!xrtOwnershipFreezeTryBegin(&freeze))return XRT_OWNERSHIP_PREPARE_BUSY;
    if(!token||producer->claim!=token||producer->cleared)abort();
    bool ready=producer_prepared(data);ContinuationFrame* frame=producer->current;
    XL6_FutureWaitProof proof=!ready&&frame ? xl6FutureWaitCycleMemberV2((xrtownershipref){producer,&producer_ops},token)
        : XL6_FUTURE_WAIT_UNPROVEN;
    /* Scratch allocation/refusal proves nothing. Defer without cancelling or
     * turning a read-only resource shortage into a language callback failure. */
    bool authorized=proof==XL6_FUTURE_WAIT_CYCLE_MEMBER;
    if(authorized&&!hold_header(frame)){
        if(!xrtOwnershipScopeEnd(&freeze))abort();
        return XRT_OWNERSHIP_PREPARE_FAILED;
    }
    if(!xrtOwnershipScopeEnd(&freeze))abort();
    if(ready)return XRT_OWNERSHIP_PREPARE_READY;
    if(!authorized)return XRT_OWNERSHIP_PREPARE_BUSY;
    (void)cancel_wait_claimed(frame->data,token);release_header(frame);
    producer_begin(producer,&mutation);ready=producer_prepared(data);producer_end(producer,&mutation);
    return ready?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void producer_clear(const void* data,const void* token)
{
    ContinuationProducer* producer=(ContinuationProducer*)data;
    if(!token||producer->claim!=token||producer->cleared||producer->current)abort();
    producer->cleared=true;
}
static const xrtownershipadapterv1* producer_adapter(xrtownershipref ref,const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),producer_hold,producer_release,producer_claim,producer_restore,NULL,producer_clear,NULL};
    static const xrtownershippreparationv1 policy={sizeof(policy),&adapter,producer_prepared,producer_prepare};
    size_t count;if(!preparation||ref.Ops!=&producer_ops||!producer_count(ref.Data,&count))return NULL;
    *preparation=&policy;return &adapter;
}
