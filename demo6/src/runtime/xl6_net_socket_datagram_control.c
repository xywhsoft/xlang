#include "xl6_text_value.h"
#include "xl6_net_socket_owner.h"
#include "src/api/xl_foreign_error.h"
static void failed(void)
{ xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); }
/* Mutable option operations exclude I/O; provider-cache publication in the
 * pinned XRT is interlocked, so queries can share an ordinary I/O borrow. */
static bool enter(void* raw,XL6_NetSocketScopeV1* scope)
{ if(xl6NetSocketOwnerControlV1(raw,scope))return true;failed();return false; }
#define QUERY(Name,Native) \
int64_t xl6NetSocket##Name##V1(void* raw) \
{ \
    XL6_NetSocketScopeV1 scope={0}; \
    if(!xl6NetSocketOwnerEnterV1(raw,&scope)){failed();return 0;} \
    int64_t result=0; \
    if(xrtNetSocketType(scope.native)!=XNET_SOCKET_DGRAM){xrtSetErrorKind(XERR_VALUE);failed();} \
    else {result=Native(scope.native);if(xrtGetError())failed();} \
    xl6NetSocketOwnerLeaveV1(&scope);return result; \
}
QUERY(MetadataAvailable,xrtNetSocketDgramMetaAvailable)
QUERY(MetadataEnabled,xrtNetSocketDgramMetaEnabled)
QUERY(ControlAvailable,xrtNetSocketDgramControlAvailable)
QUERY(DatagramCapabilities,xrtNetSocketDgramCapabilities)
#undef QUERY
void xl6NetSocketMetadataSetV1(void* raw,int64_t flags)
{
    const uint32_t all=XNET_DGRAM_META_DESTINATION|XNET_DGRAM_META_INTERFACE|XNET_DGRAM_META_HOP_LIMIT|
        XNET_DGRAM_META_TRAFFIC_CLASS|XNET_DGRAM_META_SEGMENT_SIZE;
    if(flags<0||((uint64_t)flags&~(uint64_t)all)){xrtSetErrorKind(XERR_VALUE);failed();return;}
    XL6_NetSocketScopeV1 scope={0};if(!enter(raw,&scope))return;
    if(!xrtNetSocketDgramMetaSet(scope.native,(uint32_t)flags))failed();
    xl6NetSocketOwnerLeaveV1(&scope);
}
static void membership(void* raw,const void* group,const void* iface,bool join)
{
    XL6_NetSocketScopeV1 scope={0};if(!enter(raw,&scope))return;
    bool ok=join?xrtNetSocketMulticastJoin(scope.native,group,iface):xrtNetSocketMulticastLeave(scope.native,group,iface);
    if(!ok)failed();
    xl6NetSocketOwnerLeaveV1(&scope);
}
void xl6NetSocketMulticastJoinV1(void* r,const void* g) { membership(r,g,NULL,true); }
void xl6NetSocketMulticastJoinOnV1(void* r,const void* g,const void* i) { membership(r,g,i,true); }
void xl6NetSocketMulticastLeaveV1(void* r,const void* g) { membership(r,g,NULL,false); }
void xl6NetSocketMulticastLeaveOnV1(void* r,const void* g,const void* i) { membership(r,g,i,false); }
void xl6NetSocketMulticastLoopV1(void* raw,bool enabled)
{
    XL6_NetSocketScopeV1 scope={0};if(!enter(raw,&scope))return;
    if(!xrtNetSocketMulticastLoop(scope.native,enabled))failed();
    xl6NetSocketOwnerLeaveV1(&scope);
}
void xl6NetSocketMulticastHopLimitV1(void* raw,int64_t limit)
{
    if(limit<0||limit>255){xrtSetErrorKind(XERR_RANGE);failed();return;}
    XL6_NetSocketScopeV1 scope={0};if(!enter(raw,&scope))return;
    if(!xrtNetSocketMulticastHopLimit(scope.native,(int)limit))failed();
    xl6NetSocketOwnerLeaveV1(&scope);
}
static void multicastInterface(void* raw,const void* iface)
{
    XL6_NetSocketScopeV1 scope={0};if(!enter(raw,&scope))return;
    if(!xrtNetSocketMulticastInterface(scope.native,iface))failed();
    xl6NetSocketOwnerLeaveV1(&scope);
}
void xl6NetSocketMulticastInterfaceV1(void* raw,const void* iface) { multicastInterface(raw,iface); }
void xl6NetSocketMulticastInterfaceDefaultV1(void* raw) { multicastInterface(raw,NULL); }
void xl6NetSocketPathMtuModeSetV1(void* raw,int64_t mode)
{
    if(mode<XNET_PMTU_SYSTEM||mode>XNET_PMTU_PROBE){xrtSetErrorKind(XERR_VALUE);failed();return;}
    XL6_NetSocketScopeV1 scope={0};if(!enter(raw,&scope))return;
    if(!xrtNetSocketSet(scope.native,XNET_OPTION_PATH_MTU_MODE,mode))failed();
    xl6NetSocketOwnerLeaveV1(&scope);
}
static bool get(void* raw,xnetoption option,int64* value)
{
    XL6_NetSocketScopeV1 scope={0};
    if(!xl6NetSocketOwnerEnterV1(raw,&scope)){failed();return false;}
    bool ok=xrtNetSocketGet(scope.native,option,value);if(!ok)failed();
    xl6NetSocketOwnerLeaveV1(&scope);return ok;
}
int64_t xl6NetSocketPathMtuModeV1(void* raw)
{
    int64 value=0;if(!get(raw,XNET_OPTION_PATH_MTU_MODE,&value))return 0;
    if(value<XNET_PMTU_SYSTEM||value>XNET_PMTU_PROBE){xrtSetErrorKind(XERR_STATE);failed();return 0;}
    return value;
}
uint64_t xl6NetSocketPathMtuV1(void* raw)
{
    int64 value=0;if(!get(raw,XNET_OPTION_PATH_MTU,&value))return 0;
    if(value<0){xrtSetErrorKind(XERR_STATE);failed();return 0;}return (uint64_t)value;
}
void xl6NetSocketDatagramErrorsSetV1(void* raw,bool enabled)
{
    XL6_NetSocketScopeV1 scope={0};if(!enter(raw,&scope))return;
    if(!xrtNetSocketSet(scope.native,XNET_OPTION_DGRAM_ERRORS,enabled))failed();
    xl6NetSocketOwnerLeaveV1(&scope);
}
bool xl6NetSocketDatagramErrorsV1(void* raw)
{int64 value=0;return get(raw,XNET_OPTION_DGRAM_ERRORS,&value)&&value!=0;}
