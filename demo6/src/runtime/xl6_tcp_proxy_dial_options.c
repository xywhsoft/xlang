#include "xl6_tcp_options_internal.h"
/* Immutable by-value configuration, not a retained language/Proxy reference. */
static void* publish(const xnetproxydialconfig* value)
{
    if(!xrtNetDialConfigValid(&value->Transport)){xl6TcpOptionsFailedV1();return NULL;}
    if(!xl6TcpOptionsRangeV1(value->ReceiveLimit>0))return NULL;
    xnetproxydialconfig* result=xrtMalloc(sizeof(*result));
    if(result)*result=*value;else xl6TcpOptionsFailedV1();
    return result;
}
void* xl6TcpProxyDialOptionsCreateV1(void)
{xnetproxydialconfig value;xrtNetProxyDialConfigInit(&value);return publish(&value);}
void xl6TcpProxyDialOptionsDestroyV1(void* raw){xrtFree(raw);}
void* xl6TcpProxyDialOptionsTransportV1(const void* raw)
{return xl6TcpDialOptionsCopyV1(&((const xnetproxydialconfig*)raw)->Transport);}
void* xl6TcpProxyDialOptionsWithTransportV1(const void* raw,const void* transport)
{xnetproxydialconfig value=*(const xnetproxydialconfig*)raw;value.Transport=*(const xnetdialconfig*)transport;return publish(&value);}
int64_t xl6TcpProxyDialOptionsTimeoutV1(const void* raw)
{uint64_t value=((const xnetproxydialconfig*)raw)->Timeout;return value?xl6TcpOptionsMillisV1(value):-1;}
void* xl6TcpProxyDialOptionsWithTimeoutV1(const void* raw,int64_t millis)
{
    uint64_t micros;if(!xl6TcpOptionsTimeoutV1(millis,&micros))return NULL;
    xnetproxydialconfig value=*(const xnetproxydialconfig*)raw;value.Timeout=micros;return publish(&value);
}
uint64_t xl6TcpProxyDialOptionsReceiveLimitV1(const void* raw)
{return ((const xnetproxydialconfig*)raw)->ReceiveLimit;}
void* xl6TcpProxyDialOptionsWithReceiveLimitV1(const void* raw,uint64_t limit)
{
    if(!xl6TcpOptionsSizeV1(limit)||!xl6TcpOptionsRangeV1(limit>0))return NULL;
    xnetproxydialconfig value=*(const xnetproxydialconfig*)raw;value.ReceiveLimit=(size_t)limit;return publish(&value);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TcpProxyDialOptionsHandlePolicyV1Get,xl6TcpProxyDialOptionsDestroyV1)
