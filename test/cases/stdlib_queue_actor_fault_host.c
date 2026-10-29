#include "src/runtime/xl6_concurrent_queue.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_string_type.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define JOIN_(A,B) A##B
#define JOIN(A,B) JOIN_(A,B)
#define Q(Name) JOIN(XL6_TEST_QUEUE,Name)
#define ID UINT64_C(0x9988)
#define ELEMENT UINT64_C(0x7766)
#define ARGS ELEMENT,sizeof(xstrview),xl6StringTypeV1()
static _Thread_local bool armed;
static _Thread_local size_t point,seen;
static _Thread_local unsigned flavor;
#ifndef XL6_QUEUE_PRODUCT_TEST
extern bool __real_xrtTypeCopyValue(const xrttype*,void*,const void*);
bool __wrap_xrtTypeCopyValue(const xrttype* type,void* out,const void* source)
{
    if(!armed || type!=xrtTypeStringView() || seen++!=point)
        return __real_xrtTypeCopyValue(type,out,source);
    if(!flavor)return false; /* A status failure without an allocated diagnostic. */
    bool ok=__real_xrtTypeCopyValue(type,out,source);
    if(!ok)return false;
    if(flavor==2){xrtSetErrorKind(XERR_MEMORY);return true;}
    return false; /* A partially initialized owning destination must be dropped. */
}
#endif
static xstrview input(void){return XRT_STR_LITERAL("a\0你🙂");}
static void check(const xstrview* out)
{
    xstrview source=input();
    assert(out->Data && out->Size==source.Size && !memcmp(out->Data,source.Data,source.Size));
}
static xvalue* create(void)
{
    xvalue* schema=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,123,ELEMENT,2,
        sizeof(xstrview),xl6StringTypeV1(),XL6_VALUE_ADMIT_STRING,false,ELEMENT);
    assert(schema);xvalue* q=Q(CreateTakeV1)(ID,schema,4);assert(q);return q;
}
typedef struct Job {
    xvalue* queue;
    xatomic32* start;
    unsigned operation;
    bool oom,hit;
    size_t at,count;
    unsigned mode;
} Job;
static void waitStart(Job* job)
{while(!xrtAtomic32Load(job->start,XMEMORY_ACQUIRE))xrtThreadYield();}
static int32 faultActor(void* data)
{
    Job* job=data;waitStart(job);
    point=job->at;flavor=job->mode;seen=0;
    if(job->oom)assert(xrtMemDebugFailAfter(point));else armed=true;
    xstrview text=input(),items[]={text,text,text};
    xqueueresult result=XQUEUE_ERROR;XL6_QueueArrayResultV2 output={XQUEUE_ERROR,0,NULL};
    if(job->operation==0){result=Q(PushV1)(job->queue,ID,ARGS,&text);job->count=result==XQUEUE_OK?1:0;}
    else if(job->operation==1){
        xqueuebatchresult batch=Q(PushBatchV1)(job->queue,ID,ARGS,items,3);
        result=batch.Result;job->count=batch.Count;
    }else{output=Q(PopArrayV2)(job->queue,ID,456,3,NULL);result=output.Result;job->count=output.Count;}
    job->hit=job->oom?xrtMemDebugFailTriggered():seen>point;
    armed=false;xrtMemDebugFailClear();
    assert(job->hit?result==XQUEUE_ERROR:result==XQUEUE_OK);
    assert(job->count<=3);
    if(output.Items){
        XL6_SequenceLayoutV1 layout={0};XL6_SequenceAdmissionV3 policy={0};XL6_SequenceRoleV1 role=0;
        assert(xl6SequenceSchemaCopyV1(output.Items,&role,&layout,&policy));xl6SequenceAdmissionDropV3(&policy);
        XL6_Sequence* owner=xl6SequenceBeginRoleV1(output.Items,role,layout.identity,
            layout.elementIdentity,layout.transfer,layout.size,layout.type);assert(owner);
        xtypedarray* storage=xl6SequenceStorageV1(owner);assert(xrtTypedArrayCount(storage)==job->count);
        for(size_t i=0;i<job->count;++i)check(xrtTypedArrayConstGet(storage,i));
        xl6SequenceEndV1(owner);
    }else if(job->operation==2)assert(!job->count);
    /* A diagnostic is optional; keep the checked outcome independent of it. */
    xrtClearError();xrtValueRelease(output.Items);assert(!xrtGetError());return 0;
}
static int32 peerActor(void* data)
{
    Job* job=data;waitStart(job);xstrview text=input();
    for(unsigned i=0;i<256;++i){
        assert(Q(PushV1)(job->queue,ID,ARGS,&text)==XQUEUE_OK);
        xstrview out={0};assert(Q(PopV1)(job->queue,ID,ARGS,&out)==XQUEUE_OK);check(&out);
        xrtTypeDropValue(xl6StringTypeV1(),&out);assert(!xrtGetError() && !xrtMemDebugFailTriggered());
    }
    assert(!armed);return 0;
}
static void drain(xvalue* q,size_t expected)
{
    assert(Q(QueryV1)(q,ID,XL6_QUEUE_QUERY_COUNT_V1)==(int64_t)expected);
    for(size_t i=0;i<expected;++i){xstrview out={0};assert(Q(PopV1)(q,ID,ARGS,&out)==XQUEUE_OK);
        check(&out);xrtTypeDropValue(xl6StringTypeV1(),&out);}
    xstrview empty={0};assert(Q(PopV1)(q,ID,ARGS,&empty)==XQUEUE_EMPTY && !empty.Data);
}
int main(int argc,char** argv)
{
    assert(argc==4);unsigned operation=(unsigned)strtoul(argv[1],NULL,10);assert(operation<3);
    bool oom=!strcmp(argv[2],"oom");unsigned mode=!strcmp(argv[2],"partial")?1:!strcmp(argv[2],"diagnostic")?2:0;
#ifdef XL6_QUEUE_PRODUCT_TEST
    /* A shared library's internal calls cannot be interposed by --wrap. Only
     * genuine thread-local allocator failure is claimed for actual products. */
    assert(oom);
#endif
    assert(oom || mode || !strcmp(argv[2],"copy"));assert(oom || operation<2);
    xl6ProductMemoryBegin(SIZE_MAX);xvalue* q=create(),*peer=create();xstrview text=input();
    if(operation==2)for(unsigned i=0;i<3;++i)assert(Q(PushV1)(q,ID,ARGS,&text)==XQUEUE_OK);
    xatomic32 start;xrtAtomic32Init(&start,0);
    Job job={.queue=q,.start=&start,.operation=operation,.oom=oom,.at=(size_t)strtoull(argv[3],NULL,10),.mode=mode};
    Job other={.queue=peer,.start=&start};
    xthread* actor=xrtThreadCreate(faultActor,&job,0),*observer=xrtThreadCreate(peerActor,&other,0);assert(actor && observer);
    xrtAtomic32Store(&start,1,XMEMORY_RELEASE);
    assert(xrtThreadWaitFor(actor,UINT64_C(30000000))==XWAIT_OK && xrtThreadWaitFor(observer,UINT64_C(30000000))==XWAIT_OK);
    xrtThreadDestroy(actor);xrtThreadDestroy(observer);
    assert(!armed && !xrtGetError() && !xrtMemDebugFailTriggered());
    drain(q,operation==2?3-job.count:job.count);drain(peer,0);
    /* Role handoff after join and retry must work after every failed prefix. */
    assert(Q(PushV1)(q,ID,ARGS,&text)==XQUEUE_OK);drain(q,1);
    assert(Q(CloseV1)(q,ID) && Q(CloseV1)(peer,ID));xrtValueRelease(q);xrtValueRelease(peer);
    assert(!xl6ProductMemoryEnd() && !xrtGetError());xl6ProductMemoryVerify();
    printf("actor operation=%u lane=%s point=%zu hit=%u committed=%zu balanced\n",operation,argv[2],job.at,(unsigned)job.hit,job.count);
    return 0;
}
