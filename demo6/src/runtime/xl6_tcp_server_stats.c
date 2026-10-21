#include "xl6_tcp_server_internal.h"
void* xl6TcpServerSnapshotV1(void* raw)
{
    XL6_TcpServerScopeV1 scope={0};if(!xl6TcpServerEnterV1(raw,false,&scope))return NULL;
    xnetserverstats value;bool ok=xrtNetServerStats(scope.server,&value);xl6ExecutionTcpServerLeaveInternalV1(&scope);
    if(!ok){xl6TcpServerFailedV1();return NULL;}
    xnetserverstats* result=xrtMalloc(sizeof(*result));if(result)*result=value;else xl6TcpServerFailedV1();return result;
}
void xl6TcpServerStatsDestroyV1(void* raw){xrtFree(raw);}
int64_t xl6TcpServerStatsStateV1(const void* raw){return ((const xnetserverstats*)raw)->State;}
#define XL6_TCP_SERVER_UINT(Field,Name) uint64_t xl6TcpServerStats##Field##V1(const void* raw){return ((const xnetserverstats*)raw)->Field;}
#include "xl6_tcp_server_stats_fields.inc"
#undef XL6_TCP_SERVER_UINT
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TcpServerStatsHandlePolicyV1Get,xl6TcpServerStatsDestroyV1)
