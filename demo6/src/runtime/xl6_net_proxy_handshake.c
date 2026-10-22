#include "xl6_net_proxy_internal.h"
#include "xl6_native_handle.h"
typedef XL6_NetProxyHandshakeV1 Handshake;
void* xl6NetProxyHandshakeCreateV1(const void* proxy,xstrview host,int64_t port,int64_t receiveLimit,int64_t inputLimit)
{
    if(port<=0||port>UINT16_MAX||receiveLimit<=0||(uint64_t)receiveLimit>SIZE_MAX){
        xrtSetErrorKind(XERR_RANGE);xl6NetBufferFailedV1();return NULL;}
    if(!xl6TextSelect(&host,-1)){xl6NetBufferFailedV1();return NULL;}
    Handshake* result=xrtCalloc(1,sizeof(*result));bool ok=result!=NULL;
    if(ok)ok=xl6NetBufferInitializeV1(&result->input,inputLimit);
    if(ok){xnetproxyhandshakeconfig config;xrtNetProxyHandshakeConfigInit(&config);
        config.Proxy=proxy;config.TargetHost=host;config.TargetPort=(uint16)port;config.ReceiveLimit=(size_t)receiveLimit;
        result->native=xrtNetProxyHandshakeCreate(&config);ok=result->native!=NULL;}
    if(!ok){xrtFree(result);result=NULL;xl6NetBufferFailedV1();}return result;
}
void xl6NetProxyHandshakeDestroyV1(void* raw)
{Handshake* h=raw;if(h){xrtNetProxyHandshakeDestroy(h->native);xrtNetBufClear(&h->input.native);xrtFree(h);}}
bool xl6NetProxyHandshakeEnterV1(Handshake* h,XL6_NetBufferGuardV1* guard)
{bool ok=h&&xl6NetBufferEnterV1(&h->input,guard);if(!h)xrtSetErrorKind(XERR_ARGUMENT);if(!ok)xl6NetBufferFailedV1();return ok;}
static void capturedFailure(Handshake* h)
{
    const xerror* error=xrtNetProxyHandshakeError(h->native);
    if(!xrtGetError()){if(error)xrtSetErrorTake(xrtErrorRef(error));else xrtSetErrorKind(XERR_STATE);}
    xl6NetBufferFailedV1();
}
int64_t xl6NetProxyHandshakeStateV1(void* raw)
{Handshake* h=raw;XL6_NetBufferGuardV1 g={0};if(!xl6NetProxyHandshakeEnterV1(h,&g))return XNET_PROXY_HANDSHAKE_ERROR;
 int64_t state=xrtNetProxyHandshakeState(h->native);xl6NetBufferLeaveV1(&g);return state;}
int64_t xl6NetProxyHandshakeStepV1(void* raw)
{Handshake* h=raw;XL6_NetBufferGuardV1 g={0};if(!xl6NetProxyHandshakeEnterV1(h,&g))return XNET_PROXY_HANDSHAKE_ERROR;
 xnetproxyhandshakestate state=xrtNetProxyHandshakeStep(h->native,&h->input.native);
 if(state==XNET_PROXY_HANDSHAKE_ERROR)h->input.faulted=true;
 xl6NetBufferLeaveV1(&g);if(state==XNET_PROXY_HANDSHAKE_ERROR)capturedFailure(h);return state;}
int64_t xl6NetProxyHandshakeSentV1(void* raw,int64_t size)
{
    if(size<0||(uint64_t)size>SIZE_MAX){xrtSetErrorKind(XERR_RANGE);xl6NetBufferFailedV1();return 0;}
    Handshake* h=raw;XL6_NetBufferGuardV1 g={0};if(!xl6NetProxyHandshakeEnterV1(h,&g))return 0;
    size_t count=xrtNetProxyHandshakeSent(h->native,(size_t)size);bool ok=!xrtGetError()&&count==(size_t)size;
    xl6NetBufferLeaveV1(&g);if(!ok)xl6NetBufferFailedV1();return (int64_t)count;
}
XL6_ProductBoolU32V1 xl6NetProxyHandshakeCodeV1(void* raw)
{Handshake* h=raw;XL6_NetBufferGuardV1 g={0};XL6_ProductBoolU32V1 result={0};if(!xl6NetProxyHandshakeEnterV1(h,&g))return result;
 uint32 code=0;result.e0=xrtNetProxyHandshakeCode(h->native,&code);result.e1=code;xl6NetBufferLeaveV1(&g);return result;}
