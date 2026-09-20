/* Public API only, linked against real separate translation units. */
#include <xrt.h>
#include <assert.h>
#include <stdio.h>
typedef struct ApiOwner { volatile int32 References; } ApiOwner;
static unsigned drops,releases;
static bool count_owner(const void* data,size_t* count)
{const ApiOwner* p=data;if(p->References<=0)return false;*count=(size_t)p->References;return true;}
static bool trace_owner(const void* data,xrtownershipvisitor visit,ptr context)
{(void)data;(void)visit;(void)context;return true;}
static const xrtownershipops owner_ops={count_owner,trace_owner};
static void drop_owner(ptr data)
{ApiOwner* p=data;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
 if(xrtRefRelease(&p->References)==0){xrtFree(p);++drops;}assert(xrtOwnershipScopeEnd(&s));}
static void release_owner(ptr data,cbytes bytes,size_t size)
{assert(bytes&&size==4&&bytes[0]=='s');++releases;drop_owner(data);}
static const xnetcallbackownershipv1 owner_policy={sizeof(owner_policy),drop_owner,&owner_ops};
static const xnetbufownershipv1 payload_policy={sizeof(payload_policy),release_owner,&owner_ops};
int main(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    ApiOwner* owner=xrtCalloc(1,sizeof(*owner));assert(owner);owner->References=2;
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.IdleWait=1000000;
    xnetengine* engine=xrtNetEngineCreate(&config);assert(engine&&xrtNetEngineStart(engine));
    xnetaddr local;assert(xrtNetAddrLoopback(&local,XNET_FAMILY_IPV4,0));
    xnetudp* udp=xrtNetUdpOpenOwnedV1(engine,&local,NULL,0,NULL,NULL,NULL,owner,&owner_policy);assert(udp);
    assert(xrtNetUdpLocal(udp,&local)&&xrtNetUdpSendOwnedV1(udp,&local,NULL,"send",4,owner,&payload_policy)==XNET_RESULT_OK);
    #if defined(XRT_FEATURE_NET_UDP_FUTURE)
        xfuture* future=xrtNetUdpWaitAsync(udp,XNET_UDP_WAIT_CLOSE);assert(future);
    #endif
    xrtownershipref ref=xrtNetUdpOwnership(udp);const xrtownershipadapterv1* adapter=NULL;
    const xrtownershippreparationv1* preparation=NULL;int token=0;xdeadline deadline=xrtDeadlineAfter(10000000);
    for(;;){xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)){
            adapter=xrtNetUdpOwnershipAdapterV1(ref,&preparation);
            if(adapter){assert(adapter->Hold(udp)&&adapter->Claim(udp,&token));}
            assert(xrtOwnershipScopeEnd(&freeze));if(adapter)break;
        }
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
    xrtownershipprepareresult result;
    do{result=preparation->Prepare(udp,&token);assert(result!=XRT_OWNERSHIP_PREPARE_FAILED&&!xrtDeadlineExpired(deadline));
        if(result==XRT_OWNERSHIP_PREPARE_BUSY)xrtThreadYield();
    }while(result==XRT_OWNERSHIP_PREPARE_BUSY);
    for(;;){xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)){adapter->Restore(udp,&token);assert(xrtOwnershipScopeEnd(&freeze));break;}
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
    adapter->Drop(udp);
    #if defined(XRT_FEATURE_NET_UDP_FUTURE)
        assert(xrtFutureWaitFor(future,5000000)==XWAIT_OK);xrtFutureDestroy(future);
    #endif
    assert(xrtNetUdpState(udp)==XNET_UDP_CLOSED&&releases==1&&xrtNetEngineStop(engine)&&xrtNetEngineDestroy(engine));
    xrtNetUdpDestroy(udp);assert(drops==1);xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&before.InvalidFreeCount==after.InvalidFreeCount&&
        before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);
    assert(xrtMemDebugReset());puts("UDP parent modular API: prepare-close restore retained-engine exact-memory=ok");return 0;
}
