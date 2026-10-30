#include "src/runtime/xl6_log_queue.h"
void* xl6LogAsyncV1(void* target,xstrview name,int64_t level,int64_t full,int64_t shutdown,
    int64_t capacity,int64_t recordLimit,int64_t byteLimit,int64_t stack)
{
    xlogasyncconfig config;
    if (!xl6LogSinkWorkerSafeV1(target)) return NULL;
    if (level<0 || level>6 || full<0 || full>2 || shutdown<0 || shutdown>1) { xrtSetErrorKind(XERR_RANGE); return NULL; }
    if (!xrtLogAsyncConfigInit(&config)) return NULL;
    config.Name=name; config.Level=(xloglevel)level; config.Full=(xlogasyncfull)full; config.Shutdown=(xlogasyncshutdown)shutdown;
    if (!log_queue_size(capacity,&config.Capacity) || !log_queue_size(recordLimit,&config.RecordLimit) ||
        !log_queue_size(byteLimit,&config.ByteLimit) || !log_queue_size(stack,&config.StackSize)) return NULL;
    return xrtLogAsync(target,&config);
}
bool xl6LogAsyncStopV1(void* sink) { return xrtLogAsyncStop(sink); }
xerror* xl6LogAsyncErrorV1(void* sink) { return xrtLogAsyncLastError(sink); }
void* xl6LogAsyncStatsV1(void* sink)
{
    xlogasyncstats source;
    if (!xrtLogAsyncStats(sink,&source)) return NULL;
    xl6_log_queue_stats* snapshot=xrtCalloc(1,sizeof(*snapshot));
    if (!snapshot) return NULL;
    uint64_t values[18]={0,source.Enqueued,source.Processed,source.Written,source.Skipped,
        source.DroppedNewest,source.DroppedOldest,source.DroppedTarget,source.ReentrantDrops,
        source.Discarded,source.Failed,source.Flushes,source.Queued,source.QueueBytes,source.PeakQueued,source.PeakBytes,0,0};
    memcpy(snapshot->values,values,sizeof(values)); return snapshot;
}

#include "xl6_ownership_domain_impl.h"
static bool load_log_async(XL6_OwnershipCatalog* catalog)
{ return xl6OwnershipCatalogAddLogSinkV1(catalog,xrtLogAsyncSinkOwnershipV1Get()); }
XL6_OWNERSHIP_QUERY(LogAsync)
{ (void)policies;(void)preparation;(void)completion;*adapter=xrtLogAsyncOwnershipAdapterV1(ref);return *adapter!=NULL; }
XL6_OWNERSHIP_DOMAIN(LogAsync,load_log_async,ownershipDomainLogAsync)
