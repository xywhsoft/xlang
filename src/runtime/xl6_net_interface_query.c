#include "xl6_text_internal.h"
#include <string.h>
static bool family_value(int64_t raw,xnetfamily* family)
{
    if(raw!=XNET_FAMILY_UNSPEC&&raw!=XNET_FAMILY_IPV4&&raw!=XNET_FAMILY_IPV6){xrtSetErrorKind(XERR_VALUE);return false;}
    *family=(xnetfamily)raw;return true;
}
int64_t xl6NetInterfaceIndexV1(xstrview name,int64_t rawFamily)
{
    xnetfamily family;if(!family_value(rawFamily,&family)||!xl6TextSelect(&name,-1))return 0;
    if(!name.Size||memchr(name.Data,0,name.Size)){xrtSetErrorKind(XERR_VALUE);return 0;}
    char* data=xrtStrDupView(name);if(!data)return 0;
    uint32 index=xrtNetInterfaceIndex(data,family);xrtFree(data);return index;
}
typedef struct name_query {uint32 index;xnetfamily family;} name_query;
static size_t read_name(const name_query* query,char* data,size_t size)
{return query?xrtNetInterfaceName(query->index,query->family,data,size):xrtNetHostName(data,size);}
/* One actual read first; retry only an explicit size-growth result. Native
 * snapshots can change between calls, so the retry is bounded, not a promise
 * that a size query freezes future OS state. No half text escapes on failure. */
static xstrview owned_name(const name_query* query)
{
    size_t capacity=128;
    for(unsigned attempt=0;attempt<8;++attempt){
        char* data=xrtMalloc(capacity);if(!data)return xl6TextFailure;
        size_t size=read_name(query,data,capacity);
        if(size==XRT_NPOS){xrtFree(data);return xl6TextFailure;}
        if(size>=capacity){xrtFree(data);xrtClearError();capacity=size+1;continue;}
        xstrview text={data,size};
        if(!xl6TextSelect(&text,-1)){xrtFree(data);return xl6TextFailure;}
        return text;
    }
    xrtSetErrorKind(XERR_STATE);return xl6TextFailure;
}
xstrview xl6NetInterfaceNameByIndexV1(int64_t index,int64_t rawFamily)
{
    name_query query;if(!family_value(rawFamily,&query.family))return xl6TextFailure;
    if(index<=0||(uint64_t)index>UINT32_MAX){xrtSetErrorKind(XERR_RANGE);return xl6TextFailure;}
    query.index=(uint32)index;return owned_name(&query);
}
xstrview xl6NetHostNameV1(void){return owned_name(NULL);}
