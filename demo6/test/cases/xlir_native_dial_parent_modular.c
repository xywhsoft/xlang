/* Public declarations only, linked against separate native translation units. */
#include <xrt.h>
#include <assert.h>
#include <stdio.h>
typedef struct ApiCase { xnetstream* Stream; xatomic32 Done,Closed; } ApiCase;
typedef struct ApiOwner { volatile int32 References; ApiCase* Test; } ApiOwner;
static unsigned drops;
static bool count_owner(const void* data,size_t* count)
{const ApiOwner* p=data;if(p->References<=0)return false;*count=(size_t)p->References;return true;}
static bool trace_owner(const void* data,xrtownershipvisitor visit,ptr context)
{(void)data;(void)visit;(void)context;return true;} /* Case is a borrowed stack harness. */
static const xrtownershipops owner_ops={count_owner,trace_owner};
static void drop_owner(ptr data)
{ApiOwner* p=data;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
 if(xrtRefRelease(&p->References)==0){xrtFree(p);++drops;}assert(xrtOwnershipScopeEnd(&s));}
static const xnetcallbackownershipv1 owner_policy={sizeof(owner_policy),drop_owner,&owner_ops};
static void done(xnetdial* dial,xnetresult result,xnetstream* stream,const xerror* error,ptr data)
{(void)dial;ApiOwner* owner=data;assert(result==XNET_RESULT_OK&&stream&&!error);
 owner->Test->Stream=stream;xrtAtomic32Store(&owner->Test->Done,1,XMEMORY_RELEASE);}
static void closed(xnetstream* stream,xnetresult result,const xerror* error,ptr data)
{(void)result;(void)error;ApiOwner* owner=data;assert(stream==owner->Test->Stream&&owner->References==1);
 xrtAtomic32Store(&owner->Test->Closed,1,XMEMORY_RELEASE);}
static void wait_flag(const xatomic32* flag)
{xdeadline d=xrtDeadlineAfter(10000000);while(!xrtAtomic32Load(flag,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(d));xrtThreadYield();}}
int main(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);ApiCase test={0};
    ApiOwner* owner=xrtCalloc(1,sizeof(*owner));assert(owner);owner->References=1;owner->Test=&test;
    xnetsocket server=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_STREAM,XNET_SOCKET_NONBLOCK);assert(server);
    xnetaddr address;assert(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0)&&xrtNetSocketBind(server,&address)&&
        xrtNetSocketLocal(server,&address)&&xrtNetSocketListen(server,4));
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.IdleWait=1000000;
    xnetengine* engine=xrtNetEngineCreate(&config);assert(engine&&xrtNetEngineStart(engine));
    xnetresolver* resolver=xrtNetResolverCreate(NULL);assert(resolver);xnetstreamevents events={0};events.Close=closed;
    xnetdial* dial=xrtNetDialOwnedV1(engine,resolver,"127.0.0.1",address.Port,NULL,&events,owner,done,owner,owner,&owner_policy);assert(dial);
    wait_flag(&test.Done);assert(xrtNetResolverDestroy(resolver));
    xrtownershipref ref=xrtNetDialOwnership(dial);const xrtownershipadapterv1* adapter=NULL;
    const xrtownershippreparationv1* preparation=NULL;int token=0;xdeadline deadline=xrtDeadlineAfter(10000000);
    for(;;){xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)){
            adapter=xrtNetDialOwnershipAdapterV1(ref,&preparation);
            if(adapter){assert(adapter->Hold(dial)&&adapter->Claim(dial,&token)&&preparation->Ready(dial));}
            assert(xrtOwnershipScopeEnd(&freeze));if(adapter)break;
        }assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
    xrtownershipprepareresult result;
    do{result=preparation->Prepare(dial,&token);assert(result!=XRT_OWNERSHIP_PREPARE_FAILED&&!xrtDeadlineExpired(deadline));
        if(result==XRT_OWNERSHIP_PREPARE_BUSY)xrtThreadYield();}while(result==XRT_OWNERSHIP_PREPARE_BUSY);
    for(;;){xrtownershipscope freeze={0};if(xrtOwnershipFreezeTryBegin(&freeze)){
        adapter->Restore(dial,&token);assert(xrtOwnershipScopeEnd(&freeze));break;
    }assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    adapter->Drop(dial);xrtNetDialDestroy(dial);assert(!drops&&owner->References==1);
    assert(xrtNetStreamAbort(test.Stream));wait_flag(&test.Closed);
    assert(xrtNetEngineStop(engine)&&xrtNetEngineDestroy(engine));xrtNetStreamDestroy(test.Stream);assert(drops==1&&xrtNetSocketClose(server));
    xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);
    assert(xrtMemDebugReset());puts("Dial parent modular API: shared-context restore retained-engine exact-memory=ok");return 0;
}
