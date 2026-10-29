#include "xl6_tcp_internal.h"
void* xl6TcpStreamCreateV1(XL6_ExecutionServicesV1 services,void* engine,const void* address,const void* options,int64_t worker)
{
    if(!address||worker<0||(uint64_t)worker>UINT32_MAX){xrtSetErrorKind(address?XERR_RANGE:XERR_ARGUMENT);xl6TcpFailedV1();return NULL;}
    void* result=xl6ExecutionTcpConnectV1(services.source,engine,address,(uint32_t)worker,options);
    if(!result)xl6TcpFailedV1();
    return result;
}
void* xl6TcpStreamCreateDefaultV1(XL6_ExecutionServicesV1 services,void* engine,const void* address)
{return xl6TcpStreamCreateV1(services,engine,address,NULL,0);}
void* xl6TcpListenerCreateV1(XL6_ExecutionServicesV1 services,void* engine,const void* options)
{
    if(!options){xrtSetErrorKind(XERR_ARGUMENT);xl6TcpFailedV1();return NULL;}
    void* result=xl6ExecutionTcpListenV1(services.source,engine,options);if(!result)xl6TcpFailedV1();return result;
}
void xl6TcpStreamDestroyV1(void* raw){xl6ExecutionResourceReleaseV1(raw);}
void xl6TcpListenerDestroyV1(void* raw){xl6ExecutionResourceReleaseV1(raw);}
static void closeResource(void* raw,XL6_ExecutionKindV1 kind,bool cancel)
{
    /* Validate the physical receiver before invoking the shared dispatcher.
     * Its own scope protects the close call after this verification scope. */
    XL6_TcpScopeV1 scope={0};if(!xl6TcpEnterV1(raw,kind,false,&scope))return;
    bool ok=cancel?xl6ExecutionResourceCancelV1(raw):xl6ExecutionResourceCloseV1(raw);
    xl6ExecutionTcpLeaveInternalV1(&scope);if(!ok)xl6TcpFailedV1();
}
void xl6TcpStreamCloseV1(void* raw){closeResource(raw,XL6_EXECUTION_TCP_STREAM_V1,false);}
void xl6TcpStreamAbortV1(void* raw){closeResource(raw,XL6_EXECUTION_TCP_STREAM_V1,true);}
void xl6TcpListenerCloseV1(void* raw){closeResource(raw,XL6_EXECUTION_TCP_LISTENER_V1,false);}
static int64_t state(void* raw,XL6_ExecutionKindV1 kind)
{
    XL6_TcpScopeV1 scope={0};if(!xl6TcpEnterV1(raw,kind,false,&scope))return 0;
    int64_t result=scope.stream?(int64_t)xrtNetStreamState(scope.stream):(int64_t)xrtNetListenerState(scope.listener);
    xl6ExecutionTcpLeaveInternalV1(&scope);return result;
}
int64_t xl6TcpStreamStateV1(void* raw){return state(raw,XL6_EXECUTION_TCP_STREAM_V1);}
int64_t xl6TcpListenerStateV1(void* raw){return state(raw,XL6_EXECUTION_TCP_LISTENER_V1);}
static uint64_t streamSize(void* raw,unsigned field)
{
    XL6_TcpScopeV1 scope={0};if(!xl6TcpEnterV1(raw,XL6_EXECUTION_TCP_STREAM_V1,false,&scope))return 0;
    uint64_t result=0;
    switch(field){
        case 0:result=xrtNetStreamAvailable(scope.stream);break;
        case 1:{xnetstreamstats stats;if(!xrtNetStreamStats(scope.stream,&stats))xl6TcpFailedV1();else result=stats.QueuedBytes;break;}
        case 2:result=xrtNetStreamWriteLimit(scope.stream);break;
        case 3:result=xrtNetStreamWritable(scope.stream);break;
        default:abort();
    }
    xl6ExecutionTcpLeaveInternalV1(&scope);return result;
}
uint64_t xl6TcpStreamAvailableV1(void* raw){return streamSize(raw,0);}
uint64_t xl6TcpStreamPendingSendV1(void* raw){return streamSize(raw,1);}
uint64_t xl6TcpStreamWriteLimitV1(void* raw){return streamSize(raw,2);}
uint64_t xl6TcpStreamWritableV1(void* raw){return streamSize(raw,3);}
static int64_t sendView(void* raw,xbytesview input,int64_t size)
{
    if(!xl6BufferSelect(&input,size)){xl6TcpFailedV1();return XNET_RESULT_ERROR;}
    XL6_TcpScopeV1 scope={0};if(!xl6TcpEnterV1(raw,XL6_EXECUTION_TCP_STREAM_V1,true,&scope))return XNET_RESULT_ERROR;
    xnetresult result=xrtNetStreamSend(scope.stream,input.Data,input.Size);
    xl6ExecutionTcpLeaveInternalV1(&scope);if(result==XNET_RESULT_ERROR)xl6TcpFailedV1();return result;
}
int64_t xl6TcpStreamSendTextV1(void* raw,xstrview input,int64_t size)
{return sendView(raw,(xbytesview){(const uint8_t*)input.Data,input.Size},size);}
int64_t xl6TcpStreamSendBytesV1(void* raw,const xvalue* input,int64_t size)
{xbytesview view;if(!xl6BufferBytesInput(input,size,&view)){xl6TcpFailedV1();return XNET_RESULT_ERROR;}return sendView(raw,view,-1);}
int64_t xl6TcpStreamSendBufferV1(void* raw,void* input,int64_t size)
{xbytesview view;if(!xl6BufferInput(input,size,&view)){xl6TcpFailedV1();return XNET_RESULT_ERROR;}return sendView(raw,view,-1);}
void xl6TcpStreamShutdownWriteV1(void* raw){if(!xl6ExecutionTcpShutdownWriteV1(raw))xl6TcpFailedV1();}
void xl6TcpStreamPauseReadV1(void* raw){if(!xl6ExecutionTcpPauseReadV1(raw,true))xl6TcpFailedV1();}
void xl6TcpStreamResumeReadV1(void* raw){if(!xl6ExecutionTcpPauseReadV1(raw,false))xl6TcpFailedV1();}
static void* address(void* raw,XL6_ExecutionKindV1 kind,bool remote)
{
    XL6_TcpScopeV1 scope={0};if(!xl6TcpEnterV1(raw,kind,false,&scope))return NULL;
    xnetaddr value;bool ok=scope.listener?xrtNetListenerLocal(scope.listener,&value):
        remote?xrtNetStreamRemote(scope.stream,&value):xrtNetStreamLocal(scope.stream,&value);
    xl6ExecutionTcpLeaveInternalV1(&scope);if(!ok){xl6TcpFailedV1();return NULL;}
    return xl6NetAddressCopyV1(&value);
}
void* xl6TcpStreamLocalV1(void* raw){return address(raw,XL6_EXECUTION_TCP_STREAM_V1,false);}
void* xl6TcpStreamRemoteV1(void* raw){return address(raw,XL6_EXECUTION_TCP_STREAM_V1,true);}
void* xl6TcpListenerLocalV1(void* raw){return address(raw,XL6_EXECUTION_TCP_LISTENER_V1,false);}
void* xl6TcpPrepareAcceptV1(XL6_ExecutionServicesV1 services,void* raw)
{void* result=xl6ExecutionTcpPrepareAcceptV1(services.source,raw);if(!result)xl6TcpFailedV1();return result;}
static bool trace(const xvalue* value,const XL6_NativeHandlePolicyV1* policy,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&ops==policy->Ops&&
        visit(xl6ExecutionResourceOwnershipV1(raw),context);
}
static bool traceStream(const xvalue* value,xrtownershipvisitor visit,void* context)
{return trace(value,xl6TcpStreamHandlePolicyV1Get(),visit,context);}
static bool traceListener(const xvalue* value,xrtownershipvisitor visit,void* context)
{return trace(value,xl6TcpListenerHandlePolicyV1Get(),visit,context);}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6TcpStreamHandlePolicyV1Get,xl6TcpStreamDestroyV1,traceStream)
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6TcpListenerHandlePolicyV1Get,xl6TcpListenerDestroyV1,traceListener)
