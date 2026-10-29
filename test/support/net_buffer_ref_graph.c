/* Offline ownership only: no socket, protocol parsing or handshake execution. */
#include "buffer-ref-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#define XL6_TEXT_API
#include "src/runtime/xl6_product_abi.h"
#include "src/runtime/xl6_net_bytes_abi.h"
#include "src/runtime/xl6_net_buffer_abi.h"
#include "src/runtime/xl6_net_proxy_abi.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
static XL6_OwnershipCatalog* catalog;
static bool admit(xrtownershipref ref,void* context)
{
    const xrtownershipadapterv1* a=NULL;const xrtownershippreparationv1* p=NULL;
    const xrtownershipfinishv1* f=NULL;(void)context;
    return xl6OwnershipCatalogProbeV1(ref,catalog,&a,&p,&f)==XL6_OWNERSHIP_ADMISSION_ADMITTED;
}
static bool countEdge(xrtownershipref ref,void* raw)
{assert(ref.Data&&ref.Ops);++*(unsigned*)raw;return true;}
static void nativeTrace(void)
{
    xnetbuf chain={0};assert(xrtNetBufInit(&chain,NULL));unsigned edges=0;
    assert(!xrtNetBufOwnershipTrace(&chain,NULL,NULL));
    assert(xrtNetBufOwnershipTrace(&chain,countEdge,&edges)&&!edges);
    xnetbytes* bytes=xrtNetBytesCopy("a\0b",3);assert(bytes);
    assert(xrtNetBufAppendBytes(&chain,bytes,0,2)&&xrtNetBufAppendBytes(&chain,bytes,2,1));
    xrtownershipscope frozen={0};assert(xrtOwnershipFreezeTryBegin(&frozen));
    assert(xrtNetBufOwnershipTrace(&chain,countEdge,&edges)&&edges==2);
    assert(xrtOwnershipScopeEnd(&frozen));xrtNetBufClear(&chain);xrtNetBytesDestroy(bytes);
    assert(xrtNetBufAppendBorrow(&chain,"x",1));edges=0;
    assert(!xrtNetBufOwnershipTrace(&chain,countEdge,&edges)&&!edges);xrtNetBufClear(&chain);
}
static void ownerGraph(unsigned kind)
{
    void* source=xl6NetBufferCreateV1(64);void* bytes=xl6NetBytesFromTextV1((xstrview){"a\0b",3},-1);
    assert(source&&bytes);xrtownershipref expected=xrtNetBytesOwnership(bytes),proxyEdge={0};
    xl6NetBufferAppendRefV1(source,bytes,2,0);xl6NetBufferAppendRefV1(source,bytes,1,2);
    xl6NetBytesDestroyV1(bytes);void* target=NULL;const XL6_NativeHandlePolicyV1* policy=NULL;
    if(kind==0){target=xl6NetBufferCreateV1(64);policy=xl6NetBufferHandlePolicyV1Get();xl6NetBufferAppendMoveV1(target,source);}
    if(kind==1){target=xl6NetLineFramerCreateTextV1((xstrview){"\n",1},32,false,64,-1);
        policy=xl6NetLineFramerHandlePolicyV1Get();assert(target);xl6NetLineFramerAppendMoveV1(target,source);}
    if(kind==2){target=xl6NetLengthFramerCreateV1(0,1,0,1,32,false,64);
        policy=xl6NetLengthFramerHandlePolicyV1Get();assert(target);xl6NetLengthFramerAppendMoveV1(target,source);}
    if(kind==3){void* proxy=xl6NetProxyCreateTextV1(XNET_PROXY_HTTP_CONNECT,(xstrview){"127.0.0.1",9},80,
            (xstrview){"",0},(xstrview){"",0},XNET_PROXY_AUTH_AUTO);assert(proxy);proxyEdge=xrtNetProxyOwnership(proxy);
        target=xl6NetProxyHandshakeCreateV1(proxy,(xstrview){"127.0.0.1",9},80,64,64);
        policy=xl6NetProxyHandshakeHandlePolicyV1Get();assert(target);xl6NetProxyDestroyV1(proxy);
        xl6NetProxyHandshakeAppendMoveV1(target,source);}
    assert(target&&!xl6NetBufferLengthV1(source));xl6NetBufferDestroyV1(source);
    xvalue* handle=xl6NativeHandleTakeV1(&target,policy);assert(handle&&!target);
    xrtownershipscope frozen={0};assert(xrtOwnershipFreezeTryBegin(&frozen));
    xrtownershipref root=xrtValueOwnership(handle);xrtownershipsnapshot* proof=NULL;
    assert(xrtOwnershipSnapshotCreate(&root,1,&root,1,admit,NULL,&proof));
    assert(xrtOwnershipSnapshotNodeCount(proof)==(kind==3?3:2));bool found=false,proxyFound=false;
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode node;
        assert(xrtOwnershipSnapshotNode(proof,i,&node)&&!node.Reachable);
        if(node.Reference.Data==expected.Data&&node.Reference.Ops==expected.Ops)found=true;
        if(node.Reference.Data==proxyEdge.Data&&node.Reference.Ops==proxyEdge.Ops)proxyFound=true;
    }
    assert(found&&(kind!=3||proxyFound));xrtOwnershipSnapshotDestroy(proof);
    assert(xrtOwnershipScopeEnd(&frozen));xrtValueRelease(handle);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));
    xl6_error_result result=xl6ErrorScopeEnd(&warm);assert(result.ok);xrtErrorFree(result.error);
    const XL6_OwnershipDomainV1* domains[]={
        #include "buffer-ref-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));assert(catalog);
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    nativeTrace();for(unsigned kind=0;kind<4;++kind)ownerGraph(kind);
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount&&before.OverflowCount==after.OverflowCount&&before.UnderflowCount==after.UnderflowCount);
    xl6OwnershipCatalogDestroyV1(catalog);assert(xrtMemDebugReset());
    puts("Buffer reference graphs: 4 exact owners, duplicate payload edges, retained proxy, unknown borrow rejected; balanced");return 0;
}
