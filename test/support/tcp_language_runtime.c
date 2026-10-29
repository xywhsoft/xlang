/* Actual resident public ABI and ordinary own loopback. Not a generated-class
 * substitute: separate language tests must exercise real compiler objects. */
#include "tcp-language-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_execution_net_engine.h"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#C);abort();}}while(0)
static const xstrview key={"native",6};
static XL6_NativeClassFieldContextV1 context;
static unsigned cases,cancellations,refusals,budgets,allocationFailures,mutations;
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    CHECK(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount&&before->OverflowCount==after.OverflowCount&&before->UnderflowCount==after.UnderflowCount);
}
static xvalue* object(void* raw)
{
    xvalue* field=xrtValueHandleTake(&raw,xl6TcpStreamHandlePolicyV1Get()->Ops,NULL);CHECK(field&&!raw);
    xvalue* result=xrtValueObject();CHECK(result&&xrtValueObjectSet(result,key,field));xrtValueRelease(field);return result;
}
static void* resource(xvalue* value)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    CHECK(xrtValueGetHandle(xrtValueObjectGet(value,key),&raw,&ops,&user)&&raw&&!user&&ops==context.policy->Ops);return raw;
}
static xvalue* value(xfuture* future)
{
    xfutureresult result={0};CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK&&xrtFutureResult(future,&result));
    CHECK(result.State==XFUTURE_RESOLVED&&result.Value&&*(xvalue**)result.Value);
    return xrtValueRetain(*(xvalue**)result.Value);
}
static void idle(XL6_ContinuationExecutor* executor)
{
    xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    while(!xl6ContinuationExecutorIsIdle(executor)){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
}
static void receiveExact(XL6_RuntimeServicesV1 services,void* receiver,xfuture* first,
    const void* expected,size_t size,bool asynchronous)
{
    size_t offset=0;xfuture* future=first;
    while(offset<size){
        xvalue* chunk;
        if(asynchronous){
            if(!future)future=xl6TcpStreamRecvAsyncV1(services,receiver,(int64_t)(size-offset));
            CHECK(future);chunk=value(future);xrtFutureDestroy(future);future=NULL;
        }else chunk=xl6TcpStreamRecvV1(receiver,(int64_t)(size-offset),5000);
        xbytesview bytes;CHECK(chunk&&xrtValueGetBytes(chunk,&bytes)&&bytes.Size&&bytes.Size<=size-offset);
        CHECK(!memcmp(bytes.Data,(const uint8_t*)expected+offset,bytes.Size));offset+=bytes.Size;xrtValueRelease(chunk);
    }
}
static void constructorFailures(XL6_ExecutionServicesV1 execution,XL6_RuntimeServicesV1 services,
    void* engine,void* listener,const xnetlistenconfig* config)
{
    void* other=xl6TcpListenerCreateV1(execution,engine,config);CHECK(other);
    xvalue* carrier=object(xl6TcpPrepareAcceptV1(execution,listener));
    xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));
    CHECK(!xl6TcpAcceptStartV1(services,context,other,carrier));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    CHECK(!outcome.ok&&xrtErrorKind(outcome.error)==XERR_STATE);xrtErrorFree(outcome.error);++refusals;
    xl6TcpListenerCloseV1(other);xl6TcpListenerDestroyV1(other);xrtValueRelease(carrier);
    for(unsigned budget=0;budget<24;++budget){
        carrier=object(xl6TcpPrepareAcceptV1(execution,listener));
        CHECK(xl6ErrorScopeBegin(&scope)&&xrtMemDebugFailAfter(budget));
        xfuture* future=xl6TcpAcceptStartV1(services,context,listener,carrier);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        outcome=xl6ErrorScopeEnd(&scope);++budgets;
        if(future){
            CHECK(outcome.ok);CHECK(xrtFutureCancel(future));
            CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(future)==XFUTURE_CANCELLED);
            xrtFutureDestroy(future);
        }else{CHECK(!outcome.ok&&triggered);++allocationFailures;}
        xrtErrorFree(outcome.error);xrtValueRelease(carrier);idle(services.executor);
    }
    xnetlistenerstats* stats=xl6TcpListenerSnapshotV1(listener);CHECK(stats&&!stats->AcceptWaiters);
    xl6TcpListenerStatsDestroyV1(stats);
}
static void run(xnetportbackend backend,bool cancelFirst)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();CHECK(source);
    XL6_ExecutionServicesV1 execution={source};XL6_RuntimeServicesV1 services={xl6ContinuationExecutorCreateFixedV1(16,1),NULL};CHECK(services.executor);
    xnetengineconfig engineConfig;xrtNetEngineConfigInit(&engineConfig);engineConfig.Workers=1;engineConfig.Backend=backend;
    void* engine=xl6ExecutionNetEngineCreateV1(source,&engineConfig);CHECK(engine&&xl6ExecutionNetEngineStartV1(engine));
    xnetlistenconfig config;xrtNetListenConfigInit(&config);config.Stream.ReadMode=XNET_STREAM_READ_PROBE;
    CHECK(xrtNetAddrLoopback(&config.Address,XNET_FAMILY_IPV4,0));
    void* listener=xl6TcpListenerCreateV1(execution,engine,&config);CHECK(listener);
    xnetaddr* address=xl6TcpListenerLocalV1(listener);CHECK(address&&address->Port);
    constructorFailures(execution,services,engine,listener,&config);
    if(cancelFirst){
        xvalue* carrier=object(xl6TcpPrepareAcceptV1(execution,listener));
        xfuture* cancelled=xl6TcpAcceptStartV1(services,context,listener,carrier);CHECK(cancelled);
        xrtValueRelease(carrier);CHECK(xrtFutureCancel(cancelled));
        CHECK(xrtFutureWaitFor(cancelled,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(cancelled)==XFUTURE_CANCELLED);
        idle(services.executor);CHECK(xl6TcpListenerStateV1(listener)==XNET_LISTENER_OPEN);
        xnetlistenerstats* stats=xl6TcpListenerSnapshotV1(listener);CHECK(stats&&!stats->AcceptWaiters);xl6TcpListenerStatsDestroyV1(stats);
        xrtFutureDestroy(cancelled);++cancellations;
    }
    /* Offline ABI tampering is rejected at projection, not promoted as a
     * different native class. This is not malformed network traffic. */
    xvalue* carrier=object(xl6TcpPrepareAcceptV1(execution,listener));
    xfuture* tampered=xl6TcpAcceptStartV1(services,context,listener,carrier);CHECK(tampered);
    xvalue* replacement=xrtValueInt(7);CHECK(replacement&&xrtValueObjectSet(carrier,key,replacement));
    xrtValueRelease(replacement);xrtValueRelease(carrier);
    void* orphan=xl6TcpStreamCreateDefaultV1(execution,engine,address);CHECK(orphan);
    CHECK(xrtFutureWaitFor(tampered,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(tampered)==XFUTURE_FAILED);
    xfutureresult failed={0};CHECK(xrtFutureResult(tampered,&failed)&&xrtErrorKind(failed.Error)==XERR_STATE);
    xrtFutureDestroy(tampered);xl6TcpStreamAbortV1(orphan);xl6TcpStreamDestroyV1(orphan);idle(services.executor);++mutations;
    carrier=object(xl6TcpPrepareAcceptV1(execution,listener));xvalue* sameCarrier=carrier;
    xfuture* accepted=xl6TcpAcceptStartV1(services,context,listener,carrier);CHECK(accepted);xrtValueRelease(carrier);
    xvalue* clientObject=object(xl6TcpStreamCreateDefaultV1(execution,engine,address));void* client=resource(clientObject);
    xfuture* connected=xl6TcpConnectedStartV1(services,context,client,clientObject);CHECK(connected);
    xvalue* connectedObject=value(connected);CHECK(connectedObject==clientObject);xrtValueRelease(connectedObject);xrtFutureDestroy(connected);
    xvalue* peerObject=value(accepted);CHECK(peerObject==sameCarrier);void* peer=resource(peerObject);xrtFutureDestroy(accepted);
    xl6ExecutionResourceReleaseV1(engine); /* only real child parent owners remain */
    CHECK(xl6TcpStreamStateV1(client)==XNET_STREAM_OPEN&&xl6TcpStreamStateV1(peer)==XNET_STREAM_OPEN);
    CHECK(xl6TcpStreamWaitV1(client,XNET_STREAM_WAIT_OPEN,5000)==XNET_RESULT_OK);
    xfuture* read=xl6TcpStreamRecvAsyncV1(services,peer,5);CHECK(read);
    CHECK(xl6TcpStreamSendTextV1(client,(xstrview){"ab\0cd",5},20)==XNET_RESULT_OK);
    receiveExact(services,peer,read,"ab\0cd",5,true);
    const uint8_t raw[]={255,0,128};xvalue* binary=xrtValueBytes((xbytesview){raw,sizeof(raw)});CHECK(binary);
    read=xl6TcpStreamRecvAsyncV1(services,client,3);CHECK(read&&xl6TcpStreamSendBytesV1(peer,binary,-1)==XNET_RESULT_OK);
    receiveExact(services,client,read,raw,sizeof(raw),true);xrtValueRelease(binary);
    CHECK(xl6TcpStreamSendTextV1(client,(xstrview){"sync",4},-1)==XNET_RESULT_OK);
    receiveExact(services,peer,NULL,"sync",4,false);
    xl6TcpStreamPauseReadV1(peer);xnetstreamstats* stats=xl6TcpStreamSnapshotV1(peer);CHECK(stats&&stats->ReadPaused);xl6TcpStreamStatsDestroyV1(stats);
    xl6TcpStreamResumeReadV1(peer);
    xl6TcpStreamShutdownWriteV1(client);read=xl6TcpStreamRecvAsyncV1(services,peer,16);CHECK(read);
    xvalue* bytes=value(read);xbytesview view;CHECK(xrtValueGetBytes(bytes,&view)&&!view.Size);xrtValueRelease(bytes);xrtFutureDestroy(read);
    xl6TcpStreamShutdownWriteV1(peer);read=xl6TcpStreamRecvAsyncV1(services,client,16);CHECK(read);
    bytes=value(read);CHECK(xrtValueGetBytes(bytes,&view)&&!view.Size);xrtValueRelease(bytes);xrtFutureDestroy(read);
    stats=xl6TcpStreamSnapshotV1(client);CHECK(stats&&stats->SentBytes==9);
    xnetlistenerstats* listenerStats=xl6TcpListenerSnapshotV1(listener);CHECK(listenerStats&&listenerStats->Accepted==2);
    idle(services.executor);xrtValueRelease(peerObject);xrtValueRelease(clientObject);xl6TcpListenerDestroyV1(listener);
    CHECK(xl6ExecutionSourceCloseV1(source));xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    for(;;){XL6_ExecutionRetireResultV1 outcome=xl6ExecutionSourceDestroyV1(&source);CHECK(outcome.ok);
        if(outcome.ready)break;
        CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
    xl6ContinuationExecutorClose(services.executor);CHECK(xl6ContinuationExecutorDestroy(services.executor));
    CHECK(xl6TcpStreamStatsSentBytesV1(stats)==9&&xl6TcpListenerStatsAcceptedV1(listenerStats)==2);
    xl6TcpStreamStatsDestroyV1(stats);xl6TcpListenerStatsDestroyV1(listenerStats);xl6NetAddressDestroyV1(address);
    balanced(&before);++cases;
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);CHECK(outcome.ok);xrtErrorFree(outcome.error);
    context=(XL6_NativeClassFieldContextV1){key.Data,key.Size,xl6TcpStreamHandlePolicyV1Get()};
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
        #ifdef _WIN32
        backend=XNET_PORT_IOCP;
        #else
        backend=XNET_PORT_EPOLL;
        #endif
    }
    for(unsigned i=0;i<8;++i)run(backend,(i&1)!=0);
    CHECK(refusals==8&&budgets==192&&allocationFailures>0&&mutations==8);
    CHECK(xrtMemDebugReset());printf("TCP resident language ABI: cases=%u accept-cancellations=%u refusals=%u allocation-budgets=%u failures=%u class-mutations=%u bytes/NUL/clamped-prefix, same-class Future, sync/async, half-close/EOF, parent owners and independent stats; balanced backend=%d\n",cases,cancellations,refusals,budgets,allocationFailures,mutations,(int)backend);return 0;
}
