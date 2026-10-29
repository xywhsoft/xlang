#include "xl6_tcp_options_internal.h"
static void* publish(const xnetdialconfig* value)
{
    if(!xrtNetDialConfigValid(value)){xl6TcpOptionsFailedV1();return NULL;}
    xnetdialconfig* result=xrtMalloc(sizeof(*result));if(result)*result=*value;else xl6TcpOptionsFailedV1();return result;
}
void* xl6TcpDialOptionsCreateV1(void)
{xnetdialconfig value;xrtNetDialConfigInit(&value);return publish(&value);}
void* xl6TcpDialOptionsCopyV1(const xnetdialconfig* value){return publish(value);}
void xl6TcpDialOptionsDestroyV1(void* raw){xrtFree(raw);}
void* xl6TcpDialOptionsStreamV1(const void* raw){return xl6TcpStreamOptionsCopyV1(&((const xnetdialconfig*)raw)->Stream);}
void* xl6TcpDialOptionsWithStreamV1(const void* raw,const void* stream)
{xnetdialconfig value=*(const xnetdialconfig*)raw;value.Stream=*(const xnetstreamconfig*)stream;return publish(&value);}
int64_t xl6TcpDialOptionsFamilyV1(const void* raw){return ((const xnetdialconfig*)raw)->Family;}
uint64_t xl6TcpDialOptionsAffinityV1(const void* raw){return ((const xnetdialconfig*)raw)->Affinity;}
int64_t xl6TcpDialOptionsTimeoutV1(const void* raw)
{uint64_t value=((const xnetdialconfig*)raw)->Timeout;return value?xl6TcpOptionsMillisV1(value):-1;}
int64_t xl6TcpDialOptionsFallbackDelayV1(const void* raw){return xl6TcpOptionsMillisV1(((const xnetdialconfig*)raw)->FallbackDelay);}
int64_t xl6TcpDialOptionsMaxAttemptsV1(const void* raw){return ((const xnetdialconfig*)raw)->MaxAttempts;}
void* xl6TcpDialOptionsWithFamilyV1(const void* raw,int64_t family)
{
    if(!xl6TcpOptionsRangeV1(family==XNET_FAMILY_UNSPEC||family==XNET_FAMILY_IPV4||family==XNET_FAMILY_IPV6))return NULL;
    xnetdialconfig value=*(const xnetdialconfig*)raw;value.Family=(xnetfamily)family;return publish(&value);
}
void* xl6TcpDialOptionsWithAffinityV1(const void* raw,uint64_t affinity)
{xnetdialconfig value=*(const xnetdialconfig*)raw;value.Affinity=affinity;return publish(&value);}
void* xl6TcpDialOptionsWithTimeoutV1(const void* raw,int64_t millis)
{uint64_t micros;if(!xl6TcpOptionsTimeoutV1(millis,&micros))return NULL;xnetdialconfig value=*(const xnetdialconfig*)raw;value.Timeout=micros;return publish(&value);}
void* xl6TcpDialOptionsWithFallbackDelayV1(const void* raw,int64_t millis)
{uint64_t micros;if(!xl6TcpOptionsFiniteMillisV1(millis,&micros))return NULL;xnetdialconfig value=*(const xnetdialconfig*)raw;value.FallbackDelay=micros;return publish(&value);}
void* xl6TcpDialOptionsWithMaxAttemptsV1(const void* raw,int64_t attempts)
{
    if(!xl6TcpOptionsRangeV1(attempts>0&&(uint64_t)attempts<=UINT32_MAX))return NULL;
    xnetdialconfig value=*(const xnetdialconfig*)raw;value.MaxAttempts=(uint32)attempts;return publish(&value);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TcpDialOptionsHandlePolicyV1Get,xl6TcpDialOptionsDestroyV1)
