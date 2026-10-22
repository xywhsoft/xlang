#include "net_task_owned_test_clock.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct Probe {
    volatile int32 references;
    xnetengine* engine;
    xatomic32 calls,drops;
    xatomic32 request,reply,stop;
} Probe;
static size_t budgets,failures,cancelledBeforeAccept,closedRejections,callbackFreezes;
static bool probeCount(const void* data,size_t* count)
{
    const Probe* probe=data;int32 refs=probe->references;
    if(!count||refs<=0)return false;
    *count=(size_t)refs;return true;
}
static bool probeTrace(const void* data,xrtownershipvisitor visit,ptr context)
{const Probe* probe=data;return !probe->engine||visit(xrtNetEngineOwnership(probe->engine),context);}
static const xrtownershipops probeOps={probeCount,probeTrace};
static int32 inspector(ptr data)
{
    Probe* probe=data;
    while(!xrtAtomic32Load(&probe->stop,XMEMORY_ACQUIRE)) {
        uint32 request=xrtAtomic32Load(&probe->request,XMEMORY_ACQUIRE);
        if(request!=xrtAtomic32Load(&probe->reply,XMEMORY_ACQUIRE)) {
            xrtownershipscope scope={0};
            if(xrtOwnershipFreezeTryBegin(&scope)) {
                assert(xrtOwnershipScopeEnd(&scope));
                xrtAtomic32Store(&probe->reply,request,XMEMORY_RELEASE);
            }
        }
        xrtThreadYield();
    }
    return 0;
}
static void checkCallbackFreeze(Probe* probe)
{
    uint32 request=xrtAtomic32FetchAdd(&probe->request,1,XMEMORY_ACQ_REL)+1;
    TestDeadline deadline=testDeadline();
    while(xrtAtomic32Load(&probe->reply,XMEMORY_ACQUIRE)!=request) {
        assert(!testExpired(deadline));xrtThreadYield();
    }
}
static xtaskoutcome probeRun(xnetworker* worker,xcancel* cancel,ptr data,xtaskvalue* result)
{
    Probe* probe=data;assert(worker&&cancel&&result);
    assert(xrtNetWorkerEngine(worker)==probe->engine&&xrtNetEngineCurrent(probe->engine)==worker);
    checkCallbackFreeze(probe);
    xrtAtomic32FetchAdd(&probe->calls,1,XMEMORY_RELEASE);
    return XTASK_SUCCESS; /* empty owned result is legal */
}
static void probeDrop(ptr data,ptr unused)
{
    Probe* probe=data;assert(!unused);
    checkCallbackFreeze(probe);
    xrtAtomic32FetchAdd(&probe->drops,1,XMEMORY_RELEASE);
    assert(xrtRefRelease(&probe->references)==1); /* actual constructor still owns */
}
static void payloadDrop(ptr data,ptr unused){assert(!data&&!unused);}
static bool payloadTrace(const void* data,const void* unused,xrtownershipvisitor visit,ptr context)
{(void)visit;(void)context;return !data&&!unused;}
static const xtasknetdataownershipv1 dataPolicy={sizeof(dataPolicy),probeRun,probeDrop,&probeOps};
static const xfuturepayloadownershipv1 resultPolicy={sizeof(resultPolicy),payloadDrop,payloadTrace};
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void one(unsigned kind,unsigned budget)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    Probe probe;memset(&probe,0,sizeof(probe));probe.references=1;
    xnetengineconfig config;xrtNetEngineConfigInit(&config);
    config.Workers=1;config.NodeCacheBytes=0;config.CommandCapacity=16;config.TimerLimit=8;
    probe.engine=xrtNetEngineCreate(&config);assert(probe.engine);
    if(kind!=3)assert(xrtNetEngineStart(probe.engine));
    xthread* watcher=xrtThreadCreate(inspector,&probe,0);assert(watcher);
    xcancel* parent=kind==2?xrtCancelCreate():NULL;
    if(parent)assert(xrtCancelRequest(parent));
    assert(xrtRefRetain(&probe.references)==2);
    bool injecting=kind<2;
    if(injecting)assert(xrtMemDebugFailAfter(budget));
    xfuture* future=kind==0?xrtTaskNetDataPolicyV1(probe.engine,0,&probe,parent,&dataPolicy,&resultPolicy):
        xrtTaskNetAfterDataPolicyV1(probe.engine,0,&probe,parent,&dataPolicy,&resultPolicy,TEST_DELAY);
    bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(injecting)++budgets;
    if(!future) {
        if(injecting){assert(triggered&&xrtErrorKind(xrtGetError())==XERR_MEMORY);++failures;}
        else {assert(kind==3&&xrtErrorKind(xrtGetError())==XERR_CLOSED);++closedRejections;}
        xrtClearError();
        assert(xrtNetEngineStop(probe.engine));
        assert(xrtAtomic32Load(&probe.calls,XMEMORY_ACQUIRE)==0&&
            xrtAtomic32Load(&probe.drops,XMEMORY_ACQUIRE)==0&&probe.references==2);
        assert(xrtRefRelease(&probe.references)==1); /* rejection consumes none */
    } else {
        assert(!triggered);
        if(kind==1)assert(xrtFutureCancel(future));
        assert(xrtFutureWaitFor(future,TEST_WAIT)==XWAIT_OK);
        assert(xrtFutureState(future)==(kind==0?XFUTURE_RESOLVED:XFUTURE_CANCELLED));
        /* Terminal publication is the semantic Data/code release barrier. */
        assert(xrtAtomic32Load(&probe.drops,XMEMORY_ACQUIRE)==1&&probe.references==1);
        if(kind==2){assert(!xrtAtomic32Load(&probe.calls,XMEMORY_ACQUIRE));++cancelledBeforeAccept;}
        xrtFutureDestroy(future);
    }
    xrtCancelDestroy(parent);
    assert(xrtNetEngineDestroy(probe.engine));probe.engine=NULL;
    xrtAtomic32Store(&probe.stop,1,XMEMORY_RELEASE);
    assert(xrtThreadWait(watcher)==XWAIT_OK);xrtThreadDestroy(watcher);
    callbackFreezes+=xrtAtomic32Load(&probe.reply,XMEMORY_ACQUIRE);
    assert(probe.references==1);balanced(before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for(unsigned kind=0;kind<2;++kind)for(unsigned budget=0;budget<32;++budget)one(kind,budget);
    for(unsigned i=0;i<16;++i){one(2,0);one(3,0);}
    assert(budgets==64&&failures>=10&&cancelledBeforeAccept==16&&closedRejections==16&&callbackFreezes>=16);
    assert(xrtMemDebugReset());
    printf("TaskNet owned lifetime: budgets=%zu failures=%zu precancel=%zu closed=%zu callback-freezes=%zu; exact memory; no network traffic\n",
        budgets,failures,cancelledBeforeAccept,closedRejections,callbackFreezes);
    return 0;
}

