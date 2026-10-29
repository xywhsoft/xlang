#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_TASK_POOL
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_task.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <windows.h>

typedef struct Context {
    volatile LONG started,release;
    unsigned mode,runs,finalizers,probes;
    bool finalizing;
    xrtownershipref frame,job;
} Context;
typedef struct Payload { Context* context;xvalue* first;xfuture* held;xvalue* last; } Payload;
#define SLOT(K,F) {K,offsetof(Payload,F),sizeof(((Payload*)0)->F)}
static const XL6_CaptureSlot slots[]={SLOT(XL6_CAPTURE_TRIVIAL,context),
    SLOT(XL6_CAPTURE_XVALUE,first),SLOT(XL6_CAPTURE_FUTURE,held),SLOT(XL6_CAPTURE_XVALUE,last)};
static const XL6_CaptureLayoutV1 layout={sizeof(layout),sizeof(Payload),4,slots};
static const XL6_TaskRuntimeV1* api;
static const xrtownershipops* futureOps;
static Context* current;
static unsigned graphs,rooted,prepares,notifications,finalizations,activeProbes;
static LONG flag(volatile LONG* value){return InterlockedCompareExchange(value,0,0);}
static void await(volatile LONG* value)
{ULONGLONG end=GetTickCount64()+10000;while(!flag(value)){assert(GetTickCount64()<end);SwitchToThread();}}
static void freeze_begin(xrtownershipscope* freeze)
{ULONGLONG end=GetTickCount64()+5000;while(!xrtOwnershipFreezeTryBegin(freeze)){assert(GetTickCount64()<end);SwitchToThread();}}
static const xrtownershipadapterv1* job_adapter(xrtownershipref ref,const xrtownershippreparationv1** prep)
{const xtaskdataownershipv1* tasks[]={xl6TaskNativeDataPolicyV1Get()};return xrtTaskOwnershipAdapterV1(ref,tasks,1,prep);}
/* A real test-owned bundle keeps precisely the values whose retirement is
 * under test. Its slots own actual retained references, not invented roots.
 * Active worker refusal is tested before quiescence; the complete shared Plan
 * subsequently proves class/frame/Job/pool-shell retirement and root removal. */
