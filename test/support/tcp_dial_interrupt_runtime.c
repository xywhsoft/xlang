#define XL6_TCP_DIAL_TEST_ENTRY dialBasicRegressionMain
#include "tcp_dial_language_runtime.c"
#undef XL6_TCP_DIAL_TEST_ENTRY
#include "src/runtime/xl6_deadline_service.h"
static unsigned interruptCases,interruptPrefixes,interruptFailures;
static void interruptRun(xnetportbackend backend,unsigned mode)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();CHECK(source);
    XL6_ExecutionServicesV1 execution={source};
    XL6_RuntimeServicesV1 services={xl6ContinuationExecutorCreateFixedV1(16,1),xl6DeadlineServiceCreateV1(16)};
    CHECK(services.executor&&services.deadline);xcancel* parent=xrtCancelCreate();CHECK(parent);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;
    void* engine=xl6ExecutionNetEngineCreateV1(source,&config);CHECK(engine&&xl6ExecutionNetEngineStartV1(engine));
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
    void* listener=xl6TcpListenerCreateV1(execution,engine,&listen);CHECK(listener);
    xnetaddr* address=xl6TcpListenerLocalV1(listener);CHECK(address);xnetaddrlist* list=xrtNetAddrListCreate(address,1);CHECK(list);
    xl6NetAddressDestroyV1(address);void* options=xl6TcpDialOptionsCreateV1();CHECK(options);
    xvalue* carrier=object(xl6TcpPrepareDialAddressesV1(execution,engine));void* raw=resource(carrier);
    if(mode==1)CHECK(xrtCancelRequest(parent));
    uint64_t due=mode==2?xrtClock():mode==4?xrtDeadlineAfter(UINT64_C(100000)):mode==5?XRT_DEADLINE_NEVER:xrtDeadlineAfter(UINT64_C(5000000));
    DialGate gate;gateStart(services.executor,&gate);
    xfuture* future=mode==1?xl6TcpDialAddressesStartCancelDefaultV1(services,context,raw,carrier,list,parent):
        mode==2||mode==4||mode==5?xl6TcpDialAddressesStartUntilV1(services,context,raw,carrier,list,options,due):
        xl6TcpDialAddressesStartUntilCancelDefaultV1(services,context,raw,carrier,list,due,parent);CHECK(future);
    xfuture* native=pendingSource(raw);xnetstream* observer=NULL;void* peer=NULL;
    if(mode==1||mode==2)CHECK(!native);
    else{
        CHECK(native&&xrtFutureWaitFor(native,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(native)==XFUTURE_RESOLVED);
        observer=xrtNetStreamRef(xrtFutureValue(native));CHECK(observer&&xrtFutureState(future)==XFUTURE_PENDING);
        peer=xl6TcpListenerAcceptV1(execution,listener,5000);CHECK(peer);
    }
    if(mode==3)CHECK(xrtCancelRequest(parent));
    if(mode==4){xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
        while(!xrtDeadlineExpired(due)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}}
    if(mode==6){xvalue* wrong=xrtValueInt(9);CHECK(wrong&&xrtValueObjectSet(carrier,key,wrong));xrtValueRelease(wrong);}
    xvalue* expected=carrier;bool succeeds=mode==0||mode==5;
    if(!succeeds){xrtValueRelease(carrier);carrier=NULL;}
    gateOpen(&gate);CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK);
    if(succeeds){xvalue* result=value(future);CHECK(result==expected&&resource(result)==raw);xrtValueRelease(result);
        CHECK(xl6TcpStreamSendTextV1(raw,(xstrview){"x\0y",3},-1)==XNET_RESULT_OK);
        receiveExact(services,peer,NULL,"x\0y",3,false);}
    else if(mode==1||mode==3)CHECK(xrtFutureState(future)==XFUTURE_CANCELLED);
    else{xfutureresult result={0};CHECK(xrtFutureResult(future,&result)&&result.State==XFUTURE_FAILED);
        CHECK(xrtErrorKind(result.Error)==(mode==6?XERR_STATE:XERR_TIMEOUT));}
    xrtFutureDestroy(future);xrtValueRelease(carrier);idle(services.executor);
    if(observer){xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
        while(xrtNetStreamState(observer)!=XNET_STREAM_CLOSED){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}}
    xrtFutureDestroy(native);xrtNetStreamDestroy(observer);xl6TcpStreamDestroyV1(peer);
    if(mode==0)for(unsigned budget=0;budget<64;++budget){
        carrier=object(xl6TcpPrepareDialAddressesV1(execution,engine));raw=resource(carrier);
        xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));CHECK(xrtMemDebugFailAfter(budget));++interruptPrefixes;
        future=xl6TcpDialAddressesStartUntilCancelV1(services,context,raw,carrier,list,options,xrtDeadlineAfter(UINT64_C(5000000)),parent);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xl6_error_result result=xl6ErrorScopeEnd(&scope);
        if(!future){CHECK(hit&&!result.ok);++interruptFailures;}
        else{CHECK(result.ok);(void)xrtFutureCancel(future);CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK);xrtFutureDestroy(future);}
        xrtErrorFree(result.error);xrtValueRelease(carrier);idle(services.executor);
    }
    xl6TcpDialOptionsDestroyV1(options);xrtCancelDestroy(parent);xrtNetAddrListDestroy(list);
    xl6TcpListenerDestroyV1(listener);xl6ExecutionResourceReleaseV1(engine);CHECK(xl6ExecutionSourceCloseV1(source));
    xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    for(;;){XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(&source);CHECK(result.ok);if(result.ready)break;
        CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
    xl6ContinuationExecutorClose(services.executor);CHECK(xl6ContinuationExecutorDestroy(services.executor));
    xl6DeadlineServiceCloseV1(services.deadline);CHECK(xl6DeadlineServiceDestroyCompletedV1(services.deadline));
    balanced(&before);++interruptCases;
}
#ifndef XL6_TCP_DIAL_INTERRUPT_TEST_ENTRY
#define XL6_TCP_DIAL_INTERRUPT_TEST_ENTRY main
#endif
int XL6_TCP_DIAL_INTERRUPT_TEST_ENTRY(int argc,char** argv)
{
    CHECK(dialBasicRegressionMain(argc,argv)==0);CHECK(xrtMemDebugEnable(true));xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
        #ifdef _WIN32
        backend=XNET_PORT_IOCP;
        #else
        backend=XNET_PORT_EPOLL;
        #endif
    }
    for(unsigned mode=0;mode<7;++mode)interruptRun(backend,mode);
    CHECK(interruptCases==7&&interruptPrefixes==64&&interruptFailures>0&&xrtMemDebugReset());
    printf("Dial interruption: cases=%u prefixes=%u failures=%u; native-success parent/deadline, actual undelivered close, same class, balanced\n",
        interruptCases,interruptPrefixes,interruptFailures);return 0;
}
