#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>

static bool port_value(int64_t value,uint16* output)
{
    if(value<0 || value>UINT16_MAX){xrtSetErrorKind(XERR_RANGE);return false;}
    *output=(uint16)value;return true;
}
static bool family_value(int64_t value,xnetfamily* output)
{
    if(value!=XNET_FAMILY_IPV4 && value!=XNET_FAMILY_IPV6){xrtSetErrorKind(XERR_VALUE);return false;}
    *output=(xnetfamily)value;return true;
}
/* Longest numeric IPv6 (including mixed IPv4), numeric zone and endpoint fit
 * in 80 bytes. Copy a bounded view to stack; never read Data[Size] or allocate
 * a temporary heap C string. Numeric parsing cannot invoke DNS/interface-name
 * lookup, irrespective of other XRT features present in the executable. */
static bool numeric_text(xstrview input,char output[80])
{
    if(!input.Data || !input.Size || input.Size>=80 || memchr(input.Data,0,input.Size)){
        xrtSetErrorKind(XERR_VALUE);return false;
    }
    const char* zone=memchr(input.Data,'%',input.Size);
    if(zone){
        size_t at=(size_t)(zone-input.Data)+1;
        size_t first=at;
        for(;at<input.Size && input.Data[at]!=']';++at)
            if(input.Data[at]<'0' || input.Data[at]>'9'){xrtSetErrorKind(XERR_VALUE);return false;}
        if(at==first){xrtSetErrorKind(XERR_VALUE);return false;}
        if(at<input.Size){
            ++at;
            if(at<input.Size){
                if(input.Data[at++]!=':' || at==input.Size){xrtSetErrorKind(XERR_VALUE);return false;}
                for(;at<input.Size;++at)
                    if(input.Data[at]<'0' || input.Data[at]>'9'){xrtSetErrorKind(XERR_VALUE);return false;}
            }
        }
    }
    memcpy(output,input.Data,input.Size);output[input.Size]=0;return true;
}
void* xl6NetAddressCopyV1(const xnetaddr* address)
{
    if(!address || (address->Family!=XNET_FAMILY_IPV4 && address->Family!=XNET_FAMILY_IPV6)){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    xnetaddr* result=xrtMalloc(sizeof(*result));if(result)*result=*address;return result;
}
void* xl6NetAddressAnyV1(int64_t family,int64_t port)
{
    xnetfamily selected;uint16 number;xnetaddr address;
    return family_value(family,&selected) && port_value(port,&number) && xrtNetAddrAny(&address,selected,number)
        ?xl6NetAddressCopyV1(&address):NULL;
}
void* xl6NetAddressLoopbackV1(int64_t family,int64_t port)
{
    xnetfamily selected;uint16 number;xnetaddr address;
    return family_value(family,&selected) && port_value(port,&number) && xrtNetAddrLoopback(&address,selected,number)
        ?xl6NetAddressCopyV1(&address):NULL;
}
void* xl6NetAddressDefaultV1(void){return xl6NetAddressAnyV1(XNET_FAMILY_IPV4,0);}
void* xl6NetAddressParseV1(xstrview text,int64_t port)
{
    char input[80];uint16 number;xnetaddr address;
    return port_value(port,&number) && numeric_text(text,input) && xrtNetAddrParse(&address,input,number)
        ?xl6NetAddressCopyV1(&address):NULL;
}
void* xl6NetAddressEndpointV1(xstrview text,int64_t port)
{
    char input[80];uint16 number;xnetaddr address;
    return port_value(port,&number) && numeric_text(text,input) && xrtNetAddrParseEndpoint(&address,input,number)
        ?xl6NetAddressCopyV1(&address):NULL;
}
void xl6NetAddressDestroyV1(void* address){xrtFree(address);}
int64_t xl6NetAddressFamilyV1(void* address){return address?((const xnetaddr*)address)->Family:0;}
int64_t xl6NetAddressPortV1(void* address){return address?((const xnetaddr*)address)->Port:0;}
int64_t xl6NetAddressScopeV1(void* address){return address?((const xnetaddr*)address)->Scope:0;}
static xstrview text_value(void* raw,bool endpoint)
{
    if(!raw){xrtSetErrorKind(XERR_ARGUMENT);return (xstrview){NULL,0};}
    char text[80];size_t size=endpoint?xrtNetAddrEndpointText(raw,text,sizeof(text)):xrtNetAddrText(raw,text,sizeof(text));
    if(!size || size>=sizeof(text)){if(!xrtGetError())xrtSetErrorKind(XERR_INTERNAL);return (xstrview){NULL,0};}
    char* data=xrtStrDupView((xstrview){text,size});
    return (xstrview){data,data?size:0};
}
xstrview xl6NetAddressTextV1(void* raw){return text_value(raw,false);}
xstrview xl6NetAddressEndpointTextV1(void* raw){return text_value(raw,true);}
xvalue* xl6NetAddressBytesV1(void* raw)
{
    if(!raw){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    const xnetaddr* address=raw;
    return xrtValueBytes((xbytesview){address->Address,address->Family==XNET_FAMILY_IPV4?4u:16u});
}
bool xl6NetAddressEqualV1(void* a,void* b){return xrtNetAddrEqual(a,b);}
bool xl6NetAddressSameIPV1(void* a,void* b){return xrtNetAddrSameIP(a,b);}
int64_t xl6NetAddressCompareV1(void* a,void* b){return xrtNetAddrCompare(a,b);}
bool xl6NetAddressIsUnspecifiedV1(void* a){return xrtNetAddrIsUnspecified(a);}
bool xl6NetAddressIsLoopbackV1(void* a){return xrtNetAddrIsLoopback(a);}
bool xl6NetAddressIsMulticastV1(void* a){return xrtNetAddrIsMulticast(a);}
bool xl6NetAddressIsLinkLocalV1(void* a){return xrtNetAddrIsLinkLocal(a);}
bool xl6NetAddressIsPrivateV1(void* a){return xrtNetAddrIsPrivate(a);}
bool xl6NetAddressIsMappedV1(void* a){return xrtNetAddrIsMapped(a);}
void* xl6NetAddressUnmapV1(void* a)
{xnetaddr value;return xrtNetAddrUnmap(a,&value)?xl6NetAddressCopyV1(&value):NULL;}
void* xl6NetAddressWithPortV1(void* raw,int64_t port)
{
    uint16 number;if(!raw){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if(!port_value(port,&number))return NULL;
    xnetaddr value=*(const xnetaddr*)raw;value.Port=number;return xl6NetAddressCopyV1(&value);
}
void* xl6NetAddressWithScopeV1(void* raw,int64_t scope)
{
    if(!raw){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    const xnetaddr* source=raw;
    if(scope<0 || (uint64_t)scope>UINT32_MAX){xrtSetErrorKind(XERR_RANGE);return NULL;}
    if(source->Family!=XNET_FAMILY_IPV6 && scope){xrtSetErrorKind(XERR_VALUE);return NULL;}
    xnetaddr value=*source;value.Scope=(uint32)scope;return xl6NetAddressCopyV1(&value);
}
/* Exact immutable POD leaf: no native reference, callbacks, language values,
 * retained input text, scope-name cache, Engine or Resolver is hidden here. */
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetAddressHandlePolicyV1Get,xl6NetAddressDestroyV1)
