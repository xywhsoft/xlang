#include "xl6_tcp_internal.h"
void* xl6TcpStreamSnapshotV1(void* raw)
{
    xnetstreamstats value;if(!xl6ExecutionTcpStreamStatsV1(raw,&value)){xl6TcpFailedV1();return NULL;}
    xnetstreamstats* result=xrtMalloc(sizeof(*result));if(result)*result=value;else xl6TcpFailedV1();return result;
}
void* xl6TcpListenerSnapshotV1(void* raw)
{
    xnetlistenerstats value;if(!xl6ExecutionTcpListenerStatsV1(raw,&value)){xl6TcpFailedV1();return NULL;}
    xnetlistenerstats* result=xrtMalloc(sizeof(*result));if(result)*result=value;else xl6TcpFailedV1();return result;
}
void xl6TcpStreamStatsDestroyV1(void* raw){xrtFree(raw);}
void xl6TcpListenerStatsDestroyV1(void* raw){xrtFree(raw);}
int64_t xl6TcpStreamStatsStateV1(const void* raw){return ((const xnetstreamstats*)raw)->State;}
int64_t xl6TcpListenerStatsStateV1(const void* raw){return ((const xnetlistenerstats*)raw)->State;}
#define XL6_TCP_STREAM_UINT(Field,Name) uint64_t xl6TcpStreamStats##Field##V1(const void* raw){return ((const xnetstreamstats*)raw)->Field;}
#define XL6_TCP_STREAM_BOOL(Field,Name) bool xl6TcpStreamStats##Field##V1(const void* raw){return ((const xnetstreamstats*)raw)->Field;}
#define XL6_TCP_LISTENER_UINT(Field,Name) uint64_t xl6TcpListenerStats##Field##V1(const void* raw){return ((const xnetlistenerstats*)raw)->Field;}
#include "xl6_tcp_stats_fields.inc"
#undef XL6_TCP_STREAM_UINT
#undef XL6_TCP_STREAM_BOOL
#undef XL6_TCP_LISTENER_UINT
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TcpStreamStatsHandlePolicyV1Get,xl6TcpStreamStatsDestroyV1)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TcpListenerStatsHandlePolicyV1Get,xl6TcpListenerStatsDestroyV1)
