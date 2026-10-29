#define XL6_TCP_DIAL_INTERRUPT_TEST_ENTRY asyncControlRegressionMain
#include "tcp_dial_interrupt_runtime.c"
#undef XL6_TCP_DIAL_INTERRUPT_TEST_ENTRY
#include "src/runtime/xl6_tcp_dial_sync_internal.h"
#include "src/runtime/xl6_execution_tcp_dial.h"
static unsigned syncPrefixes,syncFailures,syncLive,syncUndelivered,syncGuards;
typedef struct NativeGate {xatomic32 entered,open,returned;} NativeGate;
static void native_gate(xnetworker* worker,void* data)
{
    NativeGate* gate=data;CHECK(worker);xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE);
    xdeadline until=xrtDeadlineAfter(UINT64_C(10000000));
    while(!xrtAtomic32Load(&gate->open,XMEMORY_ACQUIRE)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
    xrtAtomic32Store(&gate->returned,1,XMEMORY_RELEASE);
}
static void native_idle(void* engine)
{
    XL6_ExecutionScopeV1 scope={0};CHECK(xl6ExecutionResourceEnterV1(engine,true,&scope));
    xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    for(;;){xrtownershipscope frozen={0};if(xrtOwnershipFreezeTryBegin(&frozen)){
        bool busy=xrtNetEngineOwnershipQuiescenceV1Get()->Busy(scope.engine);CHECK(xrtOwnershipScopeEnd(&frozen));if(!busy)break;}
        CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
    xl6ExecutionResourceLeaveV1(&scope);
}
typedef struct Finish {XL6_ExecutionResourceV1* carrier;xfuture* source;xcancel* parent;uint64_t due;xatomic32 entered,returned;xl6_error_result result;} Finish;
static int32 finish_thread(void* data)
{
    Finish* test=data;xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));
    xrtAtomic32Store(&test->entered,1,XMEMORY_RELEASE);
    CHECK(!xl6TcpDialFinishUntilInternalV1(test->carrier,test->source,test->due,test->parent));
    test->result=xl6ErrorScopeEnd(&scope);xrtAtomic32Store(&test->returned,1,XMEMORY_RELEASE);return 0;
}
static void live_stop(XL6_ExecutionServicesV1 services,void* engine,const xnetaddrlist* list,bool timeout)
{
    NativeGate gate={0};XL6_ExecutionScopeV1 scope={0};CHECK(xl6ExecutionResourceEnterV1(engine,true,&scope));
    CHECK(xrtNetEnginePost(scope.engine,0,native_gate,&gate));xl6ExecutionResourceLeaveV1(&scope);
    xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    while(!xrtAtomic32Load(&gate.entered,XMEMORY_ACQUIRE)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
    Finish test={.carrier=xl6ExecutionTcpPrepareDialV1(services.source,engine,NULL),.parent=xrtCancelCreate(),
        .due=timeout?xrtDeadlineAfter(UINT64_C(100000)):XRT_DEADLINE_NEVER};CHECK(test.carrier&&test.parent);
    test.source=xl6ExecutionTcpDialAddressesSourceV1(test.carrier,list,NULL);CHECK(test.source&&xrtFutureState(test.source)==XFUTURE_PENDING);
    xfuture* observed=xrtFutureRef(test.source);CHECK(observed);xcancel* requested=xrtFutureCancelToken(observed);CHECK(requested);
    xthread* thread=xrtThreadCreate(finish_thread,&test,0);CHECK(thread);
    while(!xrtAtomic32Load(&test.entered,XMEMORY_ACQUIRE)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
    if(!timeout)CHECK(xrtCancelRequest(test.parent));
    while(!xrtCancelRequested(requested)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
    /* Real operation accepted and cancel requested, but its own Worker is
     * still gated. No substitute Future or approximate sleep proves this. */
    CHECK(xrtFutureState(observed)==XFUTURE_PENDING);
    xdeadline grace=xrtDeadlineAfter(UINT64_C(20000));
    while(!xrtDeadlineExpired(grace)){CHECK(!xrtAtomic32Load(&test.returned,XMEMORY_ACQUIRE));xrtThreadYield();}
    CHECK(xrtFutureState(observed)==XFUTURE_PENDING);
    xrtAtomic32Store(&gate.open,1,XMEMORY_RELEASE);
    CHECK(xrtThreadWaitFor(thread,UINT64_C(5000000))==XWAIT_OK&&xrtThreadExitCode(thread)==0);xrtThreadDestroy(thread);
    CHECK(!test.result.ok&&xrtErrorKind(test.result.error)==(timeout?XERR_TIMEOUT:XERR_CANCELLED));
    CHECK(xrtFutureState(observed)!=XFUTURE_PENDING);xrtErrorFree(test.result.error);
    xrtCancelDestroy(test.parent);xrtCancelDestroy(requested);xrtFutureDestroy(observed);
    native_idle(engine);CHECK(xrtAtomic32Load(&gate.returned,XMEMORY_ACQUIRE));++syncLive;
}
typedef struct SyncGuard {XL6_ExecutionServicesV1 services;void* engine;const xnetaddrlist* list;xcancel* parent;xatomic32 done;} SyncGuard;
static void guard_run(xnetworker* worker,void* data)
{
    SyncGuard* test=data;CHECK(worker);
    for(unsigned mode=0;mode<3;++mode){xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));
        void* result=mode==0?xl6TcpDialAddressesCancelDefaultV1(test->services,test->engine,test->list,test->parent):
            mode==1?xl6TcpDialAddressesUntilDefaultV1(test->services,test->engine,test->list,0):
            xl6TcpDialAddressesUntilCancelDefaultV1(test->services,test->engine,test->list,0,test->parent);
        CHECK(!result);xl6_error_result error=xl6ErrorScopeEnd(&scope);CHECK(!error.ok&&xrtErrorKind(error.error)==XERR_STATE);xrtErrorFree(error.error);}
    xrtAtomic32Store(&test->done,1,XMEMORY_RELEASE);
}
static void sync_run(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();CHECK(source);
    XL6_ExecutionServicesV1 services={source};xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;
    void* engine=xl6ExecutionNetEngineCreateV1(source,&config);CHECK(engine&&xl6ExecutionNetEngineStartV1(engine));
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
    void* listener=xl6TcpListenerCreateV1(services,engine,&listen);CHECK(listener);xnetaddr* address=xl6TcpListenerLocalV1(listener);CHECK(address);
    xnetaddrlist* list=xrtNetAddrListCreate(address,1);CHECK(list);xl6NetAddressDestroyV1(address);
    void* options=xl6TcpDialOptionsCreateV1();xcancel* parent=xrtCancelCreate();CHECK(options&&parent);
    for(unsigned mode=0;mode<6;++mode){uint64_t due=xrtDeadlineAfter(UINT64_C(5000000));void* stream=NULL;
        switch(mode){
        case 0:stream=xl6TcpDialAddressesCancelDefaultV1(services,engine,list,parent);break;
        case 1:stream=xl6TcpDialAddressesCancelV1(services,engine,list,options,parent);break;
        case 2:stream=xl6TcpDialAddressesUntilDefaultV1(services,engine,list,due);break;
        case 3:stream=xl6TcpDialAddressesUntilV1(services,engine,list,options,due);break;
        case 4:stream=xl6TcpDialAddressesUntilCancelDefaultV1(services,engine,list,due,parent);break;
        default:stream=xl6TcpDialAddressesUntilCancelV1(services,engine,list,options,due,parent);break;}
        CHECK(stream);void* peer=xl6TcpListenerAcceptV1(services,listener,5000);CHECK(peer);
        CHECK(xl6TcpStreamSendTextV1(stream,(xstrview){"s\0x",3},-1)==XNET_RESULT_OK);
        receiveExact((XL6_RuntimeServicesV1){0},peer,NULL,"s\0x",3,false);xl6TcpStreamDestroyV1(stream);xl6TcpStreamDestroyV1(peer);
    }
    for(unsigned mode=0;mode<2;++mode){XL6_ExecutionResourceV1* carrier=xl6ExecutionTcpPrepareDialV1(source,engine,NULL);CHECK(carrier);
        xfuture* pending=xl6ExecutionTcpDialAddressesSourceV1(carrier,list,NULL);CHECK(pending&&xrtFutureWaitFor(pending,UINT64_C(5000000))==XWAIT_OK);
        xnetstream* observer=xrtNetStreamRef(xrtFutureValue(pending));CHECK(observer);void* peer=xl6TcpListenerAcceptV1(services,listener,5000);CHECK(peer);
        xcancel* stopped=xrtCancelCreate();CHECK(stopped&&xrtCancelRequest(stopped));xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));
        CHECK(!xl6TcpDialFinishUntilInternalV1(carrier,pending,mode?0:XRT_DEADLINE_NEVER,mode?NULL:stopped));
        xl6_error_result error=xl6ErrorScopeEnd(&scope);CHECK(!error.ok&&xrtErrorKind(error.error)==(mode?XERR_TIMEOUT:XERR_CANCELLED));xrtErrorFree(error.error);
        xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));while(xrtNetStreamState(observer)!=XNET_STREAM_CLOSED){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
        xrtNetStreamDestroy(observer);xrtCancelDestroy(stopped);xl6TcpStreamDestroyV1(peer);++syncUndelivered;
    }
    live_stop(services,engine,list,false);live_stop(services,engine,list,true);
    for(unsigned budget=0;budget<64;++budget){xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));CHECK(xrtMemDebugFailAfter(budget));++syncPrefixes;
        void* stream=xl6TcpDialAddressesUntilCancelV1(services,engine,list,options,xrtDeadlineAfter(UINT64_C(5000000)),parent);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xl6_error_result result=xl6ErrorScopeEnd(&scope);
        if(!stream){CHECK(hit&&!result.ok&&xrtErrorKind(result.error)==XERR_MEMORY);++syncFailures;}else{CHECK(result.ok);xl6TcpStreamDestroyV1(stream);}
        xrtErrorFree(result.error);
    }
    CHECK(xrtCancelRequest(parent));SyncGuard test={.services=services,.engine=engine,.list=list,.parent=parent};
    XL6_ExecutionScopeV1 scope={0};CHECK(xl6ExecutionResourceEnterV1(engine,true,&scope));CHECK(xrtNetEnginePost(scope.engine,0,guard_run,&test));xl6ExecutionResourceLeaveV1(&scope);
    xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));while(!xrtAtomic32Load(&test.done,XMEMORY_ACQUIRE)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
    native_idle(engine);syncGuards+=3;xrtCancelDestroy(parent);xl6TcpDialOptionsDestroyV1(options);xrtNetAddrListDestroy(list);
    xl6TcpListenerDestroyV1(listener);xl6ExecutionResourceReleaseV1(engine);CHECK(xl6ExecutionSourceCloseV1(source));until=xrtDeadlineAfter(UINT64_C(5000000));
    for(;;){XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(&source);CHECK(result.ok);if(result.ready)break;
        CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}balanced(&before);
}
int main(int argc,char** argv)
{
    CHECK(asyncControlRegressionMain(argc,argv)==0);CHECK(xrtMemDebugEnable(true));xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
        #ifdef _WIN32
        backend=XNET_PORT_IOCP;
        #else
        backend=XNET_PORT_EPOLL;
        #endif
    }
    sync_run(backend);CHECK(syncPrefixes==64&&syncFailures>0&&syncLive==2&&syncUndelivered==2&&syncGuards==3&&xrtMemDebugReset());
    printf("sync Dial controls: prefixes=%u failures=%u real-pending-joins=%u undelivered=%u worker-guards=%u; checked first error and balanced\n",
        syncPrefixes,syncFailures,syncLive,syncUndelivered,syncGuards);return 0;
}
