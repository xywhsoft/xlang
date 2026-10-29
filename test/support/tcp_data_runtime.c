/* Reuse the actual TCP carrier/callback baseline, including its OOM cases. */
#define XRT_MODULE_FILE_TEMP
#define main tcpDataBaselineMain
#include "tcp_language_runtime.c"
#include "release/include/xl_clock.h"
#undef main
#include "src/runtime/xl6_fs_file.h"
typedef xfuture* (*ReceiveInto)(XL6_RuntimeServicesV1,XL6_NativeClassFieldContextV1,void*,const xvalue*,int64_t);
static unsigned prefixes,failedPrefixes,deliveries;
static xvalue* targetObject(void* raw,const XL6_NativeHandlePolicyV1* policy)
{
    CHECK(raw);xvalue* field=xrtValueHandleTake(&raw,policy->Ops,NULL);CHECK(field&&!raw);
    xvalue* result=xrtValueObject();CHECK(result&&xrtValueObjectSet(result,key,field));xrtValueRelease(field);return result;
}
static void exercise(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();CHECK(source);
    XL6_ExecutionServicesV1 execution={source};XL6_RuntimeServicesV1 services={xl6ContinuationExecutorCreateFixedV1(16,1),NULL};CHECK(services.executor);
    xnetengineconfig ec;xrtNetEngineConfigInit(&ec);ec.Backend=backend;ec.Workers=1;
    void* engine=xl6ExecutionNetEngineCreateV1(source,&ec);CHECK(engine&&xl6ExecutionNetEngineStartV1(engine));
    xnetlistenconfig lc;xrtNetListenConfigInit(&lc);lc.Stream.ReadMode=XNET_STREAM_READ_PROBE;CHECK(xrtNetAddrLoopback(&lc.Address,XNET_FAMILY_IPV4,0));
    void* listener=xl6TcpListenerCreateV1(execution,engine,&lc);CHECK(listener);
    xnetaddr* address=xl6TcpListenerLocalV1(listener);CHECK(address);
    void* sender=xl6TcpStreamCreateDefaultV1(execution,engine,address);CHECK(sender);
    void* receiver=xl6TcpListenerAcceptV1(execution,listener,5000);CHECK(receiver);
    CHECK(xl6TcpStreamWaitV1(sender,XNET_STREAM_WAIT_OPEN,5000)==XNET_RESULT_OK);
    const XL6_NativeHandlePolicyV1* policies[]={xl6NetBufferHandlePolicyV1Get(),xl6NetLineFramerHandlePolicyV1Get(),xl6NetLengthFramerHandlePolicyV1Get()};
    ReceiveInto methods[]={xl6TcpStreamRecvIntoNetBufferAsyncV1,xl6TcpStreamRecvIntoLineAsyncV1,xl6TcpStreamRecvIntoLengthAsyncV1};
    for(unsigned kind=0;kind<3;++kind){
        void* native=kind==0?xl6NetBufferCreateV1(64):kind==1?
            xl6NetLineFramerCreateTextV1((xstrview){"\n",1},32,false,64,-1):xl6NetLengthFramerCreateV1(0,4,0,4,64,false,64);
        xvalue* output=targetObject(native,policies[kind]);
        XL6_NativeClassFieldContextV1 ctx={key.Data,key.Size,policies[kind]};
        xfuture* future=methods[kind](services,ctx,receiver,output,1);CHECK(future);
        xrtValueRelease(output); /* capture must keep the actual native target */
        CHECK(xl6TcpStreamSendTextV1(sender,(xstrview){"\0",1},-1)==XNET_RESULT_OK);
        CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED);
        CHECK(xrtFutureValue(future)&&*(int64_t*)xrtFutureValue(future)==1);xrtFutureDestroy(future);idle(services.executor);++deliveries;
        for(unsigned budget=0;budget<24;++budget){
            native=kind==0?xl6NetBufferCreateV1(64):kind==1?
                xl6NetLineFramerCreateTextV1((xstrview){"\n",1},32,false,64,-1):xl6NetLengthFramerCreateV1(0,4,0,4,64,false,64);
            output=targetObject(native,policies[kind]);xl6_error_scope error={0};CHECK(xl6ErrorScopeBegin(&error));
            CHECK(xrtMemDebugFailAfter(budget));++prefixes;future=methods[kind](services,ctx,receiver,output,1);
            bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xl6_error_result outcome=xl6ErrorScopeEnd(&error);
            if(!future){CHECK(triggered&&!outcome.ok);++failedPrefixes;}
            else{CHECK(outcome.ok&&xrtFutureCancel(future));
                CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(future)==XFUTURE_CANCELLED);xrtFutureDestroy(future);}
            xrtErrorFree(outcome.error);xrtValueRelease(output);idle(services.executor);
        }
    }
    void* chain=xl6NetBufferCreateV1(64);CHECK(chain);xl6NetBufferAppendTextV1(chain,(xstrview){"a\0b",3},-1);
    CHECK(xl6TcpStreamSendNetBufferV1(sender,chain,99)==XNET_RESULT_OK&&xl6NetBufferLengthV1(chain)==3);
    xl6NetBufferDestroyV1(chain);receiveExact(services,receiver,NULL,"a\0b",3,false);
    xl6TcpStreamAbortV1(sender);xl6TcpStreamAbortV1(receiver);xl6TcpStreamDestroyV1(sender);xl6TcpStreamDestroyV1(receiver);
    xl6TcpListenerDestroyV1(listener);xl6NetAddressDestroyV1(address);xl6ExecutionResourceReleaseV1(engine);
    CHECK(xl6ExecutionSourceCloseV1(source));XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(&source);CHECK(result.ok);if(result.ready)break;
        CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
    xl6ContinuationExecutorClose(services.executor);CHECK(xl6ContinuationExecutorDestroy(services.executor));balanced(&before);
}
int main(int argc,char** argv)
{
    CHECK(tcpDataBaselineMain(argc,argv)==0);CHECK(xrtMemDebugEnable(true));xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
        #ifdef _WIN32
        backend=XNET_PORT_IOCP;
        #else
        backend=XNET_PORT_EPOLL;
        #endif
    }
    exercise(backend);CHECK(prefixes==72&&deliveries==3&&failedPrefixes>0&&failedPrefixes<prefixes);
    CHECK(xrtMemDebugReset());printf("TCP data runtime: prefixes=%u failures=%u detached-targets=%u; queued int64 results, real source cancellation, NUL and balanced\n",prefixes,failedPrefixes,deliveries);return 0;
}