void xl6NetProxyHandshakeThrowV1(void* raw)
{Handshake* h=raw;XL6_NetBufferGuardV1 g={0};if(!xl6NetProxyHandshakeEnterV1(h,&g))return;
 bool bad=xrtNetProxyHandshakeState(h->native)==XNET_PROXY_HANDSHAKE_ERROR;
 xl6NetBufferLeaveV1(&g);if(bad)capturedFailure(h);}
int64_t xl6NetProxyHandshakeBufferedV1(void* raw){return xl6NetBufferLengthV1(&((Handshake*)raw)->input);}
int64_t xl6NetProxyHandshakeLimitV1(const void* raw){return xl6NetBufferLimitV1(&((const Handshake*)raw)->input);}
void xl6NetProxyHandshakeAppendMoveV1(void* raw,void* input){xl6NetBufferMoveInputV1(&((Handshake*)raw)->input,input);}
bool xl6NetProxyHandshakeReadyV1(Handshake* h)
{
    XL6_NetBufferGuardV1 g={0};if(!xl6NetProxyHandshakeEnterV1(h,&g))return false;
    bool ok=xrtNetProxyHandshakeState(h->native)==XNET_PROXY_HANDSHAKE_READY;xl6NetBufferLeaveV1(&g);
    if(!ok){xrtSetErrorKind(XERR_STATE);xl6NetBufferFailedV1();}return ok;
}
void xl6NetProxyHandshakeMoveRemainingV1(void* raw,void* target)
{Handshake* h=raw;if(xl6NetProxyHandshakeReadyV1(h))xl6NetBufferMoveInputV1(target,&h->input);}
bool xl6NetProxyHandshakeOutputViewV1(Handshake* h,xnetspan* view)
{return xrtNetProxyHandshakeOutput(h->native,view)||(!view->Size&&!xrtGetError());}
int64_t xl6NetProxyHandshakeBoundFamilyV1(void* raw)
{Handshake* h=raw;XL6_NetBufferGuardV1 g={0};if(!xl6NetProxyHandshakeEnterV1(h,&g))return 0;xnetproxyendpoint e={0};
 bool ok=xrtNetProxyHandshakeBound(h->native,&e);xl6NetBufferLeaveV1(&g);if(!ok)xl6NetBufferFailedV1();return e.Address.Family;}
int64_t xl6NetProxyHandshakeBoundPortV1(void* raw)
{Handshake* h=raw;XL6_NetBufferGuardV1 g={0};if(!xl6NetProxyHandshakeEnterV1(h,&g))return 0;xnetproxyendpoint e={0};
 bool ok=xrtNetProxyHandshakeBound(h->native,&e);xl6NetBufferLeaveV1(&g);if(!ok)xl6NetBufferFailedV1();return e.Address.Port;}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!visit||!xrtValueGetHandle(value,&raw,&ops,&user)||!raw||user||ops!=xl6NetProxyHandshakeHandlePolicyV1Get()->Ops)return false;
    const Handshake* h=raw;
    return !xrtAtomic32Load(&h->input.gate,XMEMORY_ACQUIRE)&&
        xrtNetBufOwnershipTrace(&h->input.native,visit,context)&&
        xrtNetProxyHandshakeOwnershipTrace(h->native,visit,context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6NetProxyHandshakeHandlePolicyV1Get,xl6NetProxyHandshakeDestroyV1,trace)
