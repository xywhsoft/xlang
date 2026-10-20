/* Actual selected resident implementations, ordinary current-thread OOM,
 * graph/lifetime checks and only own IPv4/IPv6 loopback ephemeral datagrams.
 * No DNS, external peers, multicast traffic, probing or security tests. */
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_net_datagram_internal.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_net_socket_owner.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XL6_OwnershipCatalog* catalog;
static void balanced(const xmemdebugsnapshot* before)
{
    assert(!xrtGetError());xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void checked(xl6_error_scope* scope,xerrkind kind)
{
    xl6_error_result result=xl6ErrorScopeEnd(scope);
    assert(!result.ok&&xrtErrorKind(result.error)==kind);xrtErrorFree(result.error);assert(!xrtGetError());
}
#if !defined(TEST_DATAGRAM_CONTROL_ONLY)
static void graph(bool control,bool restore)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* slot=control?xl6NetDatagramControlCreateV1():xl6NetDatagramInfoCreateV1();assert(slot);
    xvalue* handle=xl6NativeHandleTakeV1(&slot,control?xl6NetDatagramControlHandlePolicyV1Get():xl6NetDatagramInfoHandlePolicyV1Get());
    assert(handle&&!slot);xrtownershipref ref=xrtValueOwnership(handle);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prep=NULL;
    const XL6_OwnershipFinishV1* finish=NULL;xrtownershipscope scope={0};int token=0;
    /* No concurrent mutation exists in this fixture. Requiring immediate
     * admission also keeps value-only closure tests independent of Thread. */
    assert(xrtOwnershipFreezeTryBegin(&scope));
    assert(xl6OwnershipCatalogProbeV1(ref,catalog,&adapter,&prep,&finish)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
    assert(adapter&&!prep&&!finish&&adapter->Hold(ref.Data)&&adapter->Claim(ref.Data,&token));
    if(restore){adapter->Restore(ref.Data,&token);assert(xrtOwnershipScopeEnd(&scope));}
    else {adapter->Clear(ref.Data,&token);assert(xrtOwnershipScopeEnd(&scope));
        assert(!adapter->Finish||adapter->Finish(ref.Data,&token));
        assert(!adapter->Finish||adapter->Finish(ref.Data,&token));}
    xrtValueRelease(handle);adapter->Drop(ref.Data);balanced(&before);
}
static void values(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_NetDatagramInfoV1* info=xl6NetDatagramInfoCreateV1();assert(info);
    assert(!xl6NetDatagramInfoValidV1(info)&&!xl6NetDatagramInfoFlagsV1(info));
    xl6_error_scope failure={0};assert(xl6ErrorScopeBegin(&failure));
    assert(!xl6NetDatagramInfoRemoteV1(info));checked(&failure,XERR_STATE);
    assert(xrtNetAddrLoopback(&info->remote,4,19)&&xrtNetAddrLoopback(&info->meta.Destination,4,0));
    info->valid=true;info->meta.Flags=15;info->meta.Interface=UINT32_MAX;info->meta.HopLimit=37;info->meta.TrafficClass=8;
    xnetaddr* remote=xl6NetDatagramInfoRemoteV1(info);xnetaddr* destination=xl6NetDatagramInfoDestinationV1(info);
    assert(remote&&destination&&remote->Port==19&&!destination->Port&&xrtNetAddrIsLoopback(destination));
    assert(xl6NetDatagramInfoInterfaceV1(info)==UINT32_MAX&&xl6NetDatagramInfoHopLimitV1(info)==37&&xl6NetDatagramInfoTrafficClassV1(info)==8);
    assert(xl6ErrorScopeBegin(&failure));assert(!xl6NetDatagramInfoSegmentSizeV1(info));checked(&failure,XERR_STATE);
    info->meta.Flags|=XNET_DGRAM_META_SEGMENT_SIZE;info->meta.SegmentSize=512;
    assert(xl6NetDatagramInfoSegmentSizeV1(info)==512);
    xl6NetDatagramInfoClearV1(info);assert(!info->valid&&remote->Port==19&&xrtNetAddrIsLoopback(remote));
    xnetdgramcontrol* base=xl6NetDatagramControlCreateV1();assert(base&&!base->Flags);
    xnetdgramcontrol* source=xl6NetDatagramControlSourceV1(base,destination);assert(source&&source->Flags==1&&!base->Flags);
    xnetdgramcontrol* iface=xl6NetDatagramControlInterfaceV1(source,UINT32_MAX);assert(iface&&iface->Interface==UINT32_MAX);
    xnetdgramcontrol* hop=xl6NetDatagramControlHopLimitV1(iface,255);assert(hop&&hop->HopLimit==255);
    xnetdgramcontrol* traffic=xl6NetDatagramControlTrafficClassV1(hop,0);assert(traffic&&traffic->TrafficClass==0);
    xnetdgramcontrol* segment=xl6NetDatagramControlSegmentSizeV1(traffic,65535);assert(segment&&segment->Flags==31);
    xnetdgramcontrol* cleared=xl6NetDatagramControlWithoutV1(segment,31);assert(cleared&&!cleared->Flags&&segment->Flags==31);
    assert(!cleared->Source.Family&&!cleared->Interface&&!cleared->HopLimit&&!cleared->TrafficClass&&!cleared->SegmentSize);
    assert(xl6ErrorScopeBegin(&failure));assert(!xl6NetDatagramControlHopLimitV1(base,256));checked(&failure,XERR_RANGE);
    assert(xl6ErrorScopeBegin(&failure));assert(!xl6NetDatagramControlSegmentSizeV1(base,0));checked(&failure,XERR_RANGE);
    assert(xl6ErrorScopeBegin(&failure));assert(!xl6NetDatagramControlInterfaceV1(base,-1));checked(&failure,XERR_RANGE);
    assert(xl6ErrorScopeBegin(&failure));assert(!xl6NetDatagramControlSourceV1(base,remote));checked(&failure,XERR_VALUE);
    assert(xl6ErrorScopeBegin(&failure));assert(!xl6NetDatagramControlWithoutV1(base,32));checked(&failure,XERR_VALUE);
    assert(xl6ErrorScopeBegin(&failure));assert(xrtMemDebugFailAfter(0));
    assert(!xl6NetDatagramControlHopLimitV1(base,1));assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    checked(&failure,XERR_MEMORY);assert(!base->Flags);
    xl6NetDatagramControlDestroyV1(cleared);xl6NetDatagramControlDestroyV1(segment);
    xl6NetDatagramControlDestroyV1(traffic);xl6NetDatagramControlDestroyV1(hop);
    xl6NetDatagramControlDestroyV1(iface);xl6NetDatagramControlDestroyV1(source);xl6NetDatagramControlDestroyV1(base);
    xl6NetAddressDestroyV1(remote);xl6NetAddressDestroyV1(destination);xl6NetDatagramInfoDestroyV1(info);balanced(&before);
}
#endif
#if defined(TEST_DATAGRAM_CONTROL_ONLY)||defined(TEST_DATAGRAM_INTEGRATION)
static void control(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* socket=xl6NetSocketCreateV1(2,4,true);assert(socket);
    XL6_NetSocketScopeV1 read={0};assert(xl6NetSocketOwnerEnterV1(socket,&read));
    int64_t available=xl6NetSocketMetadataAvailableV1(socket);
    assert(available>=0&&!xrtGetError());assert(xl6NetSocketControlAvailableV1(socket)>=0);
    assert(xl6NetSocketDatagramCapabilitiesV1(socket)>=0);
    xl6_error_scope failure={0};assert(xl6ErrorScopeBegin(&failure));
    xl6NetSocketMetadataSetV1(socket,0);checked(&failure,XERR_STATE);
    assert(xl6ErrorScopeBegin(&failure));xl6NetSocketMulticastLoopV1(socket,true);checked(&failure,XERR_STATE);
    xl6NetSocketOwnerLeaveV1(&read);xl6NetSocketMetadataSetV1(socket,0);assert(!xl6NetSocketMetadataEnabledV1(socket));
    xl6NetSocketMulticastLoopV1(socket,true);xl6NetSocketMulticastHopLimitV1(socket,1);
    xl6NetSocketMulticastInterfaceDefaultV1(socket);
    xnetaddr loop;assert(xrtNetAddrLoopback(&loop,4,0));
    xl6NetSocketMulticastInterfaceV1(socket,&loop);
    assert(xl6ErrorScopeBegin(&failure));xl6NetSocketMetadataSetV1(socket,XNET_DGRAM_META_TRUNCATED);checked(&failure,XERR_VALUE);
    assert(xl6ErrorScopeBegin(&failure));xl6NetSocketMulticastHopLimitV1(socket,256);checked(&failure,XERR_RANGE);
    xl6NetSocketDestroyV1(socket);
    socket=xl6NetSocketCreateV1(1,4,true);assert(socket);
    assert(xl6ErrorScopeBegin(&failure));assert(!xl6NetSocketControlAvailableV1(socket));checked(&failure,XERR_VALUE);
    xl6NetSocketDestroyV1(socket);balanced(&before);
}
#endif
#if defined(TEST_DATAGRAM_IO)
#if defined(_WIN32)
#include <windows.h>
static uint64_t hostMilliseconds(void)
{
    LARGE_INTEGER ticks,frequency;
    assert(QueryPerformanceCounter(&ticks)&&QueryPerformanceFrequency(&frequency)&&frequency.QuadPart>0);
    return (uint64_t)(ticks.QuadPart/frequency.QuadPart)*1000+
        (uint64_t)(ticks.QuadPart%frequency.QuadPart)*1000/(uint64_t)frequency.QuadPart;
}
static void hostPause(void){Sleep(1);}
#else
#include <time.h>
static uint64_t hostMilliseconds(void)
{
    struct timespec now;assert(!clock_gettime(CLOCK_MONOTONIC,&now));
    return (uint64_t)now.tv_sec*1000+(uint64_t)now.tv_nsec/1000000;
}
static void hostPause(void)
{const struct timespec delay={0,1000000};(void)nanosleep(&delay,NULL);}
#endif
static XL6_ProductI64U64V1 receive(void* socket,xbuffer* buffer,XL6_NetDatagramInfoV1* info,int64_t max)
{
    XL6_ProductI64U64V1 result;uint64_t start=hostMilliseconds();
    do {result=xl6NetSocketReceiveFromIntoV1(socket,buffer,info,max);
        assert(hostMilliseconds()-start<5000);if(result.e0==XNET_RESULT_AGAIN)hostPause();
    } while(result.e0==XNET_RESULT_AGAIN);
    return result;
}
static void ordinary(int family)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* receiver=xl6NetSocketCreateV1(2,family,true);void* sender=xl6NetSocketCreateV1(2,family,true);assert(receiver&&sender);
    xnetaddr loop;assert(xrtNetAddrLoopback(&loop,(xnetfamily)family,0));
    xl6NetSocketBindV1(receiver,&loop);xl6NetSocketBindV1(sender,&loop);
    xnetaddr* target=xl6NetSocketLocalV1(receiver);xnetaddr* source=xl6NetSocketLocalV1(sender);assert(target&&source&&target->Port&&source->Port);
    int64_t capabilities=0;
    #if defined(TEST_DATAGRAM_INTEGRATION)
    int64_t metadata=xl6NetSocketMetadataAvailableV1(receiver)&15;
    xl6NetSocketMetadataSetV1(receiver,metadata);assert(xl6NetSocketMetadataEnabledV1(receiver)==metadata);
    capabilities=xl6NetSocketControlAvailableV1(sender);
    #endif
    void* plain=xl6NetDatagramControlCreateV1();void* selected=NULL;
    if(capabilities&XNET_DGRAM_CONTROL_HOP_LIMIT)selected=xl6NetDatagramControlHopLimitV1(plain,37);
    else selected=xl6NetDatagramControlWithoutV1(plain,0);
    assert(selected);XL6_NetDatagramInfoV1* info=xl6NetDatagramInfoCreateV1();assert(info);xbuffer buffer={0};
    const char payload[]={'a',0,'b',(char)0xe4,(char)0xbd,(char)0xa0};
    XL6_ProductI64U64V1 sent=xl6NetSocketSendMessageTextV1(sender,(xstrview){payload,sizeof(payload)},target,selected,99);
    assert(sent.e0==XNET_RESULT_OK&&sent.e1==sizeof(payload));
    xl6_error_scope failure={0};assert(xl6ErrorScopeBegin(&failure));assert(xrtMemDebugFailAfter(0));
    XL6_ProductI64U64V1 got=xl6NetSocketReceiveFromIntoV1(receiver,&buffer,info,64);
    assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();checked(&failure,XERR_MEMORY);
    assert(got.e0==XNET_RESULT_ERROR&&!got.e1&&!buffer.Size&&!info->valid);
    got=receive(receiver,&buffer,info,64);assert(got.e0==XNET_RESULT_OK&&got.e1==sizeof(payload));
    assert(buffer.Size==sizeof(payload)&&!memcmp(buffer.Data,payload,sizeof(payload))&&info->valid&&xrtNetAddrEqual(&info->remote,source));
    if(info->meta.Flags&XNET_DGRAM_META_DESTINATION){
        assert(!info->meta.Destination.Port&&info->meta.Destination.Family==target->Family&&
            !memcmp(info->meta.Destination.Address,target->Address,family==4?4:16));
        /* IPv6 packet metadata records the receiving interface as Scope,
         * even for ::1. SameIP also compares Scope, unlike address bytes. */
        if(family==6&&(info->meta.Flags&XNET_DGRAM_META_INTERFACE))assert(info->meta.Destination.Scope==info->meta.Interface);
    }
    if(info->meta.Flags&XNET_DGRAM_META_INTERFACE)assert(info->meta.Interface);
    if((info->meta.Flags&XNET_DGRAM_META_HOP_LIMIT)&&(capabilities&XNET_DGRAM_CONTROL_HOP_LIMIT))assert(info->meta.HopLimit==37);
    XL6_NetDatagramInfoV1 snapshot;memcpy(&snapshot,info,sizeof(snapshot));size_t previous=buffer.Size;
    got=xl6NetSocketReceiveFromIntoV1(receiver,&buffer,info,64);
    assert(got.e0==XNET_RESULT_AGAIN&&!got.e1&&buffer.Size==previous&&!memcmp(&snapshot,info,sizeof(snapshot)));
    xvalue* bytes=xrtValueBytes((xbytesview){(const uint8_t*)payload,sizeof(payload)});assert(bytes);
    sent=xl6NetSocketSendMessageBytesV1(sender,bytes,target,plain,3);assert(sent.e0==XNET_RESULT_OK&&sent.e1==3);
    got=receive(receiver,&buffer,info,1);assert(got.e0==XNET_RESULT_TRUNCATED&&got.e1==1&&buffer.Size==previous+1);
    assert(xl6NetSocketConnectV1(sender,target)==XNET_RESULT_OK);
    sent=xl6NetSocketSendConnectedMessageBytesV1(sender,bytes,plain,2);assert(sent.e0==XNET_RESULT_OK&&sent.e1==2);
    got=receive(receiver,&buffer,info,64);assert(got.e0==XNET_RESULT_OK&&got.e1==2);
    xbuffer input={0};assert(xrtBufferAppend(&input,(xbytesview){(const uint8_t*)payload,sizeof(payload)}));
    sent=xl6NetSocketSendMessageBufferV1(sender,&input,target,plain,4);assert(sent.e0==XNET_RESULT_OK&&sent.e1==4);
    got=receive(receiver,&buffer,info,64);assert(got.e0==XNET_RESULT_OK&&got.e1==4);
    sent=xl6NetSocketSendConnectedMessageBufferV1(sender,&input,plain,1);assert(sent.e0==XNET_RESULT_OK&&sent.e1==1);
    got=receive(receiver,&buffer,info,64);assert(got.e0==XNET_RESULT_OK&&got.e1==1);
    sent=xl6NetSocketSendConnectedMessageTextV1(sender,(xstrview){"",0},plain,-1);assert(sent.e0==XNET_RESULT_OK&&!sent.e1);
    previous=buffer.Size;got=receive(receiver,&buffer,info,0);assert(got.e0==XNET_RESULT_OK&&!got.e1&&info->valid&&buffer.Size==previous);
    assert(xl6ErrorScopeBegin(&failure));
    sent=xl6NetSocketSendMessageTextV1(sender,(xstrview){payload,sizeof(payload)},NULL,plain,-1);checked(&failure,XERR_ARGUMENT);assert(sent.e0==XNET_RESULT_ERROR);
    assert(xl6ErrorScopeBegin(&failure));
    sent=xl6NetSocketSendConnectedMessageTextV1(sender,(xstrview){payload,sizeof(payload)},plain,-2);checked(&failure,XERR_RANGE);assert(sent.e0==XNET_RESULT_ERROR);
    got=xl6NetSocketReceiveFromIntoV1(receiver,&buffer,info,64);assert(got.e0==XNET_RESULT_AGAIN);
    xrtValueRelease(bytes);xrtBufferUnit(&input);xrtBufferUnit(&buffer);
    xl6NetDatagramInfoDestroyV1(info);xl6NetDatagramControlDestroyV1(selected);xl6NetDatagramControlDestroyV1(plain);
    xl6NetAddressDestroyV1(source);xl6NetAddressDestroyV1(target);xl6NetSocketDestroyV1(sender);xl6NetSocketDestroyV1(receiver);
    balanced(&before);
}
#endif
int main(void)
{
    assert(xrtMemDebugEnable(true));xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));assert(xl6ErrorScopeEnd(&warm).ok);
    const XL6_OwnershipDomainV1* domains[]={
        #include "net-datagram-domains.h"
    };
    catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));assert(catalog);
    #if !defined(TEST_DATAGRAM_CONTROL_ONLY)
    for(unsigned i=0;i<2;++i){values();graph(false,false);graph(false,true);graph(true,false);graph(true,true);}
    #endif
    #if defined(TEST_DATAGRAM_CONTROL_ONLY)||defined(TEST_DATAGRAM_INTEGRATION)
    control();
    #endif
    #if defined(TEST_DATAGRAM_IO)
    ordinary(4);ordinary(6);
    #endif
    xl6OwnershipCatalogDestroyV1(catalog);
    puts("Datagram runtime: exact values/graph/control/IO, checked errors and memory balance; no external traffic");return 0;
}
