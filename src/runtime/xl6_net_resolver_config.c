#include "xl6_net_resolver_input.h"
#include "xl6_native_handle.h"
static void* copy(const xnetresolverconfig* value)
{xnetresolverconfig* result=xrtMalloc(sizeof(*result));if(result)*result=*value;else xl6NetResolverFailedV1();return result;}
void* xl6NetResolverConfigCreateV1(void)
{xnetresolverconfig value;xrtNetResolverConfigInit(&value);return copy(&value);}
void xl6NetResolverConfigDestroyV1(void* raw){xrtFree(raw);}
int64_t xl6NetResolverConfigWorkersV1(const void* raw){return ((const xnetresolverconfig*)raw)->Workers;}
void* xl6NetResolverConfigWithWorkersV1(const void* raw,int64_t workers)
{
    if(workers<=0||(uint64_t)workers>UINT32_MAX){xrtSetErrorKind(XERR_RANGE);xl6NetResolverFailedV1();return NULL;}
    xnetresolverconfig value=*(const xnetresolverconfig*)raw;value.Workers=(uint32_t)workers;return copy(&value);
}
#define XL6_NET_RESOLVER_CONFIG_SIZE(Field,Name,Minimum) \
uint64_t xl6NetResolverConfig##Field##V1(const void* raw){return ((const xnetresolverconfig*)raw)->Field;} \
void* xl6NetResolverConfigWith##Field##V1(const void* raw,uint64_t input) \
{if(((Minimum)&&!input)||input>SIZE_MAX){xrtSetErrorKind(XERR_RANGE);xl6NetResolverFailedV1();return NULL;} \
 xnetresolverconfig value=*(const xnetresolverconfig*)raw;value.Field=(size_t)input;return copy(&value);}
#include "xl6_net_resolver_config_fields.inc"
#undef XL6_NET_RESOLVER_CONFIG_SIZE
#define TTL(Field) \
uint64_t xl6NetResolverConfig##Field##MillisV1(const void* raw) \
{uint64_t us=((const xnetresolverconfig*)raw)->Field;return us/1000+(us%1000!=0);} \
void* xl6NetResolverConfigWith##Field##MillisV1(const void* raw,uint64_t millis) \
{if(millis>UINT64_MAX/1000){xrtSetErrorKind(XERR_RANGE);xl6NetResolverFailedV1();return NULL;} \
 xnetresolverconfig value=*(const xnetresolverconfig*)raw;value.Field=millis*1000;return copy(&value);}
TTL(SuccessTTL)
TTL(FailureTTL)
#undef TTL
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetResolverConfigHandlePolicyV1Get,xl6NetResolverConfigDestroyV1)
