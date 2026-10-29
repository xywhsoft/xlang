#include "xl6_tls_parameters_internal.h"
void xl6TlsLimitsDestroyV1(void* raw){xrtFree(raw);}
void* xl6TlsLimitsCopyV1(const xtlslimits* value)
{
    if(xrtGetError())return NULL;
    if(!value){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS limits");return NULL;}
    if(value->FeedLimit>INT64_MAX || value->SendLimit>INT64_MAX || value->PlainLimit>INT64_MAX || value->HandshakeLimit>INT64_MAX){
        xl6TlsFailV1(XERR_RANGE,"TLS limit exceeds language integer range");return NULL;
    }
    if(!xrtTlsLimitsValid(value))return NULL;
    xtlslimits* result=xrtMalloc(sizeof(*result));if(result)*result=*value;return result;
}
void* xl6TlsLimitsCreateV1(int64_t feed,int64_t send,int64_t plain,int64_t handshake,int64_t records,int64_t messages)
{
    if(xrtGetError())return NULL;
    if(feed<0 || send<0 || plain<0 || handshake<0 || records<=0 || messages<=0 ||
        (uint64_t)feed>SIZE_MAX || (uint64_t)send>SIZE_MAX || (uint64_t)plain>SIZE_MAX ||
        (uint64_t)handshake>SIZE_MAX || (uint64_t)records>UINT32_MAX || (uint64_t)messages>UINT32_MAX){
        xl6TlsFailV1(XERR_RANGE,"TLS queue or drive budget exceeds its native range");return NULL;
    }
    xtlslimits value={(size_t)feed,(size_t)send,(size_t)plain,(size_t)handshake,(uint32)records,(uint32)messages};
    return xl6TlsLimitsCopyV1(&value);
}
static const xtlslimits* checked(void* raw)
{if(!raw)xl6TlsFailV1(XERR_ARGUMENT,"missing TLS limits");return raw;}
#define XL6_TLS_LIMIT(Field,Name) int64_t xl6TlsLimits##Field##V1(void* raw){const xtlslimits* p=checked(raw);return p?(int64_t)p->Field:0;}
#include "xl6_tls_limits_fields.inc"
#undef XL6_TLS_LIMIT
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsLimitsHandlePolicyV1Get,xl6TlsLimitsDestroyV1)
