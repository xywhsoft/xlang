/* Actual native operation pins, never a synthetic busy flag or a generated
 * resource callback. Admission owns only immutable metadata; it must neither
 * enter a queue lane nor inspect its mutable storage during an active push. */
#include "src/runtime/xl6_concurrent_queue.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>

enum { ID=301,SEED=302,ELEMENT=303 };
typedef struct Family {
    xvalue* (*Create)(uint64_t,xvalue*,int64_t);
    bool (*Admits)(const xvalue*,uint64_t,const XL6_SequenceLayoutV1*,const XL6_SequenceAdmissionV3*);
    xqueueresult (*Push)(const xvalue*,uint64_t,uint64_t,size_t,const xrttype*,const void*);
    bool (*Reset)(const xvalue*,uint64_t);
} Family;
#define FAMILY(Name) {xl6##Name##CreateTakeV1,xl6##Name##AdmitsSchemaV1,xl6##Name##PushV1,xl6##Name##ResetV1}
static const Family families[]={FAMILY(SpscQueue),FAMILY(MpscQueue),FAMILY(MpmcQueue)};

/* Test-only resident leaf metadata. Slot is a scheduling barrier, Equivalent
 * compares a logical discriminator. This does not add a language callback to
 * any production Queue schema or forge the native owner's private provenance. */
typedef struct Gate {
    volatile int32_t refs;
    xatomic32 entered,leave;
    uint64_t discriminator;
    bool pause;
} Gate;
static bool retain(const void* raw) { return xrtRefRetain(&((Gate*)raw)->refs)>0; }
static void drop(const void* raw) { if (!xrtRefRelease(&((Gate*)raw)->refs)) xrtFree((void*)raw); }
static bool slot(const void* raw,const void* item)
{
    Gate* gate=(Gate*)raw;
    if (gate->pause) {
        uint32_t expected=0;
        if (xrtAtomic32CompareExchange(&gate->entered,&expected,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE))
            while (!xrtAtomic32Load(&gate->leave,XMEMORY_ACQUIRE)) xrtThreadYield();
    }
    return item!=NULL;
}
static bool value(const void* raw,const xvalue* item) { (void)raw; return item && xrtValueType(item)==XVALUE_INT; }
static bool equivalent(const void* a,const void* b) { return ((const Gate*)a)->discriminator==((const Gate*)b)->discriminator; }
static bool defaultable(const void* raw) { (void)raw; return true; }
static const XL6_SequenceAdmissionOpsV2 ops={retain,drop,slot,value,equivalent,defaultable,NULL,NULL};
static xvalue* seed(Gate** out,uint64_t discriminator,bool pause)
{
    Gate* gate=xrtCalloc(1,sizeof(*gate)); assert(gate);
    *gate=(Gate){.refs=1,.discriminator=discriminator,.pause=pause};
    xrtAtomic32Init(&gate->entered,0); xrtAtomic32Init(&gate->leave,0);
    XL6_SequenceAdmissionV3 policy={&ops,gate,ELEMENT,1,0,sizeof(int64_t),xrtTypeInt64(),ELEMENT};
    xvalue* result=xl6SequenceCreateAdmittedV1(XL6_SEQUENCE_ARRAY_V1,SEED,&policy);
    assert(result); *out=gate; return result;
}
typedef struct Lane { const Family* family; const xvalue* queue; xqueueresult result; } Lane;
static int32 worker(void* raw)
{
    Lane* lane=raw; int64_t item=42;
    lane->result=lane->family->Push(lane->queue,ID,ELEMENT,sizeof(item),xrtTypeInt64(),&item);
    assert(!xrtGetError()); return 0;
}
static void vectors(void)
{
    for (size_t f=0;f<3;++f) for (unsigned release=0;release<2;++release) {
        Gate *goodGate,*sameGate,*wrongGate;
        xvalue* witness=seed(&goodGate,7,true),*same=seed(&sameGate,7,false),*wrong=seed(&wrongGate,8,false);
        XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 layout={0},ignored={0};
        XL6_SequenceAdmissionV3 expected={0},refused={0};
        assert(xl6SequenceSchemaCopyV1(same,&role,&layout,&expected));
        assert(xl6SequenceSchemaCopyV1(wrong,&role,&ignored,&refused));
        assert(expected.data!=goodGate && refused.data!=goodGate);
        xrtValueRelease(same); xrtValueRelease(wrong);
        xvalue* queue=families[f].Create(ID,witness,4),*weak=xrtValueWeakRef(queue); assert(queue && weak);
        Lane lane={&families[f],queue,XQUEUE_ERROR}; xthread* thread=xrtThreadCreate(worker,&lane,0); assert(thread);
        while (!xrtAtomic32Load(&goodGate->entered,XMEMORY_ACQUIRE)) xrtThreadYield();
        assert(!families[f].Reset(queue,ID) && xrtGetError() && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
        xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError(); assert(xrtMemDebugFailAfter(0));
        assert(families[f].Admits(queue,ID,&layout,&expected));
        assert(!families[f].Admits(queue,ID,&layout,&refused));
        assert(!families[(f+1)%3].Admits(queue,ID,&layout,&expected));
        XL6_SequenceLayoutV1 altered=layout; ++altered.identity;
        assert(!families[f].Admits(queue,ID,&altered,&expected));
        assert(xrtGetError()==prior && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtClearError();
        if (release) { xrtValueRelease(queue); assert(xrtValueWeakRefExpired(weak)); }
        xrtAtomic32Store(&goodGate->leave,1,XMEMORY_RELEASE);
        assert(xrtThreadWaitFor(thread,UINT64_C(30000000))==XWAIT_OK && lane.result==XQUEUE_OK); xrtThreadDestroy(thread);
        if (!release) xrtValueRelease(queue);
        assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
        xl6SequenceAdmissionDropV3(&expected); xl6SequenceAdmissionDropV3(&refused);
    }
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX); vectors();
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("Queue admission BUSY: genuine three families, independent full policies, error/failpoint transparency, last outside shell release balanced");
    return 0;
}
