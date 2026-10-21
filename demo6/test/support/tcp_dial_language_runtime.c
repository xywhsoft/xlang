/* The original real TCP/Accept/Connected suite is part of this regression. */
#define main tcpLanguageRegressionMain
#include "tcp_language_runtime.c"
#undef main
#include "src/runtime/xl6_execution_tcp_internal.h"
typedef struct DialGate {XL6_ContinuationTicket ticket;xatomic32 entered,open,released;} DialGate;
static unsigned dialCases,dialPrefixes,dialFailures,dialMutations,dialCancellations;
typedef struct WorkerGuard {XL6_ExecutionServicesV1 services;void* engine;const xnetaddrlist* addresses;xatomic32 done;} WorkerGuard;
static void workerGuardRun(xnetworker* worker,ptr raw)
{
    WorkerGuard* test=raw;CHECK(worker);xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));
    CHECK(!xl6TcpDialAddressesDefaultV1(test->services,test->engine,test->addresses));
    xl6_error_result result=xl6ErrorScopeEnd(&scope);CHECK(!result.ok&&xrtErrorKind(result.error)==XERR_STATE);
    xrtErrorFree(result.error);xrtAtomic32Store(&test->done,1,XMEMORY_RELEASE);
}
static void workerGuard(XL6_ExecutionServicesV1 services,void* engine,const xnetaddrlist* addresses)
{
    /* A borrowed C callback only for this bounded test, not a language owner
     * or a graph admission policy. All real creators survive its whole tail. */
    WorkerGuard test={.services=services,.engine=engine,.addresses=addresses};xrtAtomic32Init(&test.done,0);
    XL6_ExecutionScopeV1 scope={0};CHECK(xl6ExecutionResourceEnterV1(engine,true,&scope));
    CHECK(xrtNetEnginePost(scope.engine,0,workerGuardRun,&test));
    xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    while(!xrtAtomic32Load(&test.done,XMEMORY_ACQUIRE)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
    /* Completion publication is not physical return. Keep the stack borrow
     * alive until the real Engine's frozen quiescence proves that tail ended. */
    for(;;){xrtownershipscope frozen={0};
        if(xrtOwnershipFreezeTryBegin(&frozen)){
            bool busy=xrtNetEngineOwnershipQuiescenceV1Get()->Busy(scope.engine);
            CHECK(xrtOwnershipScopeEnd(&frozen));if(!busy)break;}
        CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
    xl6ExecutionResourceLeaveV1(&scope);
}
static void gateRun(void* raw)
{
    DialGate* gate=raw;xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE);
    xdeadline until=xrtDeadlineAfter(UINT64_C(10000000));
    while(!xrtAtomic32Load(&gate->open,XMEMORY_ACQUIRE)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
}
static void gateRelease(void* raw){xrtAtomic32Store(&((DialGate*)raw)->released,1,XMEMORY_RELEASE);}
static void gateStart(XL6_ContinuationExecutor* executor,DialGate* gate)
{
    *gate=(DialGate){0};xrtAtomic32Init(&gate->entered,0);xrtAtomic32Init(&gate->open,0);xrtAtomic32Init(&gate->released,0);
    CHECK(xl6ContinuationExecutorReserve(executor,&gate->ticket,gateRun,gateRelease,gate)==XL6_CONTINUATION_ADMITTED);
    xl6ContinuationCommit(&gate->ticket);xl6ContinuationReady(&gate->ticket);xl6ContinuationTransportReleased(&gate->ticket);
    xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    while(!xrtAtomic32Load(&gate->entered,XMEMORY_ACQUIRE)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
}
static void gateOpen(DialGate* gate)
{
    xrtAtomic32Store(&gate->open,1,XMEMORY_RELEASE);xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    while(!xrtAtomic32Load(&gate->released,XMEMORY_ACQUIRE)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
}
static xfuture* pendingSource(void* resource)
{
    XL6_ExecutionScopeV1 scope={0};CHECK(xl6ExecutionResourceControlV1(resource,true,&scope));
    xfuture* source=xrtFutureRef(xrtAtomicPtrLoad(&((XL6_TcpStreamResourceV1*)resource)->pending,XMEMORY_ACQUIRE));
    xl6ExecutionResourceLeaveV1(&scope);return source;
}
static void dialRun(xnetportbackend backend,unsigned mode)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();CHECK(source);
    XL6_ExecutionServicesV1 execution={source};XL6_RuntimeServicesV1 services={xl6ContinuationExecutorCreateFixedV1(16,1),NULL};CHECK(services.executor);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;
    void* engine=xl6ExecutionNetEngineCreateV1(source,&config);CHECK(engine&&xl6ExecutionNetEngineStartV1(engine));
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);listen.Stream.ReadMode=XNET_STREAM_READ_PROBE;
    CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));void* listener=xl6TcpListenerCreateV1(execution,engine,&listen);CHECK(listener);
    xnetaddr* address=xl6TcpListenerLocalV1(listener);CHECK(address);
    xnetaddrlist* list=xrtNetAddrListCreate(address,1);CHECK(list);xl6NetAddressDestroyV1(address);
    workerGuard(execution,engine,list);
    xvalue* carrier=object(xl6TcpPrepareDialAddressesV1(execution,engine));void* raw=resource(carrier);
    DialGate gate;gateStart(services.executor,&gate);
    char* name=xrtMalloc(key.Size);CHECK(name);memcpy(name,key.Data,key.Size);
    XL6_NativeClassFieldContextV1 transient={name,key.Size,context.policy};
    xfuture* future=xl6TcpDialAddressesStartDefaultV1(services,transient,raw,carrier,list);CHECK(future);
    memset(name,'?',key.Size);xrtFree(name); /* no borrowed field-name metadata */
    xfuture* native=pendingSource(raw);CHECK(native&&xrtFutureWaitFor(native,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(native)==XFUTURE_RESOLVED);
    xnetstream* observer=xrtNetStreamRef(xrtFutureValue(native));CHECK(observer&&xrtFutureState(future)==XFUTURE_PENDING);
    void* peer=xl6TcpListenerAcceptV1(execution,listener,5000);CHECK(peer);
    if(mode==1){xvalue* wrong=xrtValueInt(3);CHECK(wrong&&xrtValueObjectSet(carrier,key,wrong));xrtValueRelease(wrong);++dialMutations;}
    if(mode==2){CHECK(xrtFutureCancel(future));++dialCancellations;}
    xvalue* expected=carrier;
    if(mode){xrtValueRelease(carrier);carrier=NULL;}
    gateOpen(&gate);
    if(!mode){
        xvalue* result=value(future);CHECK(result==expected&&resource(result)==raw);
        CHECK(xl6TcpStreamSendTextV1(raw,(xstrview){"a\0b",3},-1)==XNET_RESULT_OK);
        receiveExact(services,peer,NULL,"a\0b",3,false);xrtValueRelease(result);
    }else{
        CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK);
        CHECK(xrtFutureState(future)==(mode==1?XFUTURE_FAILED:XFUTURE_CANCELLED));
    }
    xrtFutureDestroy(future);xrtValueRelease(carrier);idle(services.executor);
    xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    while(xrtNetStreamState(observer)!=XNET_STREAM_CLOSED){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
    xrtFutureDestroy(native);xrtNetStreamDestroy(observer);xl6TcpStreamDestroyV1(peer);
    /* Constructor/capture/publication prefixes: no worker dispatch may hide
     * caller-thread allocation failures. Every actual accepted operation is
     * still retired by its real carrier through the terminal callback. */
    for(unsigned budget=0;budget<32;++budget){
        carrier=object(xl6TcpPrepareDialAddressesV1(execution,engine));raw=resource(carrier);
        xl6_error_scope error={0};CHECK(xl6ErrorScopeBegin(&error));CHECK(xrtMemDebugFailAfter(budget));++dialPrefixes;
        future=xl6TcpDialAddressesStartDefaultV1(services,context,raw,carrier,list);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xl6_error_result outcome=xl6ErrorScopeEnd(&error);
        if(!future){CHECK(triggered&&!outcome.ok);++dialFailures;}
        else{CHECK(outcome.ok);(void)xrtFutureCancel(future);
            CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK);xrtFutureDestroy(future);}
        xrtErrorFree(outcome.error);xrtValueRelease(carrier);idle(services.executor);
    }
    xrtNetAddrListDestroy(list);xl6TcpListenerDestroyV1(listener);xl6ExecutionResourceReleaseV1(engine);
    CHECK(xl6ExecutionSourceCloseV1(source));until=xrtDeadlineAfter(UINT64_C(5000000));
    for(;;){XL6_ExecutionRetireResultV1 outcome=xl6ExecutionSourceDestroyV1(&source);CHECK(outcome.ok);
        if(outcome.ready)break;
        CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
    xl6ContinuationExecutorClose(services.executor);CHECK(xl6ContinuationExecutorDestroy(services.executor));
    balanced(&before);++dialCases;
}
#ifndef XL6_TCP_DIAL_TEST_ENTRY
#define XL6_TCP_DIAL_TEST_ENTRY main
#endif
int XL6_TCP_DIAL_TEST_ENTRY(int argc,char** argv)
{
    CHECK(tcpLanguageRegressionMain(argc,argv)==0);CHECK(xrtMemDebugEnable(true));
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
        #ifdef _WIN32
        backend=XNET_PORT_IOCP;
        #else
        backend=XNET_PORT_EPOLL;
        #endif
    }
    for(unsigned mode=0;mode<3;++mode)dialRun(backend,mode);
    CHECK(dialCases==3&&dialPrefixes==96&&dialFailures>0&&dialMutations==1&&dialCancellations==1&&xrtMemDebugReset());
    printf("Dial class Future: cases=%u prefixes=%u failures=%u field-mutations=%u cancellations=%u; copied metadata, same object, native-success cancellation, balanced backend=%d\n",
        dialCases,dialPrefixes,dialFailures,dialMutations,dialCancellations,(int)backend);return 0;
}
