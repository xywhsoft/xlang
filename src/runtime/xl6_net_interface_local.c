#include "xl6_text_value.h"
#include <string.h>
static const xstrview xl6TextFailure={NULL,0};
static xstrview owned_ascii(char* data)
{
    if(!data){if(!xrtGetError())xrtSetErrorKind(XERR_MEMORY);return xl6TextFailure;}
    size_t size=strlen(data);
    if(size>INT64_MAX){xrtFree(data);xrtSetErrorKind(XERR_RANGE);return xl6TextFailure;}
    return (xstrview){data,size};
}
static bool family_value(int64_t raw,xnetfamily* family)
{
    if(raw!=XNET_FAMILY_UNSPEC&&raw!=XNET_FAMILY_IPV4&&raw!=XNET_FAMILY_IPV6){xrtSetErrorKind(XERR_VALUE);return false;}
    *family=(xnetfamily)raw;return true;
}
void* xl6NetLocalAddressV1(int64_t rawFamily)
{
    xnetfamily family;xnetaddr address;
    return family_value(rawFamily,&family)&&xrtNetLocalAddress(&address,family)?xl6NetAddressCopyV1(&address):NULL;
}
xstrview xl6NetLocalIPV1(int64_t rawFamily)
{
    xnetfamily family;if(!family_value(rawFamily,&family))return xl6TextFailure;
    return owned_ascii(xrtNetLocalAddressString(family));
}
xstrview xl6NetLocalMACV1(void){return owned_ascii(xrtNetLocalHardwareString());}
xvalue* xl6NetLocalHardwareV1(void)
{
    size_t capacity=64;
    for(unsigned attempt=0;attempt<8;++attempt){
        bytes data=xrtMalloc(capacity);if(!data)return NULL;
        size_t size=xrtNetLocalHardware(data,capacity);
        if(size==XRT_NPOS){xrtFree(data);return NULL;}
        if(size>capacity){xrtFree(data);xrtClearError();capacity=size;continue;}
        xvalue* result=xrtValueBytesTake(&data,size);xrtFree(data);return result;
    }
    xrtSetErrorKind(XERR_STATE);return NULL;
}
