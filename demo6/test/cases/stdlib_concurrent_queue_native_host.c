#include "src/runtime/xl6_concurrent_queue.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct Family {
    xvalue* (*create)(uint64_t,xvalue*,int64_t);
    xqueueresult (*push)(const xvalue*,uint64_t,uint64_t,size_t,const xrttype*,const void*);
    xqueueresult (*pop)(const xvalue*,uint64_t,uint64_t,size_t,const xrttype*,void*);
    xqueuebatchresult (*pushBatch)(const xvalue*,uint64_t,uint64_t,size_t,const xrttype*,const void*,size_t);
    xqueuebatchresult (*popBatch)(const xvalue*,uint64_t,uint64_t,size_t,const xrttype*,void*,size_t);
    int64_t (*query)(const xvalue*,uint64_t,unsigned);
    bool (*close)(const xvalue*,uint64_t);
    bool (*reset)(const xvalue*,uint64_t);
    const xrtownershipadapterv1* (*adapter)(xrtownershipref,const XL6_SequenceAdmissionOpsV2* const*,size_t);
} Family;
#define FAMILY(Name) {xl6##Name##CreateTakeV1,xl6##Name##PushV1,xl6##Name##PopV1, \
    xl6##Name##PushBatchV1,xl6##Name##PopBatchV1,xl6##Name##QueryV1,xl6##Name##CloseV1,xl6##Name##ResetV1,xl6##Name##OwnershipAdapterV1}
