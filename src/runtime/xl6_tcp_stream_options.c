#include "xl6_tcp_options_internal.h"
void* xl6TcpStreamOptionsCopyV1(const xnetstreamconfig* value)
{
    if(!value||!xrtNetStreamConfigValid(value)){xl6TcpOptionsFailedV1();return NULL;}
    xnetstreamconfig* result=xrtMalloc(sizeof(*result));
    if(result)*result=*value;else xl6TcpOptionsFailedV1();return result;
}
void* xl6TcpStreamOptionsCreateV1(void)
{xnetstreamconfig value;xrtNetStreamConfigInit(&value);return xl6TcpStreamOptionsCopyV1(&value);}
void xl6TcpStreamOptionsDestroyV1(void* raw){xrtFree(raw);}
#define XL6_TCP_STREAM_SIZE(Field,Name) uint64_t xl6TcpStreamOptions##Field##V1(const void* raw){return ((const xnetstreamconfig*)raw)->Field;}
#define XL6_TCP_STREAM_BOOL(Field,Name) bool xl6TcpStreamOptions##Field##V1(const void* raw){return ((const xnetstreamconfig*)raw)->Field;}
#include "xl6_tcp_options_fields.inc"
#undef XL6_TCP_STREAM_SIZE
#undef XL6_TCP_STREAM_BOOL
int64_t xl6TcpStreamOptionsConnectTimeoutV1(const void* raw)
{uint64_t value=((const xnetstreamconfig*)raw)->ConnectTimeout;return value?xl6TcpOptionsMillisV1(value):-1;}
int64_t xl6TcpStreamOptionsReadModeV1(const void* raw){return ((const xnetstreamconfig*)raw)->ReadMode;}
void* xl6TcpStreamOptionsWithReadBufferV1(const void* raw,uint64_t chunk,uint64_t limit)
{
    if(!xl6TcpOptionsSizeV1(chunk)||!xl6TcpOptionsSizeV1(limit))return NULL;
    xnetstreamconfig value=*(const xnetstreamconfig*)raw;value.ReadSize=(size_t)chunk;value.ReadLimit=(size_t)limit;
    return xl6TcpStreamOptionsCopyV1(&value);
}
void* xl6TcpStreamOptionsWithWriteWatermarksV1(const void* raw,uint64_t high,uint64_t low,uint64_t limit)
{
    if(!xl6TcpOptionsSizeV1(high)||!xl6TcpOptionsSizeV1(low)||!xl6TcpOptionsSizeV1(limit))return NULL;
    xnetstreamconfig value=*(const xnetstreamconfig*)raw;value.WriteHighWater=(size_t)high;
    value.WriteLowWater=(size_t)low;value.WriteLimit=(size_t)limit;return xl6TcpStreamOptionsCopyV1(&value);
}
void* xl6TcpStreamOptionsWithConnectTimeoutV1(const void* raw,int64_t millis)
{uint64_t micros;if(!xl6TcpOptionsTimeoutV1(millis,&micros))return NULL;xnetstreamconfig value=*(const xnetstreamconfig*)raw;value.ConnectTimeout=micros;return xl6TcpStreamOptionsCopyV1(&value);}
void* xl6TcpStreamOptionsWithReadModeV1(const void* raw,int64_t mode)
{
    if(!xl6TcpOptionsRangeV1(mode>=XNET_STREAM_READ_ADAPTIVE&&mode<=XNET_STREAM_READ_PROBE))return NULL;
    xnetstreamconfig value=*(const xnetstreamconfig*)raw;value.ReadMode=(xnetstreamreadmode)mode;return xl6TcpStreamOptionsCopyV1(&value);
}
void* xl6TcpStreamOptionsWithNoDelayV1(const void* raw,bool enabled)
{xnetstreamconfig value=*(const xnetstreamconfig*)raw;value.NoDelay=enabled;return xl6TcpStreamOptionsCopyV1(&value);}
void* xl6TcpStreamOptionsWithKeepAliveV1(const void* raw,bool enabled)
{xnetstreamconfig value=*(const xnetstreamconfig*)raw;value.KeepAlive=enabled;return xl6TcpStreamOptionsCopyV1(&value);}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TcpStreamOptionsHandlePolicyV1Get,xl6TcpStreamOptionsDestroyV1)
