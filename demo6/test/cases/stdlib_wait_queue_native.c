#include "src/runtime/xl6_wait_queue.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_cond_internal.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ID UINT64_C(0xCC19)
#define ELEMENT UINT64_C(0xCC20)
#define SLOT(V) (XL6_ChannelSlotV1){&(V),sizeof(V)}
static xvalue* seed(unsigned kind,bool nullable)
{
    if(kind==3)return xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,XL6_VALUE_ADMIT_ANY,nullable,ELEMENT);
    if(kind==4)return xl6SequenceFutureCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,ELEMENT,nullable);
    return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,kind,
        kind==2?sizeof(xstrview):sizeof(int64_t),kind==2?xl6StringTypeV1():xrtTypeInt64(),
        kind==2?XL6_VALUE_ADMIT_STRING:XL6_VALUE_ADMIT_I64,nullable,ELEMENT);
}
static xvalue* create(unsigned kind,bool nullable,int64_t capacity)
{xvalue* q=xl6WaitQueueCreateTakeV1(ID,seed(kind,nullable),capacity);assert(q);return q;}
static xqueueresult push(const xvalue* q,const xrttype* type,XL6_ChannelSlotV1 slot)
{return xl6WaitQueuePushV1(q,ID,ELEMENT,slot.size,type,slot.data);}
static bool receive(const xvalue* q,const xrttype* type,XL6_ChannelSlotV1 slot,XL6_ChannelWaitV1 mode,
    int64_t ms,XL6_ChannelStatusV1* status)
{return xl6WaitQueueReceiveV1(q,ID,ELEMENT,type,slot,mode,ms,status);}
static void payloadDrop(void* payload,void* context){(void)context;xrtValueRelease(payload);}
static bool payloadTrace(const void* payload,const void* data,xrtownershipvisitor visit,void* context)
{(void)data;return visit(xrtValueOwnership(payload),context);}
static const xfuturepayloadownershipv1 payloadPolicy={sizeof(payloadPolicy),payloadDrop,payloadTrace};
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* user)
{
    const XL6_SequenceAdmissionOpsV2* policies[]={xl6ArrayFutureAdmissionOpsV2Get()};
    const xfuturepayloadownershipv1* payloads[]={&payloadPolicy};
    const xrtownershipadapterv1* adapter=xl6WaitQueueOwnershipAdapterV1(ref,user?NULL:policies,user?0:1);
    if(!adapter)adapter=xl6SequenceOwnershipAdapterV2(ref,user?NULL:policies,user?0:1);
    if(!adapter)adapter=xl6SyncOwnershipAdapterV1(ref);
    if(!adapter)adapter=xrtFutureOwnershipAdapterV1(ref,payloads,1);
    if(!adapter)adapter=xrtCancelOwnershipAdapterV1(ref);
    if(!adapter)adapter=xrtErrorOwnershipAdapterV1(ref);
    if(!adapter)adapter=xrtValueOwnershipAdapterV1(ref);
    return adapter;
}
static void vectors(void)
{
    xvalue* q=create(1,false,2);XL6_ChannelStatusV1 status=XL6_CHANNEL_FULL_V1;
    int64_t input=7,output=99;
    assert(receive(q,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_TRY_V1,0,&status));
    assert(status==XL6_CHANNEL_EMPTY_V1 && output==99);
    assert(receive(q,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_FOR_V1,0,&status));
    assert(status==XL6_CHANNEL_TIMEOUT_V1 && output==99);
    assert(push(q,xrtTypeInt64(),SLOT(input))==XQUEUE_OK);
    assert(push(q,xrtTypeInt64(),SLOT(input))==XQUEUE_OK);
    assert(push(q,xrtTypeInt64(),SLOT(input))==XQUEUE_FULL);
    status=XL6_CHANNEL_FULL_V1;
    assert(!receive(q,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_FOR_V1,-1,&status));
    assert(xrtGetError() && status==XL6_CHANNEL_FULL_V1 && output==99);xrtClearError();
    assert(!receive(q,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_TRY_V1,0,(XL6_ChannelStatusV1*)&output));xrtClearError();
    assert(xl6WaitQueueCloseV1(q,ID) && xl6WaitQueueCloseV1(q,ID));
    assert(push(q,xrtTypeInt64(),SLOT(input))==XQUEUE_CLOSED);
    for(unsigned i=0;i<2;++i){assert(receive(q,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_WAIT_V1,0,&status));assert(status==XL6_CHANNEL_OK_V1 && output==7);}
    assert(receive(q,xrtTypeInt64(),SLOT(output),XL6_CHANNEL_WAIT_V1,0,&status) && status==XL6_CHANNEL_CLOSED_V1);
    assert(xl6WaitQueueQueryV1(q,ID,XL6_QUEUE_QUERY_DRAINED_V1)==1);xrtValueRelease(q);
    q=create(2,false,2);xstrview text=XRT_STR_LITERAL("a\0你"),taken={0};
    assert(push(q,xl6StringTypeV1(),SLOT(text))==XQUEUE_OK);
    assert(receive(q,xl6StringTypeV1(),SLOT(taken),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1);
    assert(taken.Size==text.Size && !memcmp(taken.Data,text.Data,text.Size));xrtTypeDropValue(xl6StringTypeV1(),&taken);xrtValueRelease(q);
    q=create(3,true,2);xvalue* absent=NULL; xvalue* result=NULL;
    assert(push(q,xrtTypeValue(),SLOT(absent))==XQUEUE_OK);
    assert(receive(q,xrtTypeValue(),SLOT(result),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1 && !result);
    assert(receive(q,xrtTypeValue(),SLOT(result),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_EMPTY_V1 && !result);xrtValueRelease(q);
    q=create(4,false,2);xfuture* future=NULL; xfuture* received=NULL;xpromise* promise=xrtPromiseCreate(&future,NULL);assert(promise && future);
    assert(push(q,xrtTypeFuture(),SLOT(future))==XQUEUE_OK);
    assert(receive(q,xrtTypeFuture(),SLOT(received),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1 && received==future);
    xrtFutureDestroy(received);xrtPromiseDestroy(promise);xrtFutureDestroy(future);xrtValueRelease(q);
    assert(!xl6WaitQueueCreateTakeV1(ID,seed(1,false),0));assert(xrtGetError());xrtClearError();
}
static void graph(void)
{
    xvalue* q=create(3,false,2);
    assert(push(q,xrtTypeValue(),SLOT(q))==XQUEUE_OK);
    xrtownershipref root=xrtValueOwnership(q);xrtValueRelease(q);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY && plan);
    assert(xl6OwnershipPlanFinalize(plan) && xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinish(plan) && xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
}
static void futureGraph(void)
{
    xfuture* future=NULL;xpromise* promise=xrtPromiseCreate(&future,NULL);assert(promise && future);
    xvalue* q=create(4,false,2);xvalue* weak=xrtValueWeakRef(q);assert(weak);
    assert(push(q,xrtTypeFuture(),SLOT(future))==XQUEUE_OK);
    assert(push(q,xrtTypeFuture(),SLOT(future))==XQUEUE_OK);
    assert(xrtPromiseResolveOwnedPolicyV1(promise,xrtValueRetain(q),&payloadPolicy));xrtPromiseDestroy(promise);
    xrtValueRelease(q);xrtFutureDestroy(future);XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtFutureOwnership(future),resolve,(void*)1,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan);
    xrtClearError();assert(!xrtValueWeakRefExpired(weak));assert(xrtFutureRef(future));
    assert(xl6OwnershipPlanTryCreate(xrtFutureOwnership(future),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    xrtFutureDestroy(future);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY && xrtValueWeakRefExpired(weak));
    xrtValueRelease(weak);puts("WaitQueue Future cycle: registered policy, duplicate edges and roots passed");
}
typedef struct Transport {xvalue* q;xatomic32 seen[1024],received,started;bool text;} Transport;
typedef struct Lane {Transport* all;unsigned index;bool producer;} Lane;
static int worker(void* raw)
{
    Lane* lane=raw;Transport* all=lane->all;
    if(lane->producer)for(unsigned i=0;i<256;++i) {
        int64_t value=(int64_t)lane->index*256+i;char bytes[48];int size=snprintf(bytes,sizeof(bytes),"%lld",(long long)value);
        assert(size>0);memcpy(bytes+size,"\0你",4);xstrview text={bytes,(size_t)size+4};
        xqueueresult sent;
        do {sent=all->text?push(all->q,xl6StringTypeV1(),SLOT(text)):push(all->q,xrtTypeInt64(),SLOT(value));
            assert(sent==XQUEUE_OK || sent==XQUEUE_FULL);if(sent==XQUEUE_FULL)xrtThreadYield();}while(sent==XQUEUE_FULL);
    } else {
        (void)xrtAtomic32FetchAdd(&all->started,1,XMEMORY_RELEASE);
        for(;;) {
            int64_t value=0;xstrview text={0};XL6_ChannelStatusV1 status=XL6_CHANNEL_FULL_V1;
            assert(all->text?receive(all->q,xl6StringTypeV1(),SLOT(text),XL6_CHANNEL_FOR_V1,10000,&status):
                receive(all->q,xrtTypeInt64(),SLOT(value),XL6_CHANNEL_FOR_V1,10000,&status));
            if(status==XL6_CHANNEL_CLOSED_V1)break;
            assert(status==XL6_CHANNEL_OK_V1);
            if(all->text){value=strtoll(text.Data,NULL,10);size_t first=strlen(text.Data);
                assert(text.Size==first+4 && !memcmp(text.Data+first,"\0你",4));xrtTypeDropValue(xl6StringTypeV1(),&text);}
            assert(value>=0 && value<1024 && xrtAtomic32FetchAdd(&all->seen[value],1,XMEMORY_ACQ_REL)==0);
            (void)xrtAtomic32FetchAdd(&all->received,1,XMEMORY_RELEASE);
        }
    }
    assert(!xrtGetError());return 0;
}
static void threads(void)
{
    for(unsigned text=0;text<2;++text) {
        Transport all={.q=create(text?2:1,false,2),.text=text!=0};
        xrtAtomic32Init(&all.received,0);xrtAtomic32Init(&all.started,0);
        for(unsigned i=0;i<1024;++i)xrtAtomic32Init(&all.seen[i],0);
        Lane lanes[8];xthread* actors[8];
        for(unsigned i=0;i<4;++i){lanes[i]=(Lane){&all,i,false};actors[i]=xrtThreadCreate(worker,&lanes[i],0);assert(actors[i]);}
        while(xrtAtomic32Load(&all.started,XMEMORY_ACQUIRE)<4)xrtThreadYield();
        for(unsigned i=0;i<4;++i){lanes[i+4]=(Lane){&all,i,true};actors[i+4]=xrtThreadCreate(worker,&lanes[i+4],0);assert(actors[i+4]);}
        for(unsigned i=4;i<8;++i){assert(xrtThreadWaitFor(actors[i],UINT64_C(30000000))==XWAIT_OK);xrtThreadDestroy(actors[i]);}
        assert(xl6WaitQueueCloseV1(all.q,ID));
        for(unsigned i=0;i<4;++i){assert(xrtThreadWaitFor(actors[i],UINT64_C(30000000))==XWAIT_OK);xrtThreadDestroy(actors[i]);}
        assert(xrtAtomic32Load(&all.received,XMEMORY_ACQUIRE)==1024);xrtValueRelease(all.q);
    }
}
static bool failSignal,scriptedWait,failMutexUnit,pauseWait;static unsigned wakes,deadlineCalls;static xdeadline firstDeadline;
static xatomic32 waitEntered,waitLeave;
static xvalue* scriptQueue;
bool xl6WaitProbeMutexUnit(xmutex* mutex)
{return failMutexUnit?false:xrtMutexUnit(mutex);}
bool xl6WaitProbeSignal(const xvalue* cond,uint64_t identity)
{bool ok=xl6CondSignalV1(cond,identity);return failSignal?false:ok;}
xdeadline xl6WaitProbeDeadlineAfter(uint64 timeout)
{xdeadline result=xrtDeadlineAfter(timeout);return scriptedWait?result+deadlineCalls++:result;}
xwaitresult xl6WaitProbeUntil(const xvalue* cond,uint64_t condId,const xvalue* mutex,uint64_t mutexId,xdeadline deadline)
{
    if(pauseWait) {
        xrtAtomic32Store(&waitEntered,1,XMEMORY_RELEASE);
        while(!xrtAtomic32Load(&waitLeave,XMEMORY_ACQUIRE))xrtThreadYield();
        return XWAIT_ERROR;
    }
    if(!scriptedWait)return xl6CondWaitUntilNative(cond,condId,mutex,mutexId,deadline);
    if(!wakes)firstDeadline=deadline;else assert(firstDeadline==deadline);
    ++wakes;if(wakes<3)return XWAIT_OK;
    assert(xl6MutexUnlockV1(mutex,mutexId));int64_t item=19;assert(push(scriptQueue,xrtTypeInt64(),SLOT(item))==XQUEUE_OK);
    assert(xl6MutexLockV1(mutex,mutexId));return XWAIT_TIMEOUT;
}
static int pinnedWaiter(void* raw)
{
    int64_t taken=23;XL6_ChannelStatusV1 status=XL6_CHANNEL_FULL_V1;
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    assert(!receive(raw,xrtTypeInt64(),SLOT(taken),XL6_CHANNEL_FOR_V1,1000,&status));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(!outcome.ok && !outcome.error && !xrtGetError() && taken==23 && status==XL6_CHANNEL_FULL_V1);
    return 0;
}
static void logical(void)
{
    xvalue* q=create(1,false,2);int64_t item=17,taken=0;XL6_ChannelStatusV1 status;
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));failSignal=true;
    assert(push(q,xrtTypeInt64(),SLOT(item))==XQUEUE_ERROR);failSignal=false;
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);assert(!outcome.ok && !outcome.error && !xrtGetError());
    assert(receive(q,xrtTypeInt64(),SLOT(taken),XL6_CHANNEL_TRY_V1,0,&status) && status==XL6_CHANNEL_OK_V1 && taken==17);
    scriptQueue=q;scriptedWait=true;wakes=0;deadlineCalls=0;
    assert(receive(q,xrtTypeInt64(),SLOT(taken),XL6_CHANNEL_FOR_V1,1000,&status));
    assert(wakes==3 && deadlineCalls==1 && status==XL6_CHANNEL_OK_V1 && taken==19);
    scriptedWait=false;scriptQueue=NULL;xrtValueRelease(q);
    assert(!xl6SyncRetirementSnapshotV1(NULL,0));
    q=create(1,false,2);assert(xl6ErrorScopeBegin(&scope));failMutexUnit=true;xrtValueRelease(q);failMutexUnit=false;
    outcome=xl6ErrorScopeEnd(&scope);assert(!outcome.ok && !outcome.error && !xrtGetError());
    XL6_SyncRetirementV1 pending;assert(xl6SyncRetirementSnapshotV1(&pending,1)==1);
    assert(xl6SyncRetirementRetryV1(pending.token));assert(!xl6SyncRetirementSnapshotV1(NULL,0));
    q=create(1,false,2);xrtAtomic32Init(&waitEntered,0);xrtAtomic32Init(&waitLeave,0);pauseWait=true;
    xthread* waiter=xrtThreadCreate(pinnedWaiter,q,0);assert(waiter);
    while(!xrtAtomic32Load(&waitEntered,XMEMORY_ACQUIRE))xrtThreadYield();
    xrtValueRelease(q); /* Only the actual operation pin now owns the native state. */
    xrtAtomic32Store(&waitLeave,1,XMEMORY_RELEASE);
    assert(xrtThreadWaitFor(waiter,UINT64_C(30000000))==XWAIT_OK);xrtThreadDestroy(waiter);pauseWait=false;
    puts("WaitQueue logical errors and absolute deadline: passed");
}
static void prefixes(void)
{
    for(unsigned operation=0;operation<2;++operation)for(size_t point=0;;++point) {
        assert(point<2048);xl6ProductMemoryBegin(SIZE_MAX);xvalue* schema=operation?NULL:seed(1,false);
        xvalue* q=operation?create(2,false,2):NULL;xstrview text=XRT_STR_LITERAL("failure\0你");
        assert(xrtMemDebugFailAfter(point));
        if(!operation)q=xl6WaitQueueCreateTakeV1(ID,schema,2);
        else {xqueueresult result=push(q,xl6StringTypeV1(),SLOT(text));assert(result==XQUEUE_OK || result==XQUEUE_ERROR);}
        bool hit=xl6ProductMemoryEnd();
        if(hit){assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);if(!operation)assert(!q);}
        else assert(q && !xrtGetError());
        xrtClearError();xrtValueRelease(q);xl6ProductMemoryVerify();
        if(!hit){printf("WaitQueue operation=%u first-no-hit=%zu balanced\n",operation,point);assert(point>0);break;}
    }
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==2 && !strcmp(argv[1],"logical"))logical();
    else if(argc==2 && !strcmp(argv[1],"prefixes")){xl6ProductMemoryEnd();prefixes();return 0;}
    else {vectors();graph();futureGraph();threads();}
    assert(!xrtGetError());xl6ProductMemoryEnd();xl6ProductMemoryVerify();
    puts("WaitQueue native: MPMC, exact ownership, graph and close drain passed");return 0;
}
