/* Public API only: link against separate XRT translation units, never against
 * an amalgamated implementation or test access to private parent fields. */
#include <xrt.h>
#include <assert.h>
#include <stdio.h>
typedef struct ApiOwner { volatile int32 References; } ApiOwner;
static unsigned drops;
static bool count_owner(const void* data,size_t* count)
{const ApiOwner* p=data;if(p->References<=0)return false;*count=(size_t)p->References;return true;}
static bool trace_owner(const void* data,xrtownershipvisitor visit,ptr context)
{(void)data;(void)visit;(void)context;return true;}
static const xrtownershipops owner_ops={count_owner,trace_owner};
static void drop_owner(ptr data)
{ApiOwner* p=data;if(xrtRefRelease(&p->References)==0){xrtFree(p);++drops;}}
static const xnetcallbackownershipv1 owner_policy={sizeof(owner_policy),drop_owner,&owner_ops};
int main(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    ApiOwner* owner=xrtCalloc(1,sizeof(*owner));assert(owner);owner->References=1;
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.IdleWait=1000000;
    xnetengine* engine=xrtNetEngineCreate(&config);assert(engine&&xrtNetEngineStart(engine));
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);listen.AcceptConcurrency=1;
    assert(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
    xnetlistener* listener=xrtNetListenOwnedV1(engine,&listen,NULL,NULL,NULL,owner,&owner_policy);assert(listener);
    #if defined(XRT_FEATURE_NET_TCP_FUTURE)
        xfuture* future=xrtNetListenerAcceptAsync(listener);assert(future);
    #endif
    xrtownershipref ref=xrtNetListenerOwnership(listener);const xrtownershipadapterv1* adapter=NULL;
    const xrtownershippreparationv1* preparation=NULL;int token=0;xdeadline deadline=xrtDeadlineAfter(10000000);
    for(;;){xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)){
            adapter=xrtNetListenerOwnershipAdapterV1(ref,&preparation);
            if(adapter){assert(adapter->Hold(listener)&&adapter->Claim(listener,&token));}
            assert(xrtOwnershipScopeEnd(&freeze));if(adapter)break;
        }
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
    xrtownershipprepareresult result;
    do{result=preparation->Prepare(listener,&token);assert(result!=XRT_OWNERSHIP_PREPARE_FAILED&&!xrtDeadlineExpired(deadline));
        if(result==XRT_OWNERSHIP_PREPARE_BUSY)xrtThreadYield();
    }while(result==XRT_OWNERSHIP_PREPARE_BUSY);
    for(;;){xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)){
            adapter->Restore(listener,&token);assert(xrtOwnershipScopeEnd(&freeze));break;
        }
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
    adapter->Drop(listener);
    #if defined(XRT_FEATURE_NET_TCP_FUTURE)
        assert(xrtFutureWaitFor(future,5000000)==XWAIT_OK);xrtFutureDestroy(future);
    #endif
    assert(xrtNetListenerState(listener)==XNET_LISTENER_CLOSED&&xrtNetEngineStop(engine)&&xrtNetEngineDestroy(engine));
    xrtNetListenerDestroy(listener);assert(drops==1);xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&before.InvalidFreeCount==after.InvalidFreeCount&&
        before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);
    assert(xrtMemDebugReset());puts("TCP parent modular API: prepare-close restore retained-engine exact-memory=ok");return 0;
}
