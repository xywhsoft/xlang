/* Offline snapshots and empty local queues only. No error-producing packets,
 * ICMP, external names/peers, multicast or network-security tests. */
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_net_datagram_error_internal.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_net_socket_owner.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static void checked(xl6_error_scope* scope,xerrkind kind)
{xl6_error_result error=xl6ErrorScopeEnd(scope);assert(!error.ok&&xrtErrorKind(error.error)==kind);xrtErrorFree(error.error);assert(!xrtGetError());}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(!xrtGetError());
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
#if !defined(TEST_DATAGRAM_CONTROL_ONLY)
static void values(XL6_OwnershipCatalog* catalog)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_NetDatagramErrorInfoV1* info=xl6NetDatagramErrorInfoCreateV1();assert(info);
    assert(!xl6NetDatagramErrorInfoValidV1(info)&&!xl6NetDatagramErrorInfoFlagsV1(info));
    xl6_error_scope failure={0};assert(xl6ErrorScopeBegin(&failure));
    assert(!xl6NetDatagramErrorInfoOriginV1(info));checked(&failure,XERR_STATE);
    info->valid=true;info->error=(xnetdgramerror){.Flags=31,.Origin=XNET_DGRAM_ERROR_ICMP6,.Kind=XERR_IO,
        .SystemCode=INT_MIN,.Type=255,.Code=129,.Info=UINT32_MAX,.Data=UINT32_MAX,.PathMtu=SIZE_MAX};
    assert(xrtNetAddrLoopback(&info->error.Remote,6,29)&&xrtNetAddrLoopback(&info->error.Offender,6,0));
    assert(xl6NetDatagramErrorInfoOriginV1(info)==3&&xl6NetDatagramErrorInfoKindCodeV1(info)==XERR_IO);
    assert(xl6NetDatagramErrorInfoSystemCodeV1(info)==INT_MIN&&xl6NetDatagramErrorInfoTypeV1(info)==255&&
        xl6NetDatagramErrorInfoCodeV1(info)==129&&xl6NetDatagramErrorInfoInfoV1(info)==UINT32_MAX&&
        xl6NetDatagramErrorInfoDataV1(info)==UINT32_MAX&&xl6NetDatagramErrorInfoPathMtuV1(info)==SIZE_MAX&&!xrtGetError());
    xnetaddr* remote=xl6NetDatagramErrorInfoRemoteV1(info);xnetaddr* offender=xl6NetDatagramErrorInfoOffenderV1(info);
    assert(remote&&offender&&remote->Port==29&&!offender->Port);
    info->error.Flags=0;assert(xl6ErrorScopeBegin(&failure));
    assert(!xl6NetDatagramErrorInfoPathMtuV1(info));checked(&failure,XERR_STATE);
    assert(xl6ErrorScopeBegin(&failure));assert(!xl6NetDatagramErrorInfoRemoteV1(info));checked(&failure,XERR_STATE);
    assert(xl6ErrorScopeBegin(&failure));assert(!xl6NetDatagramErrorInfoOffenderV1(info));checked(&failure,XERR_STATE);
    info->error.Flags=XNET_DGRAM_ERROR_REMOTE;
    assert(xl6ErrorScopeBegin(&failure)&&xrtMemDebugFailAfter(0));
    assert(!xl6NetDatagramErrorInfoRemoteV1(info)&&xrtMemDebugFailTriggered());xrtMemDebugFailClear();checked(&failure,XERR_MEMORY);
    xl6NetDatagramErrorInfoClearV1(info);assert(!info->valid&&remote->Port==29&&xrtNetAddrIsLoopback(offender));
    xl6NetAddressDestroyV1(remote);xl6NetAddressDestroyV1(offender);xl6NetDatagramErrorInfoDestroyV1(info);
    for(unsigned restore=0;restore<2;++restore){
        void* slot=xl6NetDatagramErrorInfoCreateV1();assert(slot);
        xvalue* handle=xl6NativeHandleTakeV1(&slot,xl6NetDatagramErrorInfoHandlePolicyV1Get());assert(handle&&!slot);
        xrtownershipref ref=xrtValueOwnership(handle);xrtownershipscope scope={0};assert(xrtOwnershipFreezeTryBegin(&scope));
        const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
        const XL6_OwnershipFinishV1* completion=NULL;int token=0;
        assert(xl6OwnershipCatalogProbeV1(ref,catalog,&adapter,&preparation,&completion)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
        assert(adapter&&!preparation&&!completion&&adapter->Hold(ref.Data)&&adapter->Claim(ref.Data,&token));
        if(restore)adapter->Restore(ref.Data,&token);else adapter->Clear(ref.Data,&token);
        assert(xrtOwnershipScopeEnd(&scope));
        if(!restore){assert(!adapter->Finish||adapter->Finish(ref.Data,&token));assert(!adapter->Finish||adapter->Finish(ref.Data,&token));}
        xrtValueRelease(handle);adapter->Drop(ref.Data);
    }
    balanced(&before);
}
#endif
#if defined(TEST_DATAGRAM_CONTROL_ONLY)||defined(TEST_DATAGRAM_INTEGRATION)
static void control(int family)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* socket=xl6NetSocketCreateV1(2,family,true);assert(socket);
    uint32_t caps=(uint32_t)xl6NetSocketDatagramCapabilitiesV1(socket);
    if(caps&XNET_DGRAM_CAP_PATH_MTU_MODE){
        int64_t mode=xl6NetSocketPathMtuModeV1(socket);assert(mode>=0&&mode<=3);
        xl6NetSocketPathMtuModeSetV1(socket,XNET_PMTU_FRAGMENT);assert(xl6NetSocketPathMtuModeV1(socket)==XNET_PMTU_FRAGMENT);
        xl6NetSocketPathMtuModeSetV1(socket,mode);
        XL6_NetSocketScopeV1 read={0};assert(xl6NetSocketOwnerEnterV1(socket,&read));
        assert(xl6NetSocketPathMtuModeV1(socket)==mode);xl6_error_scope error={0};assert(xl6ErrorScopeBegin(&error));
        xl6NetSocketPathMtuModeSetV1(socket,mode);checked(&error,XERR_STATE);xl6NetSocketOwnerLeaveV1(&read);
    }
    xl6_error_scope error={0};assert(xl6ErrorScopeBegin(&error));xl6NetSocketPathMtuModeSetV1(socket,4);checked(&error,XERR_VALUE);
    if(caps&XNET_DGRAM_CAP_ERROR_QUEUE){
        xl6NetSocketDatagramErrorsSetV1(socket,true);assert(xl6NetSocketDatagramErrorsV1(socket));
        xl6NetSocketDatagramErrorsSetV1(socket,false);assert(!xl6NetSocketDatagramErrorsV1(socket));
    }else {assert(xl6ErrorScopeBegin(&error));xl6NetSocketDatagramErrorsSetV1(socket,true);checked(&error,XERR_UNSUPPORTED);}
    if(caps&XNET_DGRAM_CAP_PATH_MTU_QUERY){
        /* Query the OS route to our own bound peer. UDP connect sends no packet;
         * this is not PMTU probing or induced ICMP. */
        void* peer=xl6NetSocketCreateV1(2,family,true);assert(peer);xnetaddr loop;
        assert(xrtNetAddrLoopback(&loop,(xnetfamily)family,0));xl6NetSocketBindV1(peer,&loop);
        void* target=xl6NetSocketLocalV1(peer);assert(target&&xl6NetSocketConnectV1(socket,target)==XNET_RESULT_OK);
        assert(xl6NetSocketPathMtuV1(socket)>0&&!xrtGetError());
        xl6NetAddressDestroyV1(target);xl6NetSocketDestroyV1(peer);
    }
    xl6NetSocketDestroyV1(socket);balanced(&before);
}
#endif
#if defined(TEST_DATAGRAM_IO)
static void emptyQueue(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* socket=xl6NetSocketCreateV1(2,4,true);assert(socket);
    XL6_NetDatagramErrorInfoV1* info=xl6NetDatagramErrorInfoCreateV1();assert(info);
    info->valid=true;info->error.Flags=XNET_DGRAM_ERROR_PATH_MTU;info->error.PathMtu=1200;
    XL6_NetDatagramErrorInfoV1 saved;memcpy(&saved,info,sizeof(saved));xbuffer output={0};
    xl6_error_scope error={0};assert(xl6ErrorScopeBegin(&error)&&xrtMemDebugFailAfter(0));
    XL6_ProductI64U64V1 result=xl6NetSocketReceiveErrorIntoV1(socket,&output,info,64);
    assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();checked(&error,XERR_MEMORY);
    assert(result.e0==XNET_RESULT_ERROR&&!result.e1&&!output.Size&&!memcmp(info,&saved,sizeof(saved)));
    assert(xl6ErrorScopeBegin(&error));result=xl6NetSocketReceiveErrorIntoV1(socket,&output,info,64);
    #if defined(__linux__)
    assert(result.e0==XNET_RESULT_AGAIN&&xl6ErrorScopeEnd(&error).ok);
    #else
    assert(result.e0==XNET_RESULT_ERROR);checked(&error,XERR_UNSUPPORTED);
    #endif
    assert(!result.e1&&!output.Size&&!memcmp(info,&saved,sizeof(saved)));
    assert(xl6ErrorScopeBegin(&error));result=xl6NetSocketReceiveErrorIntoV1(socket,&output,info,-1);checked(&error,XERR_RANGE);
    assert(result.e0==XNET_RESULT_ERROR&&!memcmp(info,&saved,sizeof(saved)));
    xrtBufferUnit(&output);xl6NetDatagramErrorInfoDestroyV1(info);xl6NetSocketDestroyV1(socket);balanced(&before);
}
#endif
int main(void)
{
    assert(xrtMemDebugEnable(true));xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm)&&xl6ErrorScopeEnd(&warm).ok);
    const XL6_OwnershipDomainV1* domains[]={
        #include "net-datagram-domains.h"
    };
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));assert(catalog);
    #if !defined(TEST_DATAGRAM_CONTROL_ONLY)
    values(catalog);
    #endif
    #if defined(TEST_DATAGRAM_CONTROL_ONLY)||defined(TEST_DATAGRAM_INTEGRATION)
    control(4);control(6);
    #endif
    #if defined(TEST_DATAGRAM_IO)
    emptyQueue();
    #endif
    xl6OwnershipCatalogDestroyV1(catalog);
    puts("Datagram error runtime: offline snapshots/graph, typed control and empty queues, balanced; no error-producing traffic");return 0;
}
