/* Fixed successful in-memory negotiation vectors and ordinary lifecycle/OOM.
 * No Socket, DNS, external proxy, traffic, malformed protocol or security test. */
#include "src/runtime/xl6_net_proxy_internal.h"
#include "src/runtime/xl6_native_handle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define VIEW(Text) ((xstrview){Text,sizeof(Text)-1})
#ifndef TEST_ROOT
#error TEST_ROOT is required
#endif
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void handle(void** raw,const XL6_NativeHandlePolicyV1* policy)
{
    xvalue* value=xl6NativeHandleTakeV1(raw,policy);assert(value&&!*raw);
    xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope freeze={0};size_t count;
    assert(xrtOwnershipFreezeTryBegin(&freeze)&&ref.Ops->Count(ref.Data,&count)&&count==1);
    const xrtownershipadapterv1* adapter=xrtValueHandleOwnershipAdapterV1(ref,policy->Ops,policy->Trace);assert(adapter);
    int token=0;assert(adapter->Hold(ref.Data)&&adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
    assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xrtValueRelease(value);
}
static void errorEnd(xl6_error_scope* scope,xerrkind kind)
{xl6_error_result result=xl6ErrorScopeEnd(scope);assert(!result.ok&&xrtErrorKind(result.error)==kind);xrtErrorFree(result.error);assert(!xrtGetError());}
static void* proxy(int64_t type)
{void* result=xl6NetProxyCreateTextV1(type,VIEW("127.0.0.1"),1080,VIEW(""),VIEW(""),XNET_PROXY_AUTH_AUTO);assert(result);return result;}
static void endpoints(void)
{
    void* p=proxy(XNET_PROXY_SOCKS5);assert(xl6NetProxyTypeV1(p)==XNET_PROXY_SOCKS5&&xl6NetProxyPortV1(p)==1080&&xl6NetProxyAuthV1(p)==XNET_PROXY_AUTH_NONE);
    void* copy=xl6NetProxyCloneV1(p);assert(copy);xl6NetProxyDestroyV1(p);assert(xl6NetProxyTypeV1(copy)==XNET_PROXY_SOCKS5);
    handle(&copy,xl6NetProxyHandlePolicyV1Get());
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    assert(!xl6NetProxyCreateTextV1(XNET_PROXY_SOCKS5,VIEW("127.0.0.1"),0,VIEW(""),VIEW(""),0));errorEnd(&scope,XERR_RANGE);
}
#if TEST_ROOT==1||TEST_ROOT==7
static void endpointValues(void)
{
    void* p=xl6NetProxyCreateTextV1(XNET_PROXY_SOCKS5,VIEW("127.0.0.1"),1080,VIEW("u\0x"),VIEW("p\0y"),0);assert(p);
    xvalue* value=xl6NetProxyUsernameV1(p);xbytesview bytes;assert(value&&xrtValueGetBytes(value,&bytes)&&bytes.Size==3&&!memcmp(bytes.Data,"u\0x",3));xrtValueRelease(value);
    value=xl6NetProxyPasswordV1(p);assert(value&&xrtValueGetBytes(value,&bytes)&&bytes.Size==3&&!memcmp(bytes.Data,"p\0y",3));xrtValueRelease(value);
    xstrview host=xl6NetProxyHostV1(p);assert(host.Size==9&&!memcmp(host.Data,"127.0.0.1",9));xrtFree((void*)host.Data);xl6NetProxyDestroyV1(p);
}
#endif
#if TEST_ROOT>=2
static void* handshake(int64_t type)
{void* p=proxy(type);void* h=xl6NetProxyHandshakeCreateV1(p,VIEW("127.0.0.1"),80,65536,100);assert(h);xl6NetProxyDestroyV1(p);return h;}
static void feed(void* raw,const void* data,size_t size)
{assert(xrtNetBufAppend(&((XL6_NetProxyHandshakeV1*)raw)->input.native,data,size));}
static void complete(void* raw)
{
    assert(xl6NetProxyHandshakeSentV1(raw,3)==3);feed(raw,"\5\0",2);
    assert(xl6NetProxyHandshakeStepV1(raw)==XNET_PROXY_HANDSHAKE_WRITE&&xl6NetProxyHandshakeSentV1(raw,10)==10);
    static const uint8_t reply[]={5,0,0,1,127,0,0,1,0,80,'a',0,'b'};feed(raw,reply,sizeof(reply));
    assert(xl6NetProxyHandshakeStepV1(raw)==XNET_PROXY_HANDSHAKE_READY&&xl6NetProxyHandshakeBufferedV1(raw)==3);
}
static void core(void)
{
    void* h=handshake(XNET_PROXY_SOCKS5);assert(xl6NetProxyHandshakeStateV1(h)==XNET_PROXY_HANDSHAKE_WRITE);
    XL6_ProductBoolU32V1 code=xl6NetProxyHandshakeCodeV1(h);assert(!code.e0&&!code.e1);
    XL6_NetBufferGuardV1 guard={0};assert(xl6NetBufferEnterV1(&((XL6_NetProxyHandshakeV1*)h)->input,&guard));
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));(void)xl6NetProxyHandshakeStateV1(h);errorEnd(&scope,XERR_STATE);xl6NetBufferLeaveV1(&guard);
    assert(xl6ErrorScopeBegin(&scope));assert(!xl6NetProxyHandshakeSentV1(h,100));errorEnd(&scope,XERR_STATE);
    complete(h);code=xl6NetProxyHandshakeCodeV1(h);assert(code.e0&&!code.e1);
    assert(xl6NetProxyHandshakeBoundPortV1(h)==80&&xl6NetProxyHandshakeBoundFamilyV1(h)==4);
    void* target=xl6NetBufferCreateV1(100);assert(target);assert(xrtMemDebugFailAfter(0));
    xl6NetProxyHandshakeMoveRemainingV1(h,target);assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(!xl6NetProxyHandshakeBufferedV1(h)&&xl6NetBufferLengthV1(target)==3);xl6NetBufferDestroyV1(target);
    xl6NetProxyHandshakeThrowV1(h);handle(&h,xl6NetProxyHandshakeHandlePolicyV1Get());
}
#endif
#if TEST_ROOT==3||TEST_ROOT==5||TEST_ROOT==6||TEST_ROOT==7
static void values(void)
{
    void* h=handshake(XNET_PROXY_SOCKS5);xvalue* output=xl6NetProxyHandshakeOutputV1(h);assert(output);
    complete(h);xvalue* tail=xl6NetProxyHandshakeRemainingV1(h,99);xbytesview bytes;assert(tail&&xrtValueGetBytes(tail,&bytes)&&bytes.Size==3&&!memcmp(bytes.Data,"a\0b",3));
#if TEST_ROOT==5||TEST_ROOT==7
    xstrview host=xl6NetProxyHandshakeBoundHostV1(h);assert(host.Size==9&&!memcmp(host.Data,"127.0.0.1",9));xrtFree((void*)host.Data);
#endif
#if TEST_ROOT==6||TEST_ROOT==7
    xnetaddr* address=xl6NetProxyHandshakeBoundAddressV1(h);assert(address&&address->Family==4&&address->Port==80);xl6NetAddressDestroyV1(address);
#endif
    xl6NetProxyHandshakeDestroyV1(h);assert(xrtValueGetBytes(output,&bytes)&&bytes.Size==3&&bytes.Data[0]==5);
    xrtValueRelease(tail);xrtValueRelease(output);
    h=handshake(XNET_PROXY_HTTP_CONNECT);output=xl6NetProxyHandshakeOutputV1(h);assert(output&&xrtValueGetBytes(output,&bytes)&&bytes.Size>0);
    assert(xl6NetProxyHandshakeSentV1(h,(int64_t)bytes.Size)==(int64_t)bytes.Size);
    xl6NetProxyHandshakeAppendTextV1(h,VIEW("HTTP/1.1 200 OK\r\n\r\na\0b"),-1);
    assert(xl6NetProxyHandshakeStepV1(h)==XNET_PROXY_HANDSHAKE_READY);
    XL6_ProductBoolU32V1 code=xl6NetProxyHandshakeCodeV1(h);assert(code.e0&&code.e1==200);
    tail=xl6NetProxyHandshakeRemainingV1(h,-1);assert(tail&&xrtValueGetBytes(tail,&bytes)&&bytes.Size==3&&!memcmp(bytes.Data,"a\0b",3));
    xl6NetProxyHandshakeDestroyV1(h);xrtValueRelease(output);xrtValueRelease(tail);
}
#endif
#if TEST_ROOT==4||TEST_ROOT==7
static void buffer(void)
{
    void* h=handshake(XNET_PROXY_SOCKS5);void* output=xl6BufferCreateReserveV1(100);assert(output);
    assert(xrtMemDebugFailAfter(0));assert(xl6NetProxyHandshakeOutputIntoV1(h,output,-1)==3);assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(((xbuffer*)output)->Size==3);assert(xl6NetProxyHandshakeSentV1(h,3)==3);
    xrtBufferClear(output);assert(xrtBufferAppend(output,(xbytesview){(const uint8_t*)"\5\0",2}));
    xl6NetProxyHandshakeAppendBufferV1(h,output,-1);
    assert(xl6NetProxyHandshakeStepV1(h)==XNET_PROXY_HANDSHAKE_WRITE);
    xl6BufferDestroyV1(output);xl6NetProxyHandshakeDestroyV1(h);
}
#endif
#if TEST_ROOT==7
static void prefixes(void)
{
    size_t terminal=SIZE_MAX;
    for(size_t point=0;point<256;++point){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        void *p=NULL,*h=NULL;xvalue* output=NULL;xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope)&&xrtMemDebugFailAfter(point));
