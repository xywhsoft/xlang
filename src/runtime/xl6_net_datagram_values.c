#include "xl6_net_datagram_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>

static void failed(void)
{ xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); }
static void invalid(xerrkind kind) { xrtSetErrorKind(kind); failed(); }
void* xl6NetDatagramInfoCreateV1(void)
{
    void* result=xrtCalloc(1,sizeof(XL6_NetDatagramInfoV1));
    if(!result)failed();
    return result;
}
void xl6NetDatagramInfoDestroyV1(void* raw) { xrtFree(raw); }
void xl6NetDatagramInfoClearV1(void* raw)
{ if(raw)memset(raw,0,sizeof(XL6_NetDatagramInfoV1)); }
bool xl6NetDatagramInfoValidV1(const void* raw)
{ return raw&&((const XL6_NetDatagramInfoV1*)raw)->valid; }
int64_t xl6NetDatagramInfoFlagsV1(const void* raw)
{ return xl6NetDatagramInfoValidV1(raw)?((const XL6_NetDatagramInfoV1*)raw)->meta.Flags:0; }
static const XL6_NetDatagramInfoV1* info(const void* raw,uint32_t flag)
{
    const XL6_NetDatagramInfoV1* value=raw;
    if(!value){invalid(XERR_ARGUMENT);return NULL;}
    if(!value->valid||(flag&&!(value->meta.Flags&flag))){invalid(XERR_STATE);return NULL;}
    return value;
}
static void* copyAddress(const xnetaddr* address)
{
    void* result=xl6NetAddressCopyV1(address);
    if(!result)failed();
    return result;
}
void* xl6NetDatagramInfoRemoteV1(const void* raw)
{ const XL6_NetDatagramInfoV1* v=info(raw,0);return v?copyAddress(&v->remote):NULL; }
void* xl6NetDatagramInfoDestinationV1(const void* raw)
{ const XL6_NetDatagramInfoV1* v=info(raw,XNET_DGRAM_META_DESTINATION);return v?copyAddress(&v->meta.Destination):NULL; }
#define INFO_FIELD(Name,Field,Flag) \
int64_t xl6NetDatagramInfo##Name##V1(const void* raw) \
{ const XL6_NetDatagramInfoV1* v=info(raw,Flag);return v?v->meta.Field:0; }
INFO_FIELD(Interface,Interface,XNET_DGRAM_META_INTERFACE)
INFO_FIELD(HopLimit,HopLimit,XNET_DGRAM_META_HOP_LIMIT)
INFO_FIELD(TrafficClass,TrafficClass,XNET_DGRAM_META_TRAFFIC_CLASS)
INFO_FIELD(SegmentSize,SegmentSize,XNET_DGRAM_META_SEGMENT_SIZE)
#undef INFO_FIELD

void* xl6NetDatagramControlCreateV1(void)
{
    void* result=xrtCalloc(1,sizeof(xnetdgramcontrol));if(!result)failed();return result;
}
void xl6NetDatagramControlDestroyV1(void* raw) { xrtFree(raw); }
int64_t xl6NetDatagramControlFlagsV1(const void* raw)
{ return raw?((const xnetdgramcontrol*)raw)->Flags:0; }
static xnetdgramcontrol* controlCopy(const void* raw)
{
    if(!raw){invalid(XERR_ARGUMENT);return NULL;}
    xnetdgramcontrol* result=xrtMalloc(sizeof(*result));
    if(result)*result=*(const xnetdgramcontrol*)raw;else failed();
    return result;
}
void* xl6NetDatagramControlSourceV1(const void* raw,const void* address)
{
    const xnetaddr* source=address;
    if(!source||source->Port||(source->Family!=XNET_FAMILY_IPV4&&source->Family!=XNET_FAMILY_IPV6)||xrtNetAddrIsUnspecified(source)){
        invalid(XERR_VALUE);return NULL;
    }
    xnetdgramcontrol* result=controlCopy(raw);
    if(result){result->Source=*source;result->Flags|=XNET_DGRAM_CONTROL_SOURCE;}
    return result;
}
#define CONTROL_FIELD(Name,Field,Flag,Minimum,Maximum) \
void* xl6NetDatagramControl##Name##V1(const void* raw,int64_t number) \
{ \
    if(number<(Minimum)||(uint64_t)number>(uint64_t)(Maximum)){invalid(XERR_RANGE);return NULL;} \
    xnetdgramcontrol* result=controlCopy(raw); \
    if(result){result->Field=number;result->Flags|=Flag;} \
    return result; \
}
CONTROL_FIELD(Interface,Interface,XNET_DGRAM_CONTROL_INTERFACE,0,UINT32_MAX)
CONTROL_FIELD(HopLimit,HopLimit,XNET_DGRAM_CONTROL_HOP_LIMIT,0,255)
CONTROL_FIELD(TrafficClass,TrafficClass,XNET_DGRAM_CONTROL_TRAFFIC_CLASS,0,255)
CONTROL_FIELD(SegmentSize,SegmentSize,XNET_DGRAM_CONTROL_SEGMENT_SIZE,1,UINT16_MAX)
#undef CONTROL_FIELD
void* xl6NetDatagramControlWithoutV1(const void* raw,int64_t flags)
{
    const uint32_t all=XNET_DGRAM_CONTROL_SOURCE|XNET_DGRAM_CONTROL_INTERFACE|
        XNET_DGRAM_CONTROL_HOP_LIMIT|XNET_DGRAM_CONTROL_TRAFFIC_CLASS|XNET_DGRAM_CONTROL_SEGMENT_SIZE;
    if(flags<0||((uint64_t)flags&~(uint64_t)all)){invalid(XERR_VALUE);return NULL;}
    xnetdgramcontrol* result=controlCopy(raw);
    if(result){
        result->Flags&=~(uint32_t)flags;
        if(flags&XNET_DGRAM_CONTROL_SOURCE)memset(&result->Source,0,sizeof(result->Source));
        if(flags&XNET_DGRAM_CONTROL_INTERFACE)result->Interface=0;
        if(flags&XNET_DGRAM_CONTROL_HOP_LIMIT)result->HopLimit=0;
        if(flags&XNET_DGRAM_CONTROL_TRAFFIC_CLASS)result->TrafficClass=0;
        if(flags&XNET_DGRAM_CONTROL_SEGMENT_SIZE)result->SegmentSize=0;
    }
    return result;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetDatagramInfoHandlePolicyV1Get,xl6NetDatagramInfoDestroyV1)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetDatagramControlHandlePolicyV1Get,xl6NetDatagramControlDestroyV1)
