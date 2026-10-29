/* Ordinary lifecycle and own loopback dynamic ports only. No external names,
 * other services, protocol probes, security testing, packet fuzz or scanning. */
#include "net-socket-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include "release/include/xl_clock.h"
#define XL6_TEXT_API
#include "src/runtime/xl6_net_socket_abi.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_net_socket_owner.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef XL6_NetSocketOwnerV1 Owner;
static XL6_OwnershipCatalog* catalog;
static void frozen(xrtownershipscope* scope)
{XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);while(!xrtOwnershipFreezeTryBegin(scope)){assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}}
static void balanced(const xmemdebugsnapshot* before)
{
    xrtClearError();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void lifecycle(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Owner* owner=xl6NetSocketOwnerOpenV1(4,1,true);assert(owner);
    XL6_NetSocketScopeV1 shared={0},other={0};assert(xl6NetSocketOwnerEnterV1(owner,&shared));
    assert(!xl6NetSocketOwnerControlV1(owner,&other)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    assert(!xl6NetSocketOwnerCloseV1(owner)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xrtownershipscope scope={0};frozen(&scope);const xrtownershipadapterv1* adapter=NULL;
    const xrtownershippreparationv1* prep=NULL;const XL6_OwnershipFinishV1* done=NULL;
    assert(xl6OwnershipCatalogProbeV1(xl6NetSocketOwnershipV1(owner),catalog,&adapter,&prep,&done)==XL6_OWNERSHIP_ADMISSION_BUSY);
    assert(!adapter&&!prep&&!done&&xrtOwnershipScopeEnd(&scope));
    xl6NetSocketOwnerLeaveV1(&shared);assert(xl6NetSocketOwnerControlV1(owner,&other));
    assert(!xl6NetSocketOwnerEnterV1(owner,&shared)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xl6NetSocketOwnerLeaveV1(&other);assert(xl6NetSocketOwnerCloseV1(owner)&&xl6NetSocketOwnerClosedV1(owner));
    assert(xl6NetSocketOwnerCloseV1(owner));assert(!xl6NetSocketOwnerEnterV1(owner,&shared)&&xrtErrorKind(xrtGetError())==XERR_CLOSED);
    xrtClearError();xl6NetSocketOwnerReleaseV1(owner);balanced(&before);
    xrtMemDebugSnapshot(&before);owner=xl6NetSocketOwnerOpenV1(4,2,true);assert(owner&&xl6NetSocketOwnerEnterV1(owner,&shared));
    xl6NetSocketOwnerReleaseV1(owner);xl6NetSocketOwnerLeaveV1(&shared);balanced(&before);
}
static bool forbidden(const void* raw,size_t* refs){(void)raw;(void)refs;abort();}
static void graph(bool parentFirst,bool restore)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* slot=xl6NetSocketOwnerOpenV1(4,2,true);assert(slot);Owner* owner=slot;
    xvalue* handle=xl6NativeHandleTakeV1(&slot,xl6NetSocketHandlePolicyV1Get());assert(handle&&!slot);
    xrtownershipref refs[2];refs[0]=xrtValueOwnership(handle);refs[1]=xl6NetSocketOwnershipV1(owner);
    const xrtownershipadapterv1* adapters[2]={0};xrtownershipscope scope={0};frozen(&scope);int token=0;
    xrtownershipref forged=refs[1];xrtownershipops copy=*forged.Ops;copy.Count=forbidden;forged.Ops=&copy;
    const xrtownershipadapterv1* bad=NULL;const xrtownershippreparationv1* prep=NULL;const XL6_OwnershipFinishV1* done=NULL;
    assert(xl6OwnershipCatalogProbeV1(forged,catalog,&bad,&prep,&done)==XL6_OWNERSHIP_ADMISSION_REFUSED&&!bad&&!prep&&!done);
    for(unsigned i=0;i<2;++i){assert(xl6OwnershipCatalogProbeV1(refs[i],catalog,&adapters[i],&prep,&done)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
        assert(adapters[i]&&!prep&&!done&&adapters[i]->Hold(refs[i].Data)&&adapters[i]->Claim(refs[i].Data,&token));}
    if(restore){for(unsigned i=0;i<2;++i)adapters[i]->Restore(refs[i].Data,&token);assert(xrtOwnershipScopeEnd(&scope));
        for(unsigned i=0;i<2;++i)adapters[i]->Drop(refs[i].Data);
        xrtValueRelease(handle);balanced(&before);return;}
    for(unsigned i=0;i<2;++i)adapters[i]->Clear(refs[i].Data,&token);
    assert(xrtOwnershipScopeEnd(&scope));
    for(unsigned step=0;step<2;++step){unsigned i=parentFirst?step:1-step;
        assert(!adapters[i]->Finish||adapters[i]->Finish(refs[i].Data,&token));}
    assert(adapters[1]->Finish(refs[1].Data,&token));xrtValueRelease(handle);
    for(unsigned i=0;i<2;++i)adapters[i]->Drop(refs[i].Data);
    balanced(&before);
}
static void ordinary_tcp(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    Owner *listener=xl6NetSocketCreateV1(1,4,true),*client=xl6NetSocketCreateV1(1,4,true),*peer=xl6NetSocketEmptyV1();assert(listener&&client&&peer);
    xnetaddr loop;assert(xrtNetAddrLoopback(&loop,XNET_FAMILY_IPV4,0));xl6NetSocketBindV1(listener,&loop);xl6NetSocketListenV1(listener,8);
    xnetaddr* local=xl6NetSocketLocalV1(listener);assert(local&&local->Port&&xrtNetAddrIsLoopback(local));
    int64 status=xl6NetSocketConnectV1(client,local);assert(status==XNET_RESULT_OK||status==XNET_RESULT_AGAIN);
    XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);
    while((status=xl6NetSocketAcceptIntoV1(listener,peer))==XNET_RESULT_AGAIN){assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}
    assert(status==XNET_RESULT_OK);
    while((status=xl6NetSocketFinishConnectV1(client))==XNET_RESULT_AGAIN){assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}
    assert(status==XNET_RESULT_OK&&xl6NetSocketKindV1(peer)==XNET_SOCKET_STREAM&&xl6NetSocketFamilyV1(peer)==4);
    assert(xl6NetSocketGetV1(peer,XNET_OPTION_NONBLOCK)==1);xl6NetSocketSetV1(peer,XNET_OPTION_NO_DELAY,1);
    const char payload[]={'a',0,'b',(char)0xe4,(char)0xbd,(char)0xa0};
    XL6_ProductI64U64V1 sent=xl6NetSocketSendTextV1(client,(xstrview){payload,sizeof(payload)},100);
    assert(sent.e0==XNET_RESULT_OK&&sent.e1==sizeof(payload));
    xbuffer output={0};
    xl6_error_scope failedRead={0};assert(xl6ErrorScopeBegin(&failedRead));assert(xrtMemDebugFailAfter(0));
    XL6_ProductI64U64V1 read=xl6NetSocketReceiveIntoV1(peer,&output,64);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    xl6_error_result failure=xl6ErrorScopeEnd(&failedRead);assert(hit&&!failure.ok&&read.e0==XNET_RESULT_ERROR&&!read.e1&&!output.Size);xrtErrorFree(failure.error);
    while(output.Size<sizeof(payload)){
        read=xl6NetSocketReceiveIntoV1(peer,&output,64);assert(read.e0==XNET_RESULT_OK||read.e0==XNET_RESULT_AGAIN);
        assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}
    assert(output.Size==sizeof(payload)&&!memcmp(output.Data,payload,sizeof(payload)));
    size_t size=output.Size;read=xl6NetSocketReceiveIntoV1(peer,&output,64);assert(read.e0==XNET_RESULT_AGAIN&&!read.e1&&output.Size==size);
    xvalue* bytes=xrtValueBytes((xbytesview){(const uint8_t*)payload,sizeof(payload)});assert(bytes);
    sent=xl6NetSocketSendBytesV1(client,bytes,3);assert(sent.e0==XNET_RESULT_OK&&sent.e1==3);xrtValueRelease(bytes);
    xl6NetSocketShutdownV1(client,XNET_SHUTDOWN_WRITE);
    while((read=xl6NetSocketReceiveIntoV1(peer,&output,64)).e0!=XNET_RESULT_CLOSED){
        assert(read.e0==XNET_RESULT_OK||read.e0==XNET_RESULT_AGAIN);assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}
    assert(!read.e1&&output.Size==sizeof(payload)+3);
    xrtBufferUnit(&output);xrtFree(local);xl6NetSocketDestroyV1(peer);xl6NetSocketDestroyV1(client);xl6NetSocketDestroyV1(listener);balanced(&before);
}
static void oom(void)
{
    unsigned prefixes=0;
    for(size_t n=0;n<32;++n){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
        assert(xrtMemDebugFailAfter(n));void* owner=xl6NetSocketCreateV1(2,4,true);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        xl6_error_result result=xl6ErrorScopeEnd(&scope);
        if(hit){assert(!owner&&!result.ok);++prefixes;}else assert(owner&&result.ok);
        xrtErrorFree(result.error);xl6NetSocketDestroyV1(owner);balanced(&before);if(!hit)break;}
    assert(prefixes>=2&&prefixes<32);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));assert(xl6ErrorScopeEnd(&warm).ok);
    const XL6_OwnershipDomainV1* domains[]={
        #include "net-socket-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));assert(catalog);
    for(unsigned i=0;i<4;++i){lifecycle();for(unsigned order=0;order<2;++order){graph(order,false);graph(order,true);}ordinary_tcp();oom();}
    xl6OwnershipCatalogDestroyV1(catalog);
    puts("Socket runtime: 8 guarded owner lifetimes, 16 graph/restore orders, 4 own-loopback TCP/NUL/EOF and nonconsuming OOM rounds; exact memory balance");return 0;
}
