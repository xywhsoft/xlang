#include "xl6_tcp_options_internal.h"
static void* publish(const xnetlistenconfig* value)
{
    if(!xrtNetListenConfigValid(value)){xl6TcpOptionsFailedV1();return NULL;}
    xnetlistenconfig* result=xrtMalloc(sizeof(*result));if(result)*result=*value;else xl6TcpOptionsFailedV1();return result;
}
void* xl6TcpListenOptionsCopyV1(const xnetlistenconfig* value){return publish(value);}
void* xl6TcpListenOptionsCreateV1(void)
{xnetlistenconfig value;xrtNetListenConfigInit(&value);return publish(&value);}
void* xl6TcpListenOptionsCreateAddressV1(const void* address)
{xnetlistenconfig value;xrtNetListenConfigInit(&value);value.Address=*(const xnetaddr*)address;return publish(&value);}
void xl6TcpListenOptionsDestroyV1(void* raw){xrtFree(raw);}
void* xl6TcpListenOptionsWithAddressV1(const void* raw,const void* address)
{xnetlistenconfig value=*(const xnetlistenconfig*)raw;value.Address=*(const xnetaddr*)address;return publish(&value);}
void* xl6TcpListenOptionsStreamV1(const void* raw){return xl6TcpStreamOptionsCopyV1(&((const xnetlistenconfig*)raw)->Stream);}
void* xl6TcpListenOptionsWithStreamV1(const void* raw,const void* stream)
{xnetlistenconfig value=*(const xnetlistenconfig*)raw;value.Stream=*(const xnetstreamconfig*)stream;return publish(&value);}
#define XL6_TCP_LISTEN_INT(Field,Name) int64_t xl6TcpListenOptions##Field##V1(const void* raw){return ((const xnetlistenconfig*)raw)->Field;}
#define XL6_TCP_LISTEN_BOOL(Field,Name) bool xl6TcpListenOptions##Field##V1(const void* raw){return ((const xnetlistenconfig*)raw)->Field;}
#include "xl6_tcp_options_fields.inc"
#undef XL6_TCP_LISTEN_INT
#undef XL6_TCP_LISTEN_BOOL
uint64_t xl6TcpListenOptionsAffinityV1(const void* raw){return ((const xnetlistenconfig*)raw)->Affinity;}
int64_t xl6TcpListenOptionsDistributionV1(const void* raw){return ((const xnetlistenconfig*)raw)->Distribution;}
void* xl6TcpListenOptionsWithAffinityV1(const void* raw,uint64_t affinity)
{xnetlistenconfig value=*(const xnetlistenconfig*)raw;value.Affinity=affinity;return publish(&value);}
void* xl6TcpListenOptionsWithAcceptQueueV1(const void* raw,int64_t concurrent,int64_t pending)
{
    if(!xl6TcpOptionsRangeV1(concurrent>0&&(uint64_t)concurrent<=UINT32_MAX&&pending>0&&(uint64_t)pending<=UINT32_MAX))return NULL;
    xnetlistenconfig value=*(const xnetlistenconfig*)raw;value.AcceptConcurrency=(uint32)concurrent;value.AcceptQueueLimit=(uint32)pending;return publish(&value);
}
void* xl6TcpListenOptionsWithBacklogV1(const void* raw,int64_t backlog)
{if(!xl6TcpOptionsRangeV1(backlog>0&&backlog<=INT_MAX))return NULL;xnetlistenconfig value=*(const xnetlistenconfig*)raw;value.Backlog=(int)backlog;return publish(&value);}
void* xl6TcpListenOptionsWithDistributionV1(const void* raw,int64_t distribution)
{
    if(!xl6TcpOptionsRangeV1(distribution==XNET_ACCEPT_ROUND_ROBIN||distribution==XNET_ACCEPT_LOCAL))return NULL;
    xnetlistenconfig value=*(const xnetlistenconfig*)raw;value.Distribution=(xnetacceptdistribution)distribution;return publish(&value);
}
void* xl6TcpListenOptionsWithBindingV1(const void* raw,bool reuse,bool reusePort,bool exclusive,bool ipv6Only)
{
    xnetlistenconfig value=*(const xnetlistenconfig*)raw;value.ReuseAddress=reuse;value.ReusePort=reusePort;
    value.ExclusiveAddress=exclusive;value.IPv6Only=ipv6Only;return publish(&value);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TcpListenOptionsHandlePolicyV1Get,xl6TcpListenOptionsDestroyV1)
