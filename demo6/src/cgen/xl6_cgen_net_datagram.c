/* Legacy Engine snapshot only. UDP implementations are resident services. */
void xl6CgenEmitNetDatagramComponent(XL6_Output* out, const XL6_CgenNetDatagramNeeds* needs)
{
    if (!out || !needs || !needs->engineStats) return;
    XL6_CgenNetNeeds common = {0}; common.stats = 1;
    xl6CgenEmitNetTcpStatsComponent(out, &common);
    xl6OutputPuts(
        "void* xl6_xrt_net_runtime_stats(void) { xnetenginestats v; if (xl6_net_engine == NULL || !xrtNetEngineStats(xl6_net_engine, &v)) return NULL; { const uint64_t a[] = {v.State,v.Workers,v.PostsAccepted,v.PostsRejected,v.PostsExecuted,v.TimersAccepted,v.TimersRejected,v.TimersFired,v.TimersCancelled,v.TimersClosed,v.TimerErrors,v.Events,v.WaitErrors,v.NodeCacheHits,v.NodeCacheMisses,v.PendingCommands,v.ActiveTimers,v.NodeCachedBytes,v.LiveObjects}; xl6_net_stats_snapshot* snapshot=xl6_net_stats_create(sizeof(a)/sizeof(a[0])); if(snapshot)memcpy(snapshot->Values,a,sizeof(a)); return snapshot; } }\n", out);
}
