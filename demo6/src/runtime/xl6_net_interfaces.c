#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>

/* A returned interface is one packed native allocation, not a view retaining
 * an entire OS snapshot. Addresses are independent POD copies. None of these
 * three resource families contains callbacks, language values or graph refs. */
static bool add_size(size_t* total,size_t size)
{if(size>SIZE_MAX-*total){xrtSetErrorKind(XERR_RANGE);return false;}*total+=size;return true;}
static bool address_valid(const xnetinterfaceaddress* value)
{
    if(!value || (value->Address.Family!=XNET_FAMILY_IPV4 && value->Address.Family!=XNET_FAMILY_IPV6) ||
        (value->PrefixLength!=XNET_INTERFACE_PREFIX_UNKNOWN &&
            value->PrefixLength>(value->Address.Family==XNET_FAMILY_IPV4?32:128))){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    return true;
}
void* xl6NetInterfaceAddressCopyV1(const xnetinterfaceaddress* value)
{
    if(!address_valid(value))return NULL;
    xnetinterfaceaddress* result=xrtMalloc(sizeof(*result));if(result)*result=*value;return result;
}
void* xl6NetInterfaceCopyV1(const xnetinterface* value)
{
    if(!value || (value->Name.Size&&!value->Name.Data) || (value->DisplayName.Size&&!value->DisplayName.Data) ||
        (value->HardwareAddress.Size&&!value->HardwareAddress.Data) || (value->AddressCount&&!value->Addresses)){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    if(value->AddressCount>INT64_MAX || value->AddressCount>SIZE_MAX/sizeof(xnetinterfaceaddress)){
        xrtSetErrorKind(XERR_RANGE);return NULL;
    }
    size_t addresses=value->AddressCount*sizeof(xnetinterfaceaddress),size=sizeof(*value);
    if(!add_size(&size,addresses)||!add_size(&size,value->Name.Size)||!add_size(&size,1)||
        !add_size(&size,value->DisplayName.Size)||!add_size(&size,1)||!add_size(&size,value->HardwareAddress.Size))return NULL;
    for(size_t n=0;n<value->AddressCount;++n)if(!address_valid(&value->Addresses[n]))return NULL;
    xnetinterface* result=xrtMalloc(size);if(!result)return NULL;*result=*value;
    _Static_assert(sizeof(xnetinterface)%_Alignof(xnetinterfaceaddress)==0,"packed address alignment");
    char* cursor=(char*)(result+1);result->Addresses=(const xnetinterfaceaddress*)cursor;
    if(addresses)memcpy(cursor,value->Addresses,addresses);
    cursor+=addresses;
    result->Name=(xstrview){cursor,value->Name.Size};
    if(value->Name.Size)memcpy(cursor,value->Name.Data,value->Name.Size);
    cursor+=value->Name.Size;*cursor++=0;
    result->DisplayName=(xstrview){cursor,value->DisplayName.Size};
    if(value->DisplayName.Size)memcpy(cursor,value->DisplayName.Data,value->DisplayName.Size);
    cursor+=value->DisplayName.Size;*cursor++=0;
    result->HardwareAddress=(xbytesview){(const uint8*)cursor,value->HardwareAddress.Size};
    if(value->HardwareAddress.Size)memcpy(cursor,value->HardwareAddress.Data,value->HardwareAddress.Size);
    return result;
}
void* xl6NetInterfacesCreateV1(void)
{
    xnetinterfacelist* list=xrtCalloc(1,sizeof(*list));if(!list)return NULL;
    if(!xrtNetInterfaces(list)){xrtFree(list);return NULL;}
    if(list->Count>INT64_MAX){xrtNetInterfacesFree(list);xrtFree(list);xrtSetErrorKind(XERR_RANGE);return NULL;}
    return list;
}
void xl6NetInterfacesDestroyV1(void* raw)
{if(raw){xrtNetInterfacesFree(raw);xrtFree(raw);}}
int64_t xl6NetInterfacesCountV1(void* raw){return raw?(int64_t)((const xnetinterfacelist*)raw)->Count:0;}
void* xl6NetInterfacesGetV1(void* raw,int64_t index)
{
    const xnetinterfacelist* list=raw;if(!list){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if(index<0 || (uint64_t)index>=list->Count){xrtSetErrorKind(XERR_RANGE);return NULL;}
    return xl6NetInterfaceCopyV1(&list->Items[index]);
}
void xl6NetInterfaceDestroyV1(void* raw){xrtFree(raw);}
int64_t xl6NetInterfacesLengthV1(void* raw){return xl6NetInterfacesCountV1(raw);}
void* xl6NetInterfacesItemV1(void* raw,int64_t index){return xl6NetInterfacesGetV1(raw,index);}
int64_t xl6NetInterfaceIPv4IndexV1(void* raw){return raw?((const xnetinterface*)raw)->IPv4Index:0;}
int64_t xl6NetInterfaceIPv6IndexV1(void* raw){return raw?((const xnetinterface*)raw)->IPv6Index:0;}
int64_t xl6NetInterfaceFlagsV1(void* raw){return raw?((const xnetinterface*)raw)->Flags:0;}
int64_t xl6NetInterfaceMtuV1(void* raw){return raw?((const xnetinterface*)raw)->Mtu:0;}
int64_t xl6NetInterfaceAddressCountV1(void* raw){return raw?(int64_t)((const xnetinterface*)raw)->AddressCount:0;}
bool xl6NetInterfaceHasFlagV1(void* raw,int64_t flag)
{return raw&&flag>=0&&(uint64_t)flag<=UINT32_MAX&&(((const xnetinterface*)raw)->Flags&(uint32)flag)==(uint32)flag;}
bool xl6NetInterfaceIsUpV1(void* raw){return xl6NetInterfaceHasFlagV1(raw,XNET_INTERFACE_UP);}
bool xl6NetInterfaceIsRunningV1(void* raw){return xl6NetInterfaceHasFlagV1(raw,XNET_INTERFACE_RUNNING);}
bool xl6NetInterfaceIsLoopbackV1(void* raw){return xl6NetInterfaceHasFlagV1(raw,XNET_INTERFACE_LOOPBACK);}
bool xl6NetInterfaceSupportsBroadcastV1(void* raw){return xl6NetInterfaceHasFlagV1(raw,XNET_INTERFACE_BROADCAST);}
bool xl6NetInterfaceIsPointToPointV1(void* raw){return xl6NetInterfaceHasFlagV1(raw,XNET_INTERFACE_POINT_TO_POINT);}
bool xl6NetInterfaceSupportsMulticastV1(void* raw){return xl6NetInterfaceHasFlagV1(raw,XNET_INTERFACE_MULTICAST);}
xvalue* xl6NetInterfaceHardwareV1(void* raw)
{if(!raw){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}return xrtValueBytes(((const xnetinterface*)raw)->HardwareAddress);}
void* xl6NetInterfaceAddressV1(void* raw,int64_t index)
{
    const xnetinterface* value=raw;if(!value){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if(index<0 || (uint64_t)index>=value->AddressCount){xrtSetErrorKind(XERR_RANGE);return NULL;}
    return xl6NetInterfaceAddressCopyV1(&value->Addresses[index]);
}
void xl6NetInterfaceAddressDestroyV1(void* raw){xrtFree(raw);}
int64_t xl6NetInterfaceAddressFamilyV1(void* raw){return raw?((const xnetinterfaceaddress*)raw)->Address.Family:0;}
int64_t xl6NetInterfaceAddressScopeV1(void* raw){return raw?((const xnetinterfaceaddress*)raw)->Address.Scope:0;}
int64_t xl6NetInterfaceAddressPrefixV1(void* raw){return raw?((const xnetinterfaceaddress*)raw)->PrefixLength:XNET_INTERFACE_PREFIX_UNKNOWN;}
bool xl6NetInterfaceAddressPrefixKnownV1(void* raw){return raw&&((const xnetinterfaceaddress*)raw)->PrefixLength!=XNET_INTERFACE_PREFIX_UNKNOWN;}
void* xl6NetInterfaceAddressValueV1(void* raw)
{if(!raw){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}return xl6NetAddressCopyV1(&((const xnetinterfaceaddress*)raw)->Address);}
xstrview xl6NetInterfaceAddressTextV1(void* raw)
{if(!raw){xrtSetErrorKind(XERR_ARGUMENT);return (xstrview){NULL,0};}return xl6NetAddressTextV1(&((xnetinterfaceaddress*)raw)->Address);}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetInterfacesHandlePolicyV1Get,xl6NetInterfacesDestroyV1)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetInterfaceHandlePolicyV1Get,xl6NetInterfaceDestroyV1)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetInterfaceAddressHandlePolicyV1Get,xl6NetInterfaceAddressDestroyV1)