static const Family families[]={FAMILY(SpscQueue),FAMILY(MpscQueue),FAMILY(MpmcQueue)};
#define ID UINT64_C(0x9988)
#define ELEMENT UINT64_C(0x7766)
#define SCALAR_ARGS ELEMENT,sizeof(int64_t),xrtTypeInt64()
static xvalue* schema(unsigned profile)
{
    if (profile==2) return xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,XL6_VALUE_ADMIT_ANY,false,ELEMENT);
    return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,profile==1 ? 2 : 1,
        profile==1 ? sizeof(xstrview) : sizeof(int64_t),profile==1 ? xl6StringTypeV1() : xrtTypeInt64(),
        profile==1 ? XL6_VALUE_ADMIT_STRING : XL6_VALUE_ADMIT_U8,false,ELEMENT);
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref, void* context)
{
    (void)context;
    for (size_t i=0;i<3;++i) {
        const xrtownershipadapterv1* adapter=families[i].adapter(ref,NULL,0);
        if (adapter) return adapter;
    }
    return xrtValueOwnershipAdapterV1(ref);
}
static void payloadDrop(void* payload,void* context) { (void)context; xrtValueRelease(payload); }
static bool payloadTrace(const void* payload,const void* data,xrtownershipvisitor visit,void* context)
{ (void)data; return visit(xrtValueOwnership(payload),context); }
static const xfuturepayloadownershipv1 payloadPolicy={sizeof(payloadPolicy),payloadDrop,payloadTrace};
static const xrtownershipadapterv1* resolveFuture(xrtownershipref ref,void* context)
{
    const XL6_SequenceAdmissionOpsV2* policies[]={xl6ArrayFutureAdmissionOpsV2Get()};
    const xfuturepayloadownershipv1* payloads[]={&payloadPolicy};
    const xrtownershipadapterv1* adapter=NULL;
    for (size_t i=0;i<3 && !adapter;++i) adapter=families[i].adapter(ref,context ? NULL : policies,context ? 0 : 1);
    if (!adapter) adapter=xrtFutureOwnershipAdapterV1(ref,payloads,1);
    if (!adapter) adapter=xrtCancelOwnershipAdapterV1(ref);
    if (!adapter) adapter=xrtErrorOwnershipAdapterV1(ref);
    if (!adapter) adapter=xrtValueOwnershipAdapterV1(ref);
    return adapter;
}
static void futureGraphs(void)
{
    for (unsigned i=0;i<3;++i) {
        const Family* f=&families[i];
        xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise && future);
        xvalue* seed=xl6SequenceFutureCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,ELEMENT,false);
        xvalue* q=f->create(ID,seed,2); assert(q);
        assert(f->push(q,ID,ELEMENT,sizeof(future),xrtTypeFuture(),&future)==XQUEUE_OK);
        assert(f->push(q,ID,ELEMENT,sizeof(future),xrtTypeFuture(),&future)==XQUEUE_OK);
        xvalue* weak=xrtValueWeakRef(q); assert(weak);
        assert(xrtPromiseResolveOwnedPolicyV1(promise,xrtValueRetain(q),&payloadPolicy)); xrtPromiseDestroy(promise);
        xrtValueRelease(q); xrtFutureDestroy(future);
        XL6_OwnershipPlan* plan=NULL;
        assert(xl6OwnershipPlanTryCreate(xrtFutureOwnership(future),resolveFuture,(void*)1,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan);
        xrtClearError(); assert(!xrtValueWeakRefExpired(weak));
        assert(xrtFutureRef(future));
        assert(xl6OwnershipPlanTryCreate(xrtFutureOwnership(future),resolveFuture,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
        xrtFutureDestroy(future);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY && xrtValueWeakRefExpired(weak));
        xrtValueRelease(weak);
    }
}
static void vectors(void)
{
    for (size_t i=0;i<3;++i) {
        const Family* f=&families[i];
        assert(!f->create(ID,schema(0),0)); xrtClearError();
        assert(!f->create(ID,schema(0),-1)); xrtClearError();
        assert(!f->create(0,schema(0),2)); xrtClearError();
        assert(!f->create(ID,NULL,2)); xrtClearError();
        assert(!f->create(ID,xrtValueInt(1),2)); xrtClearError();
        xvalue* q=f->create(ID,schema(0),2); assert(q);
        int64_t input=17,output=0;
        assert(f->pop(q,ID,SCALAR_ARGS,&output)==XQUEUE_EMPTY && output==0);
        assert(f->push(q,ID,SCALAR_ARGS,&input)==XQUEUE_OK && input==17);
        assert(f->push(q,ID,SCALAR_ARGS,&input)==XQUEUE_OK);
        assert(f->push(q,ID,SCALAR_ARGS,&input)==XQUEUE_FULL && !xrtGetError());
        assert(f->query(q,ID,XL6_QUEUE_QUERY_COUNT_V1)==2);
        input=256; assert(f->push(q,ID,SCALAR_ARGS,&input)==XQUEUE_ERROR); xrtClearError();
        assert(f->query(q,ID,XL6_QUEUE_QUERY_COUNT_V1)==2);
        assert(f->close(q,ID) && f->query(q,ID,XL6_QUEUE_QUERY_CLOSED_V1)==1);
        input=1; assert(f->push(q,ID,SCALAR_ARGS,&input)==XQUEUE_CLOSED);
        assert(f->pop(q,ID,SCALAR_ARGS,&output)==XQUEUE_OK && output==17);
        assert(f->pop(q,ID,SCALAR_ARGS,&output)==XQUEUE_OK);
        assert(f->pop(q,ID,SCALAR_ARGS,&output)==XQUEUE_CLOSED);
        assert(f->query(q,ID,XL6_QUEUE_QUERY_DRAINED_V1)==1 && f->reset(q,ID));
        assert(f->query(q,ID,XL6_QUEUE_QUERY_CLOSED_V1)==0);
        assert(f->pop(q,ID,ELEMENT+1,sizeof(output),xrtTypeInt64(),&output)==XQUEUE_ERROR); xrtClearError();
        assert(families[(i+1)%3].push(q,ID,SCALAR_ARGS,&input)==XQUEUE_ERROR); xrtClearError();
        assert(f->reset(q,ID)); /* Empty, quiescent reset also permits an open queue. */
        assert(f->push(q,ID,SCALAR_ARGS,&input)==XQUEUE_OK);
        assert(!f->reset(q,ID)); xrtClearError(); /* A nonempty queue is unchanged. */
        assert(f->pop(q,ID,SCALAR_ARGS,&output)==XQUEUE_OK && output==input);
        assert(f->push(q,ID,SCALAR_ARGS,(const unsigned char*)&input+1)==XQUEUE_ERROR); xrtClearError();
        int64_t values[]={1,2,256},outputs[3]={0}; xqueuebatchresult batch;
        batch=f->pushBatch(q,ID,SCALAR_ARGS,values,3);
        assert(batch.Result==XQUEUE_ERROR && !batch.Count && f->query(q,ID,XL6_QUEUE_QUERY_COUNT_V1)==0); xrtClearError();
        values[2]=3; assert(xrtMemDebugFailAfter(0));
        batch=f->pushBatch(q,ID,SCALAR_ARGS,values,3); assert(batch.Result==XQUEUE_OK && batch.Count==2);
        batch=f->popBatch(q,ID,SCALAR_ARGS,outputs,3); assert(batch.Result==XQUEUE_OK && batch.Count==2 && outputs[0]==1 && outputs[1]==2 && !outputs[2]);
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        batch=f->pushBatch(q,ID,SCALAR_ARGS,NULL,0); assert(batch.Result==XQUEUE_OK && !batch.Count);
        batch=f->popBatch(q,ID,SCALAR_ARGS,NULL,0); assert(batch.Result==XQUEUE_OK && !batch.Count);
        batch=f->pushBatch(q,ID,SCALAR_ARGS,values,SIZE_MAX); assert(batch.Result==XQUEUE_ERROR && !batch.Count); xrtClearError();
        xrtValueRelease(q);
        q=f->create(ID,schema(1),2); assert(q);
        xstrview text=XRT_STR_LITERAL("a\0你🙂"),copy={0};
        assert(f->push(q,ID,ELEMENT,sizeof(text),xl6StringTypeV1(),&text)==XQUEUE_OK);
        assert(f->pop(q,ID,ELEMENT,sizeof(copy),xl6StringTypeV1(),&copy)==XQUEUE_OK);
        assert(copy.Size==text.Size && copy.Data!=text.Data && !memcmp(copy.Data,text.Data,text.Size));
        xrtTypeDropValue(xl6StringTypeV1(),&copy); xrtValueRelease(q);
        q=f->create(ID,xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,1,
            sizeof(uint32_t),xl6CharTypeV1(),XL6_VALUE_ADMIT_CHAR,false,ELEMENT),1); assert(q);
        uint32_t character=UINT32_C(0x4F60),characterCopy=0;
        assert(f->push(q,ID,ELEMENT,sizeof(character),xl6CharTypeV1(),&character)==XQUEUE_OK);
        assert(f->pop(q,ID,ELEMENT,sizeof(characterCopy),xl6CharTypeV1(),&characterCopy)==XQUEUE_OK && characterCopy==character);
        character=UINT32_C(0xD800);
        assert(f->push(q,ID,ELEMENT,sizeof(character),xl6CharTypeV1(),&character)==XQUEUE_ERROR); xrtClearError(); xrtValueRelease(q);
        q=f->create(ID,xl6SequenceCScalarCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,xrtTypeInt8(),ELEMENT),2); assert(q);
        int8_t narrow=INT8_MIN,narrowCopy=0;
        assert(f->push(q,ID,ELEMENT,sizeof(narrow),xrtTypeInt8(),&narrow)==XQUEUE_OK);
        assert(f->pop(q,ID,ELEMENT,sizeof(narrowCopy),xrtTypeInt8(),&narrowCopy)==XQUEUE_OK && narrowCopy==narrow); xrtValueRelease(q);
        q=f->create(ID,xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,1,
            sizeof(XL6_NullableI64V1),xl6NullableI64TypeV1(),XL6_VALUE_ADMIT_I64,true,ELEMENT+1),2); assert(q);
        XL6_NullableI64V1 nullable={.hasValue=true,.value=INT64_MIN},nullableCopy={0};
        assert(f->push(q,ID,ELEMENT,sizeof(nullable),xl6NullableI64TypeV1(),&nullable)==XQUEUE_OK);
        assert(f->pop(q,ID,ELEMENT,sizeof(nullableCopy),xl6NullableI64TypeV1(),&nullableCopy)==XQUEUE_OK && nullableCopy.hasValue && nullableCopy.value==INT64_MIN);
        nullable.hasValue=false;
        assert(f->push(q,ID,ELEMENT,sizeof(nullable),xl6NullableI64TypeV1(),&nullable)==XQUEUE_OK);
        nullableCopy=(XL6_NullableI64V1){0};
        assert(f->pop(q,ID,ELEMENT,sizeof(nullableCopy),xl6NullableI64TypeV1(),&nullableCopy)==XQUEUE_OK && !nullableCopy.hasValue); xrtValueRelease(q);
        q=f->create(ID,xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,XL6_VALUE_ADMIT_BYTES,false,ELEMENT),2); assert(q);
        xvalue* bytes=xrtValueBytes((xbytesview){(cbytes)text.Data,text.Size}),*bytesCopy=NULL; assert(bytes);
        assert(f->push(q,ID,ELEMENT,sizeof(bytes),xrtTypeValue(),&bytes)==XQUEUE_OK); xrtValueRelease(bytes);
        assert(f->pop(q,ID,ELEMENT,sizeof(bytesCopy),xrtTypeValue(),&bytesCopy)==XQUEUE_OK && bytesCopy==bytes);
        xbytesview byteView={0}; assert(xrtValueGetBytes(bytesCopy,&byteView));
        assert(byteView.Size==text.Size && !memcmp(byteView.Data,text.Data,text.Size)); xrtValueRelease(bytesCopy); xrtValueRelease(q);
        q=f->create(ID,schema(2),2); assert(q);
        assert(f->push(q,ID,ELEMENT,sizeof(q),xrtTypeValue(),&q)==XQUEUE_OK);
        xvalue* weak=xrtValueWeakRef(q); assert(weak); xrtValueRelease(q);
        XL6_OwnershipPlan* plan=NULL;
        xvalue* alias=xrtValueClone(q); assert(alias==q && xrtValueTypeId(alias)==ID);
        assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(q),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY); plan=NULL; xrtValueRelease(alias);
        assert(xrtValueRetain(q));
        assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(q),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
        xrtValueRelease(q);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
        xl6_error_scope pending={0}; assert(xl6ErrorScopeBegin(&pending));
        xl6ErrorScopeCaptureFailure(&pending); /* Already unwinding, no diagnostic allocation. */
        assert(xl6OwnershipPlanFinish(plan)); /* Completion is its own physical result. */
        xl6_error_result preserved=xl6ErrorScopeEnd(&pending); assert(!preserved.ok); xrtErrorFree(preserved.error);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY && xrtValueWeakRefExpired(weak));
        xrtValueRelease(weak);
    }
}
static void fault(unsigned family,unsigned operation,size_t point)
{
    const Family* f=&families[family];
    xvalue* seed=schema(1); assert(seed); xvalue* q=NULL;
    if (operation) { q=f->create(ID,seed,4); assert(q); seed=NULL; }
    assert(xrtMemDebugFailAfter(point));
    xstrview text=XRT_STR_LITERAL("a\0你🙂"),output={0};
    xqueueresult result=XQUEUE_ERROR;
    if (!operation) { q=f->create(ID,seed,2); seed=NULL; }
    xqueuebatchresult batch={XQUEUE_ERROR,0};
    if (operation==1) result=f->push(q,ID,ELEMENT,sizeof(text),xl6StringTypeV1(),&text);
    else if (operation==2) { xstrview items[]={text,text,text}; batch=f->pushBatch(q,ID,ELEMENT,sizeof(text),xl6StringTypeV1(),items,3); result=batch.Result; }
    bool hit=xl6ProductMemoryEnd();
    assert(operation ? ((result==XQUEUE_OK)==!hit) : ((q!=NULL)==!hit));
    xrtClearError();
    if (operation) {
        size_t count=operation==2 ? batch.Count : hit ? 0 : 1;
        assert(f->query(q,ID,XL6_QUEUE_QUERY_COUNT_V1)==(int64_t)count);
        for (size_t i=0;i<count;++i) {
            assert(f->pop(q,ID,ELEMENT,sizeof(output),xl6StringTypeV1(),&output)==XQUEUE_OK);
            assert(output.Size==text.Size && !memcmp(output.Data,text.Data,text.Size));
            xrtTypeDropValue(xl6StringTypeV1(),&output);
        }
        assert(f->pop(q,ID,ELEMENT,sizeof(output),xl6StringTypeV1(),&output)==XQUEUE_EMPTY);
    }
    xrtTypeDropValue(xl6StringTypeV1(),&output); xrtValueRelease(q); xrtValueRelease(seed);
    xl6ProductMemoryVerify(); printf("family=%u operation=%u point=%zu hit=%d complete-prefix\n",family,operation,point,hit);
}
typedef struct Workers {
    const Family* f; xvalue* queue; unsigned producers;
    bool text;
    xatomic32 received; xatomic32 seen[4096];
} Workers;
typedef struct Lane { Workers* all; unsigned index; bool producer; } Lane;
static int32 worker(void* data)
{
    Lane* lane=data; Workers* all=lane->all;
    if (lane->producer) for (unsigned i=0;i<1024;++i) {
        int64_t value=(int64_t)(lane->index*1024+i); xqueueresult result;
        char spelling[32]; int length=snprintf(spelling,sizeof(spelling),"%lld",(long long)value);
        assert(length>0 && (size_t)length+4<sizeof(spelling)); memcpy(spelling+length,"\0你",4);
        xstrview text={spelling,(size_t)length+4};
        do { result=all->text ? all->f->push(all->queue,ID,ELEMENT,sizeof(text),xl6StringTypeV1(),&text) :
                all->f->push(all->queue,ID,SCALAR_ARGS,&value); if (result==XQUEUE_FULL) xrtThreadYield(); } while (result==XQUEUE_FULL);
        assert(result==XQUEUE_OK && !xrtGetError());
    } else while (xrtAtomic32Load(&all->received,XMEMORY_ACQUIRE)<all->producers*1024) {
        int64_t value=0; xstrview text={0};
        xqueueresult result=all->text ? all->f->pop(all->queue,ID,ELEMENT,sizeof(text),xl6StringTypeV1(),&text) :
            all->f->pop(all->queue,ID,SCALAR_ARGS,&value);
        if (result==XQUEUE_EMPTY) { xrtThreadYield(); continue; }
        if (all->text && result==XQUEUE_OK) {
            assert(text.Size>4 && text.Data[text.Size-4]=='\0' && !memcmp(text.Data+text.Size-3,"你",3));
            value=(int64_t)strtoll(text.Data,NULL,10); xrtTypeDropValue(xl6StringTypeV1(),&text);
        }
        assert(result==XQUEUE_OK && value>=0 && (uint64_t)value<all->producers*1024);
        uint32_t expected=0;
        assert(xrtAtomic32CompareExchange(&all->seen[value],&expected,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE));
        (void)xrtAtomic32FetchAdd(&all->received,1,XMEMORY_RELEASE);
    }
    return 0;
}
static void threads(void)
{
    for (unsigned family=0;family<3;++family) for (unsigned text=0;text<2;++text) {
        const Family* f=&families[family];
        xvalue* seed=text ? schema(1) : xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,1,sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,ELEMENT);
        Workers all={.f=f,.queue=f->create(ID,seed,64),.producers=family ? 4 : 1,.text=text!=0}; assert(all.queue);
        xrtAtomic32Init(&all.received,0);
        for (size_t i=0;i<4096;++i) xrtAtomic32Init(&all.seen[i],0);
        unsigned consumers=family==2 ? 4 : 1; Lane lanes[8]; xthread* actors[8]; unsigned count=0;
        for (unsigned i=0;i<consumers;++i) { lanes[count]=(Lane){&all,i,false}; actors[count]=xrtThreadCreate(worker,&lanes[count],0); assert(actors[count]); ++count; }
        for (unsigned i=0;i<all.producers;++i) { lanes[count]=(Lane){&all,i,true}; actors[count]=xrtThreadCreate(worker,&lanes[count],0); assert(actors[count]); ++count; }
        for (unsigned i=0;i<count;++i) { assert(xrtThreadWaitFor(actors[i],UINT64_C(30000000))==XWAIT_OK); xrtThreadDestroy(actors[i]); }
        assert(f->query(all.queue,ID,XL6_QUEUE_QUERY_COUNT_V1)==0 && f->close(all.queue,ID));
        xrtValueRelease(all.queue);
    }
}
static unsigned dropped;
static void failedDrop(void* handle,void* user)
{ assert(handle==(void*)1 && !user); ++dropped; xl6ForeignCallbackFail(); }
static const xvaluehandleops failedDropOps={NULL,failedDrop,NULL,NULL};
static void cleanupFailures(void)
{
    for (unsigned family=0;family<3;++family) for (unsigned fault=0;fault<2;++fault) {
        const Family* f=&families[family];
        xvalue* q=f->create(ID,schema(2),2); assert(q);
        void* payload=(void*)1; xvalue* item=xrtValueHandleTake(&payload,&failedDropOps,NULL); assert(item);
        assert(f->push(q,ID,ELEMENT,sizeof(item),xrtTypeValue(),&item)==XQUEUE_OK); xrtValueRelease(item);
        unsigned before=dropped; xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        if (fault) assert(xrtMemDebugFailAfter(0));
        xrtValueRelease(q); bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(!outcome.ok && dropped==before+1 && hit==(fault!=0));
        xrtErrorFree(outcome.error); assert(!xrtGetError());
    }
}
/* Test-only resident admission. It owns no language/code/value edge. A Slot
 * predicate pauses outside mutation to prove operation pins refuse graph
 * collection and lifecycle transitions without waiting or entering callbacks
 * under a freeze. Production factories have no such testing callback. */
