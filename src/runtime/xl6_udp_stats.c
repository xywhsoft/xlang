#include "xl6_udp_internal.h"
void* xl6UdpSnapshotV1(void* raw)
{
    xnetudpstats value;if(!xl6ExecutionUdpStatsV1(raw,&value)){xl6UdpFailedV1();return NULL;}
    xnetudpstats* result=xrtMalloc(sizeof(*result));if(result)*result=value;else xl6UdpFailedV1();return result;
}
void xl6UdpStatsDestroyV1(void* raw){xrtFree(raw);}
int64_t xl6UdpStatsStateV1(const void* raw){return ((const xnetudpstats*)raw)->State;}
bool xl6UdpStatsConnectedV1(const void* raw){return ((const xnetudpstats*)raw)->Connected;}
#define XL6_UDP_STAT(Field,Name) uint64_t xl6UdpStats##Field##V1(const void* raw){return ((const xnetudpstats*)raw)->Field;}
#include "xl6_udp_stats_fields.h"
#undef XL6_UDP_STAT
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6UdpStatsHandlePolicyV1Get,xl6UdpStatsDestroyV1)
