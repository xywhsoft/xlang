#include "xl6_net_engine_config_internal.h"
static XL6_NetEngineConfigV1* publish(XL6_NetEngineConfigV1* input)
{
    xl6NetEngineConfigRebindV1(input);
    if(!xrtNetEngineConfigValidate(&input->value)){xl6NetEngineFailedV1();return NULL;}
    XL6_NetEngineConfigV1* result=xrtMalloc(sizeof(*result));
    if(result){*result=*input;xl6NetEngineConfigRebindV1(result);}else xl6NetEngineFailedV1();
    return result;
}
static const XL6_NetEngineConfigV1* get(const void* raw)
{if(!raw){xrtSetErrorKind(XERR_ARGUMENT);xl6NetEngineFailedV1();}return raw;}
static bool size(uint64_t value)
{if(value<=SIZE_MAX)return true;xrtSetErrorKind(XERR_RANGE);xl6NetEngineFailedV1();return false;}
static bool index(int64_t value)
{if(value>=0&&value<XNET_BUFFER_CLASS_COUNT)return true;xrtSetErrorKind(XERR_RANGE);xl6NetEngineFailedV1();return false;}
void* xl6NetEngineConfigCreateV1(void)
{
    XL6_NetEngineConfigV1 config;xrtNetEngineConfigInit(&config.value);xrtNetBufPoolConfigInit(&config.buffers);
    return publish(&config);
}
void xl6NetEngineConfigDestroyV1(void* raw){xrtFree(raw);}
int64_t xl6NetEngineConfigBackendV1(const void* raw){return ((const XL6_NetEngineConfigV1*)raw)->value.Backend;}
void* xl6NetEngineConfigWithBackendV1(const void* raw,int64_t backend)
{
    const XL6_NetEngineConfigV1* c=get(raw);if(!c)return NULL;
    if(backend<XNET_PORT_AUTO||backend>XNET_PORT_SELECT){xrtSetErrorKind(XERR_RANGE);xl6NetEngineFailedV1();return NULL;}
    XL6_NetEngineConfigV1 value=*c;value.value.Backend=(xnetportbackend)backend;return publish(&value);
}
int64_t xl6NetEngineConfigWorkersV1(const void* raw){return ((const XL6_NetEngineConfigV1*)raw)->value.Workers;}
void* xl6NetEngineConfigWithWorkersV1(const void* raw,int64_t workers)
{
    const XL6_NetEngineConfigV1* c=get(raw);if(!c)return NULL;
    if(workers<0||(uint64_t)workers>UINT32_MAX){xrtSetErrorKind(XERR_RANGE);xl6NetEngineFailedV1();return NULL;}
    XL6_NetEngineConfigV1 value=*c;value.value.Workers=(uint32_t)workers;return publish(&value);
}
int64_t xl6NetEngineConfigIdleWaitV1(const void* raw)
{
    const XL6_NetEngineConfigV1* c=raw;uint64_t micros=c->value.IdleWait;
    /* Native-host-created engines may use sub-ms values. The copied native
     * config remains exact; this language unit view rounds positive waits up. */
    return micros?(int64_t)(micros/1000+(micros%1000!=0)):-1;
}
void* xl6NetEngineConfigWithIdleWaitV1(const void* raw,int64_t millis)
{
    const XL6_NetEngineConfigV1* c=get(raw);if(!c)return NULL;
    /* Pinned native zero means infinite, not a poll. No truncation or finite
     * saturation into infinity. Language permits positive ms or -1 only. */
    if((millis<=0&&millis!=-1)||(millis>0&&(uint64_t)millis>UINT64_MAX/1000)){
        xrtSetErrorKind(XERR_RANGE);xl6NetEngineFailedV1();return NULL;}
    XL6_NetEngineConfigV1 value=*c;value.value.IdleWait=millis==-1?0:(uint64_t)millis*1000;return publish(&value);
}
uint64_t xl6NetEngineConfigBufferBudgetV1(const void* raw){return ((const XL6_NetEngineConfigV1*)raw)->buffers.MaxCacheBytes;}
void* xl6NetEngineConfigWithBufferBudgetV1(const void* raw,uint64_t bytes)
{const XL6_NetEngineConfigV1* c=get(raw);if(!c||!size(bytes))return NULL;XL6_NetEngineConfigV1 value=*c;value.buffers.MaxCacheBytes=(size_t)bytes;return publish(&value);}
uint64_t xl6NetEngineConfigBufferBlockSizeV1(const void* raw,int64_t item)
{const XL6_NetEngineConfigV1* c=get(raw);return c&&index(item)?c->buffers.BlockSize[item]:0;}
uint64_t xl6NetEngineConfigBufferCacheLimitV1(const void* raw,int64_t item)
{const XL6_NetEngineConfigV1* c=get(raw);return c&&index(item)?c->buffers.CacheLimit[item]:0;}
void* xl6NetEngineConfigWithBufferClassV1(const void* raw,int64_t item,uint64_t bytes,uint64_t limit)
{
    const XL6_NetEngineConfigV1* c=get(raw);if(!c||!index(item)||!size(bytes)||!size(limit))return NULL;
    XL6_NetEngineConfigV1 value=*c;value.buffers.BlockSize[item]=(size_t)bytes;value.buffers.CacheLimit[item]=(size_t)limit;return publish(&value);
}
#define XL6_NET_ENGINE_CONFIG_SIZE(Field,Name) \
uint64_t xl6NetEngineConfig##Field##V1(const void* raw){return ((const XL6_NetEngineConfigV1*)raw)->value.Field;} \
void* xl6NetEngineConfigWith##Field##V1(const void* raw,uint64_t item) \
{const XL6_NetEngineConfigV1* c=get(raw);if(!c||!size(item))return NULL;XL6_NetEngineConfigV1 value=*c;value.value.Field=(size_t)item;return publish(&value);}
#include "xl6_net_engine_config_fields.inc"
#undef XL6_NET_ENGINE_CONFIG_SIZE
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetEngineConfigHandlePolicyV1Get,xl6NetEngineConfigDestroyV1)
