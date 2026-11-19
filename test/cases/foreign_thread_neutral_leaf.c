#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_foreign_view.h"
#include <assert.h>
#include <stdio.h>

typedef struct Payload {
    int sample;
    xevent* entered;
    xevent* proceed;
    xatomic32 drops;
    xatomic32 failOnce;
} Payload;

static bool destroy(void* raw)
{
    Payload* value=raw;
    if(xrtAtomic32Exchange(&value->failOnce,0,XMEMORY_ACQ_REL))return false;
    assert(xrtAtomic32FetchAdd(&value->drops,1,XMEMORY_ACQ_REL)==0);
    if(value->entered){
        assert(xrtEventSet(value->entered));
        assert(xrtEventWaitFor(value->proceed,10000)==XWAIT_OK);
    }
    return true;
}

static const XL6_ForeignViewDescriptorV1 policy={
    .size=sizeof(policy), .abiTag=XL6_FOREIGN_VIEW_V1_TAG, .family="test.thread_neutral_leaf",
    .watchAlignment=1, .Destroy=destroy, .callAlignment=1,
    .flags=XL6_FOREIGN_VIEW_THREAD_NEUTRAL_LEAF
};

typedef struct Handoff {
    Payload* payload;
    XL6_ForeignView* view;
} Handoff;

static int32 createOnWorker(void* raw)
{
    Handoff* input=raw;
    input->view=xl6ForeignViewCreateRootV1(input->payload,&policy,NULL);
    assert(input->view);
    return 0;
}

static int32 closeOnWorker(void* raw)
{
    XL6_ForeignView* view=raw;
    assert(xl6ForeignViewCloseV1(view));
    assert(xl6ForeignViewReleaseV1(view));
    return 0;
}

static int32 lastRelease(void* raw)
{
    assert(xl6ForeignViewReleaseV1(raw));
    return 0;
}

static xthread* start(int32(*entry)(void*),void* raw)
{
    xthread* worker=xrtThreadCreate(entry,raw,0);
    assert(worker);
    return worker;
}

static void join(xthread* worker)
{
    assert(xrtThreadWaitFor(worker,10000)==XWAIT_OK);
    xrtThreadDestroy(worker);
}

static void closeWithActiveReader(void)
{
    Payload payload={.sample=42};
    Handoff exchange={&payload,NULL};
    join(start(createOnWorker,&exchange));
    XL6_ForeignView* view=exchange.view;
    XL6_ForeignViewCallV1 active={0}, rejected={0};
    assert(xl6ForeignViewCallBeginV1(view,&active,NULL,0));
    assert(((const Payload*)active.raw)->sample==42);
    XL6_ForeignView* alias=xl6ForeignViewRetainV1(view);
    assert(alias);
    join(start(closeOnWorker,alias));
    assert(xrtAtomic32Load(&payload.drops,XMEMORY_ACQUIRE)==0);
    assert(!xl6ForeignViewCallBeginV1(view,&rejected,NULL,0));
    xrtClearError();
    assert(((const Payload*)active.raw)->sample==42);
    assert(xl6ForeignViewCallEndV1(&active));
    assert(xrtAtomic32Load(&payload.drops,XMEMORY_ACQUIRE)==1);
    assert(xl6ForeignViewReleaseV1(view));
}

static void competingRetirement(void)
{
    Payload payload={0};
    payload.entered=xrtEventCreate(false,false);
    payload.proceed=xrtEventCreate(false,false);
    assert(payload.entered && payload.proceed);
    XL6_ForeignView* view=xl6ForeignViewCreateRootV1(&payload,&policy,NULL);
    assert(view);
    XL6_ForeignView* alias=xl6ForeignViewRetainV1(view);
    assert(alias);
    xthread* owner=start(closeOnWorker,alias);
    assert(xrtEventWaitFor(payload.entered,10000)==XWAIT_OK);
    XL6_ForeignView* competitor=xl6ForeignViewRetainV1(view);
    assert(competitor);
    join(start(closeOnWorker,competitor));
    assert(xrtAtomic32Load(&payload.drops,XMEMORY_ACQUIRE)==1);
    assert(xrtEventSet(payload.proceed));
    join(owner);
    assert(xl6ForeignViewReleaseV1(view));
    xrtEventDestroy(payload.proceed);
    xrtEventDestroy(payload.entered);
}

static void retryAndFinalRelease(void)
{
    Payload retry={0};
    xrtAtomic32Store(&retry.failOnce,1,XMEMORY_RELEASE);
    XL6_ForeignView* view=xl6ForeignViewCreateRootV1(&retry,&policy,NULL);
    assert(view && !xl6ForeignViewCloseV1(view));
    assert(xrtAtomic32Load(&retry.drops,XMEMORY_ACQUIRE)==0);
    xrtClearError();
    join(start(lastRelease,view));
    assert(xrtAtomic32Load(&retry.drops,XMEMORY_ACQUIRE)==1);

    Payload fresh={0};
    view=xl6ForeignViewCreateRootV1(&fresh,&policy,NULL);
    assert(view);
    join(start(lastRelease,view));
    assert(xrtAtomic32Load(&fresh.drops,XMEMORY_ACQUIRE)==1);
}

int main(void)
{
    closeWithActiveReader();
    for(int i=0;i<64;++i)competingRetirement();
    retryAndFinalRelease();
    assert(xl6ForeignViewPendingV1(0)==0 && !xrtGetError());
    puts("THREAD_NEUTRAL_LEAF_OK");
    return 0;
}
