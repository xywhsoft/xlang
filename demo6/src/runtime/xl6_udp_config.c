#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
static void failed(void){xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
static bool range(bool valid){if(valid)return true;xrtSetErrorKind(XERR_RANGE);failed();return false;}
static bool sizeRange(uint64_t number){return range(number<=SIZE_MAX);}
/* One authoritative native validator for both immutable language copies and
 * the actual socket factory. No Engine, service, callback or address owner. */
static void* xl6UdpConfigCopyV1(const xnetudpconfig* value)
{
    if(!xrtNetUdpConfigValid(value)){failed();return NULL;}
    xnetudpconfig* result=xrtMalloc(sizeof(*result));if(result)*result=*value;else failed();return result;
}
void* xl6UdpConfigCreateV1(void){xnetudpconfig value;xrtNetUdpConfigInit(&value);return xl6UdpConfigCopyV1(&value);}
void xl6UdpConfigDestroyV1(void* raw){xrtFree(raw);}
#define XL6_UDP_CONFIG_SIZE(Field,Name) uint64_t xl6UdpConfig##Field##V1(const void* raw){return ((const xnetudpconfig*)raw)->Field;}
#define XL6_UDP_CONFIG_UINT(Field,Name) XL6_UDP_CONFIG_SIZE(Field,Name)
#define XL6_UDP_CONFIG_INT(Field,Name) int64_t xl6UdpConfig##Field##V1(const void* raw){return ((const xnetudpconfig*)raw)->Field;}
#define XL6_UDP_CONFIG_BOOL(Field,Name) bool xl6UdpConfig##Field##V1(const void* raw){return ((const xnetudpconfig*)raw)->Field;}
#include "xl6_udp_config_fields.h"
#undef XL6_UDP_CONFIG_SIZE
#undef XL6_UDP_CONFIG_UINT
#undef XL6_UDP_CONFIG_INT
#undef XL6_UDP_CONFIG_BOOL
int64_t xl6UdpConfigReceiveMetadataV1(const void* raw){return ((const xnetudpconfig*)raw)->ReceiveMeta;}
int64_t xl6UdpConfigOverflowV1(const void* raw){return ((const xnetudpconfig*)raw)->Overflow;}
int64_t xl6UdpConfigTruncationV1(const void* raw){return ((const xnetudpconfig*)raw)->Truncation;}
int64_t xl6UdpConfigErrorOverflowV1(const void* raw){return ((const xnetudpconfig*)raw)->ErrorOverflow;}
int64_t xl6UdpConfigPathMtuV1(const void* raw){return ((const xnetudpconfig*)raw)->PathMtu;}
void* xl6UdpConfigWithReceiveV1(const void* raw,uint64_t size,uint64_t concurrency,uint64_t batch)
{
    if(!sizeRange(size)||!range(concurrency<=UINT32_MAX&&batch<=UINT32_MAX))return NULL;
    xnetudpconfig value=*(const xnetudpconfig*)raw;value.ReceiveSize=(size_t)size;
    value.ReceiveConcurrency=(uint32_t)concurrency;value.ReceiveBatch=(uint32_t)batch;return xl6UdpConfigCopyV1(&value);
}
void* xl6UdpConfigWithReceiveQueueV1(const void* raw,uint64_t packets,uint64_t bytes,int64_t overflow)
{
    if(!sizeRange(packets)||!sizeRange(bytes)||!range(overflow>=XNET_UDP_DROP_NEWEST&&overflow<=XNET_UDP_DROP_ERROR))return NULL;
    xnetudpconfig value=*(const xnetudpconfig*)raw;value.ReceiveQueueLimit=(size_t)packets;
    value.ReceiveQueueByteLimit=(size_t)bytes;value.Overflow=(xnetudpoverflow)overflow;return xl6UdpConfigCopyV1(&value);
}
void* xl6UdpConfigWithReceiveMetadataV1(const void* raw,int64_t flags)
{
    if(!range(flags>=0&&(uint64_t)flags<=UINT32_MAX))return NULL;
    xnetudpconfig value=*(const xnetudpconfig*)raw;value.ReceiveMeta=(uint32_t)flags;return xl6UdpConfigCopyV1(&value);
}
void* xl6UdpConfigWithTruncationV1(const void* raw,int64_t mode)
{
    if(!range(mode>=XNET_UDP_TRUNCATE_DELIVER&&mode<=XNET_UDP_TRUNCATE_ERROR))return NULL;
    xnetudpconfig value=*(const xnetudpconfig*)raw;value.Truncation=(xnetudptruncation)mode;return xl6UdpConfigCopyV1(&value);
}
void* xl6UdpConfigWithSendQueueV1(const void* raw,uint64_t high,uint64_t low,uint64_t limit,uint64_t packets)
{
    if(!sizeRange(high)||!sizeRange(low)||!sizeRange(limit)||!sizeRange(packets))return NULL;
    xnetudpconfig value=*(const xnetudpconfig*)raw;value.SendHighWater=(size_t)high;value.SendLowWater=(size_t)low;
    value.SendLimit=(size_t)limit;value.SendPacketLimit=(size_t)packets;return xl6UdpConfigCopyV1(&value);
}
void* xl6UdpConfigWithSendConcurrencyV1(const void* raw,uint64_t count)
{
    if(!range(count<=UINT32_MAX))return NULL;
    xnetudpconfig value=*(const xnetudpconfig*)raw;value.SendConcurrency=(uint32_t)count;return xl6UdpConfigCopyV1(&value);
}
void* xl6UdpConfigWithSocketBuffersV1(const void* raw,int64_t receive,int64_t send)
{
    if(!range(receive>=INT_MIN&&receive<=INT_MAX&&send>=INT_MIN&&send<=INT_MAX))return NULL;
    xnetudpconfig value=*(const xnetudpconfig*)raw;value.ReceiveBuffer=(int)receive;value.SendBuffer=(int)send;return xl6UdpConfigCopyV1(&value);
}
void* xl6UdpConfigWithTrafficV1(const void* raw,int64_t hop,int64_t traffic)
{
    if(!range(hop>=INT_MIN&&hop<=INT_MAX&&traffic>=INT_MIN&&traffic<=INT_MAX))return NULL;
    xnetudpconfig value=*(const xnetudpconfig*)raw;value.HopLimit=(int)hop;value.TrafficClass=(int)traffic;return xl6UdpConfigCopyV1(&value);
}
void* xl6UdpConfigWithPathMtuV1(const void* raw,int64_t mode)
{
    if(!range(mode>=XNET_PMTU_SYSTEM&&mode<=XNET_PMTU_PROBE))return NULL;
    xnetudpconfig value=*(const xnetudpconfig*)raw;value.PathMtu=(xnetpmtumode)mode;return xl6UdpConfigCopyV1(&value);
}
void* xl6UdpConfigWithBindingV1(const void* raw,bool reuse,bool reusePort,bool exclusive,bool ipv6Only)
{
    xnetudpconfig value=*(const xnetudpconfig*)raw;value.ReuseAddress=reuse;value.ReusePort=reusePort;
    value.ExclusiveAddress=exclusive;value.IPv6Only=ipv6Only;return xl6UdpConfigCopyV1(&value);
}
void* xl6UdpConfigWithBroadcastV1(const void* raw,bool enabled)
{xnetudpconfig value=*(const xnetudpconfig*)raw;value.Broadcast=enabled;return xl6UdpConfigCopyV1(&value);}
void* xl6UdpConfigWithErrorQueueV1(const void* raw,bool enabled,uint64_t size,uint64_t packets,uint64_t bytes,int64_t overflow)
{
    if(!sizeRange(size)||!sizeRange(packets)||!sizeRange(bytes)||!range(overflow>=XNET_UDP_DROP_NEWEST&&overflow<=XNET_UDP_DROP_ERROR))return NULL;
    xnetudpconfig value=*(const xnetudpconfig*)raw;value.ReceiveErrors=enabled;value.ErrorSize=(size_t)size;
    value.ErrorQueueLimit=(size_t)packets;value.ErrorQueueByteLimit=(size_t)bytes;value.ErrorOverflow=(xnetudpoverflow)overflow;
    return xl6UdpConfigCopyV1(&value);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6UdpConfigHandlePolicyV1Get,xl6UdpConfigDestroyV1)
