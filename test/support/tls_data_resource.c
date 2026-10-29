/* Actual owned TLS data, fixed certificate and literal loopback only. */
#define main tlsResourceBaselineMain
#include "release/include/xl_clock.h"
#include "tls_execution_resource.c"
#undef main
#include "src/runtime/xl6_tls_data_abi.h"
#include "src/runtime/xl6_net_bytes_abi.h"
#include "src/runtime/xl6_net_buffer_abi.h"
static void data(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};create(&s,backend);
    CHECK(xl6TlsStreamSendBoundV1(s.client,7,5000)>=7);
    xvalue* protocol=xl6TlsStreamProtocolV1(s.client,5000);xbytesview view;
    CHECK(protocol&&xrtValueGetBytes(protocol,&view)&&!view.Size);xrtValueRelease(protocol);
    CHECK(xl6TlsStreamSendTextV1(s.client,(xstrview){"a\0b\n",4},-1,5000)==4);
    CHECK(xl6TlsStreamWaitV1(s.peer,XTLS_STREAM_WAIT_READ,5000));
    xvalue* peek=xl6TlsStreamPeekV1(s.peer,100,5000);
    CHECK(peek&&xrtValueGetBytes(peek,&view)&&view.Size==4&&!memcmp(view.Data,"a\0b\n",4));xrtValueRelease(peek);
    CHECK(xl6TlsStreamConsumeV1(s.peer,1,5000)==1);
    void* owned=xl6TlsStreamRecvOwnedV1(s.peer,3,5000);CHECK(owned&&xl6NetBytesLengthV1(owned)==3);
    void* frame=xl6NetLineFramerCreateTextV1((xstrview){"\n",1},100,false,100,-1);CHECK(frame);
    xl6NetLineFramerAppendRefV1(frame,owned,100,0);CHECK(!xrtGetError());xl6NetBytesDestroyV1(owned);
    void* snapshot=xl6NetLineFramerNextV1(frame);CHECK(snapshot&&xl6NetFrameResultPayloadSizeV1(snapshot)==2);
    xl6NetFrameResultDestroyV1(snapshot);xl6NetLineFramerDestroyV1(frame);
    ordinaryDrop(&s);balanced(before);
}
static void taskGraph(xnetportbackend backend,unsigned order)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};create(&s,backend);
    s.pending=xl6TlsStreamPeekAsyncV1(s.peer,-1);CHECK(s.pending);collect(&s,order);balanced(before);
}
static void allocation(xnetportbackend backend)
{
    unsigned successes=0;
    for(unsigned budget=0;budget<64&&successes<2;budget++){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};create(&s,backend);
        CHECK(xrtMemDebugFailAfter(budget));s.pending=xl6TlsStreamProtocolAsyncV1(s.peer);
        /* Keep the budget armed through actual terminal publication, not
         * merely through submit: the Worker allocates its result separately. */
        if(s.pending)CHECK(xrtFutureWaitFor(s.pending,UINT64_C(10000000))==XWAIT_OK);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();budgets++;
        if(!s.pending){CHECK(hit&&xrtGetError());xrtClearError();failures++;successes=0;}
        else if(hit){
            CHECK(xrtFutureState(s.pending)==XFUTURE_FAILED||xrtFutureState(s.pending)==XFUTURE_RESOLVED);
            if(xrtFutureState(s.pending)==XFUTURE_FAILED)failures++;
            successes=0;
        }else{CHECK(xrtFutureState(s.pending)==XFUTURE_RESOLVED);successes++;}
        ordinaryDrop(&s);balanced(before);
    }CHECK(successes==2&&failures);
}
/* Allocation failure state is THREAD LOCAL. A caller budget cannot test a
 * Worker result. These test-only embedded posts arm/observe on the exact TLS
 * Worker; their stack storage lives until ordinaryDrop joins that Engine. */
typedef struct Fault {xnetpost post;xatomic32 done;unsigned budget;bool ok,hit;} Fault;
static void armFault(xnetworker* worker,void* raw)
{(void)worker;Fault* f=raw;f->ok=xrtMemDebugFailAfter(f->budget);xrtAtomic32Store(&f->done,1,XMEMORY_RELEASE);}
static void clearFault(xnetworker* worker,void* raw)
{(void)worker;Fault* f=raw;f->hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtAtomic32Store(&f->done,1,XMEMORY_RELEASE);}
static void faultPost(Fault* f,xnetworker* worker,xnettaskproc proc)
{
    xrtAtomic32Store(&f->done,0,XMEMORY_RELAXED);CHECK(xrtNetPost(worker,&f->post,proc,f));
    XL6_DeadlineV1 due=xl6DeadlineAfterMillisecondsV1(10000);
    while(!xrtAtomic32Load(&f->done,XMEMORY_ACQUIRE)){CHECK(!xl6DeadlineExpiredV1(due));xrtThreadYield();}
}
static void workerAllocation(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};create(&s,backend);
    XL6_TlsScopeV1 scope={0};CHECK(xl6ExecutionTlsStreamEnterV1(s.peer,false,&scope));
    xnetworker* worker=xrtNetStreamWorker(xrtTlsStreamTransport(scope.stream));CHECK(worker);
    Fault fault={0};CHECK(xrtNetPostInit(&fault.post));
    for(unsigned mode=0;mode<2;mode++){
        unsigned successes=0;
        for(unsigned budget=0;budget<32&&successes<2;budget++){
            fault.budget=budget;faultPost(&fault,worker,armFault);CHECK(fault.ok);
            xfuture* result=mode?xl6TlsStreamProtocolAsyncV1(s.peer):xl6TlsStreamPeekAsyncV1(s.peer,-1);CHECK(result);
            CHECK(xrtFutureWaitFor(result,UINT64_C(10000000))==XWAIT_OK);faultPost(&fault,worker,clearFault);budgets++;
            if(fault.hit){CHECK(xrtFutureState(result)==XFUTURE_FAILED);failures++;successes=0;}
            else{CHECK(xrtFutureState(result)==XFUTURE_RESOLVED);successes++;}
            xrtFutureDestroy(result);
        }CHECK(successes==2);
    }
    xl6ExecutionTlsLeaveV1(&scope);ordinaryDrop(&s);balanced(before);
}
static void checkedFailures(void)
{
    for(unsigned operation=0;operation<4;operation++){
        xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));CHECK(xrtMemDebugFailAfter(0));
        xfuture* result=operation==0?xl6TlsStreamPeekAsyncV1(NULL,-1):operation==1?xl6TlsStreamConsumeAsyncV1(NULL,0):
            operation==2?xl6TlsStreamSendBoundAsyncV1(NULL,0):xl6TlsStreamProtocolAsyncV1(NULL);
        xrtMemDebugFailClear();xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        CHECK(!result&&!outcome.ok);xrtErrorFree(outcome.error);
    }
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));const XL6_OwnershipDomainV1* domains[]={
        #include "tls-resource-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));CHECK(catalog);
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
#ifdef _WIN32
        backend=XNET_PORT_IOCP;
#else
        backend=XNET_PORT_EPOLL;
#endif
    }
    checkedFailures();data(backend);taskGraph(backend,0);taskGraph(backend,1);allocation(backend);workerAllocation(backend);
    xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("TLS data resources: worker operations, owned receives, reference framing, %u task graphs, %u allocation budgets/%u failures; balanced\n",graphs,budgets,failures);return 0;
}
