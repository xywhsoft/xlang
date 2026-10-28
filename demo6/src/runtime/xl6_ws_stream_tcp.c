#include "xl6_ws_stream_internal.h"
#include "xl6_execution_tcp_internal.h"
static xwsstream* build(XL6_WsResource* r,const XL6_WsCreateOptions* o)
{
    XL6_ExecutionScopeV1 scope={0};XL6_TcpStreamResourceV1* source=o->transport;
    if(!xl6ExecutionResourceControlV1(&source->base,true,&scope))return NULL;
    xwsstream* result=NULL;
    if(source->base.family!=xl6ExecutionTcpStreamFamilyV1Get()){xrtSetErrorKind(XERR_TYPE);goto done;}
    xnetstream* tcp=xrtAtomicPtrLoad(&source->native,XMEMORY_ACQUIRE);
    if(!tcp||!xrtNetWorkerIsCurrent(xrtNetStreamWorker(tcp))){xrtSetErrorKind(XERR_STATE);goto done;}
    xwsstreamconfig negotiated;
    if(!xrtWsUpgradeStreamConfig(&negotiated,o->upgrade->role,&o->upgrade->value))goto done;
    xwsstreamconfig config=*o->config;config.Role=negotiated.Role;config.Protocol=negotiated.Protocol;
    config.Deflate=negotiated.Deflate;config.DeflateEnabled=negotiated.DeflateEnabled;
    r->parent=xl6ExecutionResourceRetainV1(source->parent);if(!r->parent)goto done;
    r->worker=xrtNetWorkerIndex(xrtNetStreamWorker(tcp));
    XL6_NetBufferGuardV1 guard={0};if(!xl6NetBufferEnterV1(o->initial,&guard))goto done;
    if(o->initial->faulted)xrtSetErrorKind(XERR_STATE);
    else if(xl6ExecutionResourceNativePinV1(&r->base)){
        result=xrtWsStreamAttachOwnedV1(tcp,&o->initial->native,&config,r,xl6WsStreamDataPolicyV2Get());
        if(result)xrtAtomicPtrStore(&source->native,NULL,XMEMORY_RELEASE); /* creator moved to WS */
        else xl6ExecutionResourceNativeDropV1(&r->base);
    }
    xl6NetBufferLeaveV1(&guard);
done:
    xl6ExecutionResourceLeaveV1(&scope);return result;
}
void xl6WsStreamBindTcpV2(void* receiver,void* transport,void* initial,void* upgrade,void* options)
{if(!xl6WsStreamBindInternalV2(receiver,transport,initial,upgrade,options,build))xl6WsStreamFailV2();}
uint64_t xl6WsStreamTcpWorkerV2(void* transport)
{
    XL6_TcpScopeV1 scope={0};if(!xl6ExecutionTcpEnterInternalV1(transport,XL6_EXECUTION_TCP_STREAM_V1,false,&scope)){xl6WsStreamFailV2();return 0;}
    uint64_t result=xrtNetWorkerIndex(xrtNetStreamWorker(scope.stream));xl6ExecutionTcpLeaveInternalV1(&scope);return result;
}
void* xl6WsStreamTcpEngineV2(void* transport)
{
    XL6_TcpScopeV1 scope={0};if(!xl6ExecutionTcpEnterInternalV1(transport,XL6_EXECUTION_TCP_STREAM_V1,false,&scope)){xl6WsStreamFailV2();return NULL;}
    void* result=xl6ExecutionResourceRetainV1(scope.engineOwner);xl6ExecutionTcpLeaveInternalV1(&scope);if(!result)xl6WsStreamFailV2();return result;
}