#define REQUIRE(Expression) do{if(!(Expression)||scope.failed||xrtGetError())goto done;}while(0)
        REQUIRE((p=xl6NetProxyCreateTextV1(1,VIEW("127.0.0.1"),1080,VIEW(""),VIEW(""),0)));
        REQUIRE((h=xl6NetProxyHandshakeCreateV1(p,VIEW("127.0.0.1"),80,65536,100)));
        REQUIRE((output=xl6NetProxyHandshakeOutputV1(h)));xrtValueRelease(output);output=NULL;
        REQUIRE(xl6NetProxyHandshakeSentV1(h,3)==3);xl6NetProxyHandshakeAppendTextV1(h,VIEW("\5\0"),-1);REQUIRE(!scope.failed);
        REQUIRE(xl6NetProxyHandshakeStepV1(h)==XNET_PROXY_HANDSHAKE_WRITE);
done:;
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtValueRelease(output);xl6NetProxyHandshakeDestroyV1(h);xl6NetProxyDestroyV1(p);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);assert(hit?!outcome.ok&&xrtErrorKind(outcome.error)==XERR_MEMORY:outcome.ok&&!outcome.error);
        xrtErrorFree(outcome.error);assert(!xrtGetError());balanced(before);if(!hit){terminal=point;break;}
#undef REQUIRE
    }
    assert(terminal!=SIZE_MAX);printf("Proxy owned prefixes=%zu first-no-hit=%zu balanced\n",terminal+1,terminal);
}
#endif
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);endpoints();
#if TEST_ROOT==1||TEST_ROOT==7
    endpointValues();
#endif
#if TEST_ROOT>=2
    core();
#endif
#if TEST_ROOT==3||TEST_ROOT==5||TEST_ROOT==6||TEST_ROOT==7
    values();
#endif
#if TEST_ROOT==4||TEST_ROOT==7
    buffer();
#endif
#if TEST_ROOT==7
    prefixes();
#endif
    assert(!xrtGetError());balanced(before);puts("proxy offline lifecycle balanced");return 0;
}