typedef struct HeldGraph {
    size_t refs;const void* claim;bool cleared;
    xrtownershipref frame,job;
    const xrtownershipadapterv1 *frameAdapter,*jobAdapter;
    xvalue *first,*last;xfuture* output;
} HeldGraph;
static bool held_count(const void* data,size_t* count)
{const HeldGraph* graph=data;if(!graph||!count||!graph->refs||graph->cleared)return false;*count=graph->refs;return true;}
static bool held_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const HeldGraph* graph=data;size_t count;
    return visit&&held_count(data,&count)&&visit(graph->frame,context)&&visit(graph->job,context)&&
        visit(xrtValueOwnership(graph->first),context)&&visit(xrtValueOwnership(graph->last),context)&&
        visit(xrtFutureOwnership(graph->output),context);
}
static const xrtownershipops held_ops={held_count,held_trace};
static bool held_hold(const void* data)
{
    HeldGraph* graph=(HeldGraph*)data;xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    assert(graph->refs&&graph->refs<SIZE_MAX&&!graph->cleared);++graph->refs;assert(xrtOwnershipScopeEnd(&mutation));return true;
}
static void held_drop(const void* data)
{
    HeldGraph* graph=(HeldGraph*)data;xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    assert(graph->refs);if(!--graph->refs){assert(graph->cleared&&!graph->frame.Data&&!graph->job.Data&&!graph->first&&!graph->last&&!graph->output);xrtFree(graph);}
    assert(xrtOwnershipScopeEnd(&mutation));
}
static bool held_claim(const void* data,const void* token)
{HeldGraph* graph=(HeldGraph*)data;if(!token||graph->cleared||(graph->claim&&graph->claim!=token))return false;graph->claim=token;return true;}
static void held_restore(const void* data,const void* token)
{HeldGraph* graph=(HeldGraph*)data;assert(token&&graph->claim==token&&!graph->cleared);graph->claim=NULL;}
static void held_clear(const void* data,const void* token)
{HeldGraph* graph=(HeldGraph*)data;assert(token&&graph->claim==token&&!graph->cleared);graph->cleared=true;}
static bool held_finish(const void* data,const void* token)
{
    HeldGraph* graph=(HeldGraph*)data;assert(token&&graph->claim==token&&graph->cleared);
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    xrtownershipref frame=graph->frame,job=graph->job;xvalue *first=graph->first,*last=graph->last;xfuture* output=graph->output;
    graph->frame=(xrtownershipref){0};graph->job=(xrtownershipref){0};graph->first=graph->last=NULL;graph->output=NULL;
    assert(xrtOwnershipScopeEnd(&mutation));
    xrtFutureDestroy(output);xrtValueRelease(first);xrtValueRelease(last);
    if(frame.Data)graph->frameAdapter->Drop(frame.Data);
    if(job.Data)graph->jobAdapter->Drop(job.Data);
    return true;
}
static const xrtownershipadapterv1 held_adapter={sizeof(held_adapter),held_hold,held_drop,held_claim,held_restore,NULL,held_clear,held_finish};
static bool resolve(xrtownershipref ref,void* unused,const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** prep)
{
    (void)unused;*prep=NULL;
    if(ref.Ops==&held_ops&&ref.Data){*adapter=&held_adapter;return true;}
    if((*adapter=xl6TaskPoolOwnershipAdapterV1(ref,prep)))return true;
    if((*adapter=xrtTaskPoolOwnershipAdapterV1(ref,prep)))return true;
    if((*adapter=xl6TaskOwnershipAdapterV1(ref,prep)))return true;
    if((*adapter=job_adapter(ref,prep)))return true;
    const xfutureproducerownershipv1* producers[]={xl6TaskProducerPolicyV1Get(),xrtTaskProducerPolicyV1Get()};
    const xfuturewatchownershipv1* watches[]={xl6TaskWatchPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[XL6_FUTURE_PAYLOAD_COUNT-XL6_FUTURE_PAYLOAD_VALUE];
    for(unsigned i=0;i<sizeof(payloads)/sizeof(*payloads);++i)payloads[i]=xl6PayloadPolicyGet((XL6_FuturePayloadKind)(XL6_FUTURE_PAYLOAD_VALUE+i));
    if((*adapter=xrtFutureOwnershipAdapterV3(ref,payloads,sizeof(payloads)/sizeof(*payloads),producers,2,watches,1,prep)))return true;
    if((*adapter=xl6ClassOwnershipAdapterV1(ref)))return true;
    if((*adapter=xrtCancelOwnershipAdapterV1(ref)))return true;
    if((*adapter=xrtErrorOwnershipAdapterV1(ref)))return true;
    if((*adapter=xl6PayloadOwnershipAdapterV1(ref)))return true;
    *adapter=xrtValueOwnershipAdapterV1(ref);return *adapter!=NULL;
}
static DWORD WINAPI cross_thread(void* data)
{
    Context* context=data;xrtownershipscope freeze={0};freeze_begin(&freeze);
    if(!context->finalizing){
        const xrtownershippreparationv1* prep=NULL;size_t count=777;
        assert(!xl6TaskOwnershipAdapterV1(context->frame,&prep)&&!prep);
        assert(!job_adapter(context->job,&prep)&&!prep);
        assert(!context->frame.Ops->Count(context->frame.Data,&count)&&count==777);
        assert(!context->job.Ops->Count(context->job.Data,&count)&&count==777);
    }
    assert(xrtOwnershipScopeEnd(&freeze));return 0;
}
static void probe(Context* context)
{
    HANDLE thread=CreateThread(NULL,0,cross_thread,context,0,NULL);
    assert(thread&&WaitForSingleObject(thread,10000)==WAIT_OBJECT_0&&CloseHandle(thread));++context->probes;
}
static bool finalize(xvalue* value,void* unused)
{
    (void)value;(void)unused;assert(current->finalizing);++current->finalizers;probe(current);return true;
}
static XL6_TaskOutcome run(xcancel* cancel,void* data,XL6_TaskResult* result)
{
    Payload* payload=data;Context* context=payload->context;(void)cancel;
    ++context->runs;probe(context);assert(!context->finalizers&&context->mode!=2);
    if(context->mode){xrtSetErrorKind(context->mode==1?XERR_STATE:XERR_CLOSED);return XL6_TASK_FAILED;}
    int64* value=xrtMalloc(sizeof(*value));assert(value);*value=79;
    *result=(XL6_TaskResult){value,xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_VALUE),NULL};return XL6_TASK_SUCCESS;
}
static XL6_TaskOutcome block(xcancel* cancel,void* data,XL6_TaskResult* result)
{Context* context=*(Context**)data;(void)cancel;(void)result;InterlockedExchange(&context->started,1);await(&context->release);return XL6_TASK_SUCCESS;}
static void block_drop(void* data,void* unused){(void)data;(void)unused;}
static bool empty(const void* data,xrtownershipvisitor visit,void* context){(void)data;(void)visit;(void)context;return true;}
static bool find_job(xrtownershipref ref,void* data)
{Context* context=data;const xrtownershippreparationv1* prep=NULL;if(job_adapter(ref,&prep)){assert(!context->job.Data);context->job=ref;}return true;}
static bool find_source_job(xrtownershipref ref,void* data)
{
    /* Follow only the actual Future identity observed at the output. This
     * is traversal of real frame slots, not a Promise cast or hidden root. */
    Context* context=data;
    if(!context->job.Data&&ref.Data&&ref.Ops==futureOps)
        return ref.Ops->Trace(ref.Data,find_job,data);
    return true;
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void transaction(unsigned mode,bool language)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Context context={0},blocked={0};context.mode=mode;current=&context;
    XL6_TaskPoolConfig config={1,4,0};XL6_TaskPool* pool=api->PoolCreate(&config);assert(pool);
    Context** blocking=api->Allocate(NULL,sizeof(*blocking),block,block_drop,empty);assert(blocking);*blocking=&blocked;
    xfuture* first=api->Submit(pool,blocking,NULL,NULL,false);assert(first);await(&blocked.started);
    Payload* payload=xl6TaskRuntimeV2Get()->Allocate(NULL,sizeof(*payload),run,&layout);assert(payload);
    *payload=(Payload){0};payload->context=&context;
    payload->first=xl6ClassRuntimeV2Get()->CommitTake(xl6ClassRuntimeV2Get()->Create(NULL,finalize));
    payload->last=xl6ClassRuntimeV2Get()->CommitTake(xl6ClassRuntimeV2Get()->Create(NULL,finalize));assert(payload->first&&payload->last);
    context.frame=api->Ownership(payload);
    xrtownershipscope freeze={0};freeze_begin(&freeze);
    const xrtownershippreparationv1* absent=NULL;
    assert(!xl6TaskOwnershipAdapterV1(context.frame,&absent)&&!absent);assert(xrtOwnershipScopeEnd(&freeze));
    xfuture* output=xl6TaskSubmitPayloadV1(pool,payload,NULL,XL6_FUTURE_PAYLOAD_VALUE,language);assert(output);
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    payload->held=xrtFutureRef(output);assert(payload->held);assert(xrtOwnershipScopeEnd(&mutation));
    xrtownershipref root=xrtFutureOwnership(output);futureOps=root.Ops;freeze_begin(&freeze);
    if(language)assert(context.frame.Ops->Trace(context.frame.Data,find_source_job,&context));
    else assert(root.Ops->Trace(root.Data,find_job,&context));
    assert(context.job.Data);
    xrtownershipresult graph={0};assert(!xrtOwnershipInspect(&root,1,NULL,0,&graph));xrtClearError();
    const xrtownershippreparationv1 *framePrep=NULL,*jobPrep=NULL;
    const xrtownershipadapterv1* frameAdapter=xl6TaskOwnershipAdapterV1(context.frame,&framePrep);
    const xrtownershipadapterv1* jobAdapter=job_adapter(context.job,&jobPrep);
    assert(frameAdapter&&jobAdapter&&framePrep&&!framePrep->Ready(context.frame.Data)&&jobPrep&&!jobPrep->Ready(context.job.Data));
    assert(frameAdapter->Hold(context.frame.Data)&&jobAdapter->Hold(context.job.Data));
    HeldGraph* retained=xrtCalloc(1,sizeof(*retained));assert(retained);
    *retained=(HeldGraph){1,NULL,false,context.frame,context.job,frameAdapter,jobAdapter,
        xrtValueRetain(payload->first),xrtValueRetain(payload->last),output};assert(retained->first&&retained->last);
    assert(frameAdapter->Claim(context.frame.Data,&context)&&jobAdapter->Claim(context.job.Data,&context));
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(framePrep->Prepare(context.frame.Data,&context)==XRT_OWNERSHIP_PREPARE_BUSY);
    assert(jobPrep->Prepare(context.job.Data,&context)==XRT_OWNERSHIP_PREPARE_BUSY);prepares+=2;
    freeze_begin(&freeze);frameAdapter->Restore(context.frame.Data,&context);jobAdapter->Restore(context.job.Data,&context);
    assert(xrtOwnershipScopeEnd(&freeze));
    /* The holds now belong to retained's explicit owning slots. */
    if(mode==2)assert(xrtFutureCancel(output));
    InterlockedExchange(&blocked.release,1);
    assert(xrtFutureWaitFor(output,10000000)==XWAIT_OK&&api->PoolQuiesce(pool)&&api->PoolDestroy(pool));
    assert(context.runs==(mode==2?0u:1u)&&!context.finalizers);activeProbes+=context.probes;
    assert(!payload->first&&!payload->last&&!payload->held);++notifications;
    xfutureresult result={0};assert(xrtFutureResult(output,&result));
    assert(result.State==(mode==0?XFUTURE_RESOLVED:mode==2?XFUTURE_CANCELLED:mode==3&&language?XFUTURE_CLOSED:XFUTURE_FAILED));
    if(mode==0)assert(result.Value&&*(int64*)result.Value==79);
    if(mode==1||(!language&&mode==3))assert(result.Error&&xrtErrorKind(result.Error)==(mode==1?XERR_STATE:XERR_CLOSED));
    freeze_begin(&freeze);const xrtownershippreparationv1* again=NULL;size_t count=0;
    assert(xl6TaskOwnershipAdapterV1(context.frame,&again)==frameAdapter&&again==framePrep&&framePrep->Ready(context.frame.Data));
    assert(context.frame.Ops->Count(context.frame.Data,&count)&&count==1);
    assert(job_adapter(context.job,&again)==jobAdapter&&again==jobPrep&&jobPrep->Ready(context.job.Data));
    assert(context.job.Ops->Count(context.job.Data,&count)&&count==1);
    root=(xrtownershipref){retained,&held_ops};
    assert(xrtOwnershipInspect(&root,1,NULL,0,&graph)&&graph.ExternalRootCount==1);++graphs;
    assert(xrtOwnershipScopeEnd(&freeze));
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY&&!context.finalizers);
    assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);++rooted;
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);plan=NULL;
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6OwnershipPlanTryRefresh(plan)==XL6_OWNERSHIP_PLAN_ROOTED);++rooted;
    held_drop(retained);xrtFutureDestroy(first);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);context.finalizing=true;
    assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY&&context.finalizers==2);finalizations+=context.finalizers;
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY&&xl6OwnershipPlanFinish(plan)&&xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);balance(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();api=xl6TaskRuntimeV1Get();
    for(unsigned n=0;n<25;++n)for(unsigned mode=0;mode<4;++mode)for(unsigned language=0;language<2;++language)transaction(mode,language!=0);
    assert(graphs==200&&rooted==400&&prepares==400&&notifications==200&&finalizations==400&&activeProbes==150);
    puts("Task ownership lifecycle: 200 complete retired raw/language Job graphs, 200 active-pool refusals, 400 real-root deferrals, 400 queued BUSY preparations, 200 normal outcomes, 400 class finalizers, 150 active refusals and 400 finalizer freeze probes; prepare/finalize/clear/finish with exact allocator balance");return 0;
}