typedef struct Gate { volatile int32_t refs; xatomic32 entered,leave; } Gate;
static bool gateRetain(const void* raw) { return xrtRefRetain(&((Gate*)raw)->refs)>0; }
static void gateDrop(const void* raw) { if (!xrtRefRelease(&((Gate*)raw)->refs)) xrtFree((void*)raw); }
static bool gateSlot(const void* raw,const void* slot)
{
    Gate* gate=(Gate*)raw; uint32_t expected=0;
    if (xrtAtomic32CompareExchange(&gate->entered,&expected,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE))
        while (!xrtAtomic32Load(&gate->leave,XMEMORY_ACQUIRE)) xrtThreadYield();
    return slot!=NULL;
}
static bool gateValue(const void* raw,const xvalue* value) { (void)raw; return xrtValueType(value)==XVALUE_INT; }
static bool gateEquivalent(const void* a,const void* b) { return a==b; }
static bool gateDefault(const void* raw) { (void)raw; return true; }
static const XL6_SequenceAdmissionOpsV2 gateOps={gateRetain,gateDrop,gateSlot,gateValue,gateEquivalent,gateDefault,NULL,NULL};
typedef struct GateLane { const Family* f; xvalue* queue; xqueueresult result; } GateLane;
static int32 gateWorker(void* raw)
{ GateLane* lane=raw; int64_t item=19; lane->result=lane->f->push(lane->queue,ID,SCALAR_ARGS,&item); assert(!xrtGetError()); return 0; }
static void activePins(void)
{
    for (unsigned family=0;family<3;++family) for (unsigned release=0;release<2;++release) {
        const Family* f=&families[family]; Gate* gate=xrtCalloc(1,sizeof(*gate)); assert(gate);
        gate->refs=1; xrtAtomic32Init(&gate->entered,0); xrtAtomic32Init(&gate->leave,0);
        XL6_SequenceAdmissionV3 admission={&gateOps,gate,ELEMENT,1,sizeof(int64_t),xrtTypeInt64(),ELEMENT};
        xvalue* seed=xl6SequenceCreateAdmittedV1(XL6_SEQUENCE_ARRAY_V1,123,&admission); assert(seed);
        xvalue* q=f->create(ID,seed,4),*weak=xrtValueWeakRef(q); assert(q && weak);
        GateLane lane={f,q,XQUEUE_ERROR}; xthread* actor=xrtThreadCreate(gateWorker,&lane,0); assert(actor);
        while (!xrtAtomic32Load(&gate->entered,XMEMORY_ACQUIRE)) xrtThreadYield();
        assert(!f->close(q,ID) && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
        assert(!f->reset(q,ID) && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
        XL6_OwnershipPlan* plan=NULL;
        assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(q),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan); xrtClearError();
        int64_t item=23;
        assert(f->push(q,ID,SCALAR_ARGS,&item)==(family ? XQUEUE_OK : XQUEUE_ERROR)); xrtClearError();
        if (release) { xrtValueRelease(q); assert(xrtValueWeakRefExpired(weak)); }
        xrtAtomic32Store(&gate->leave,1,XMEMORY_RELEASE);
        assert(xrtThreadWaitFor(actor,UINT64_C(30000000))==XWAIT_OK && lane.result==XQUEUE_OK); xrtThreadDestroy(actor);
        if (!release) { assert(f->query(q,ID,XL6_QUEUE_QUERY_COUNT_V1)==(family ? 2 : 1)); xrtValueRelease(q); }
        assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
    }
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if (argc==4) { unsigned family=(unsigned)strtoul(argv[1],NULL,10),operation=(unsigned)strtoul(argv[2],NULL,10);
        assert(family<3 && operation<3); fault(family,operation,(size_t)strtoull(argv[3],NULL,10)); return 0; }
    if (argc==2) { threads(); activePins(); } else { assert(argc==1); vectors(); futureGraphs(); cleanupFailures(); }
    assert(!xrtGetError()); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("typed queue kernel: exact statuses, owned slots, schema, cycles and real concurrent transport balanced"); return 0;
}
