#include "src/runtime/xl6_log_queue.h"
void* xl6LogRingV1(void* target,xstrview name,int64_t level,int64_t capacity,int64_t recordLimit,
    int64_t batch,int64_t stack,uint64_t idleWait)
{
    xlogringconfig config;
    if (!xl6LogSinkWorkerSafeV1(target)) return NULL;
    if (level<0 || level>6) { xrtSetErrorKind(XERR_RANGE); return NULL; }
    if (!xrtLogRingConfigInit(&config)) return NULL;
    config.Name=name; config.Level=(xloglevel)level; config.IdleWait=idleWait;
    if (!log_queue_size(capacity,&config.Capacity) || !log_queue_size(recordLimit,&config.RecordLimit) ||
        !log_queue_size(batch,&config.Batch) || !log_queue_size(stack,&config.StackSize)) return NULL;
    return xrtLogRing(target,&config);
}
bool xl6LogRingStopV1(void* sink) { return xrtLogRingStop(sink); }
xerror* xl6LogRingErrorV1(void* sink) { return xrtLogRingLastError(sink); }
void* xl6LogRingStatsV1(void* sink)
{
    xlogringstats source;
    if (!xrtLogRingStats(sink,&source)) return NULL;
    xl6_log_queue_stats* snapshot=xrtCalloc(1,sizeof(*snapshot));
    if (!snapshot) return NULL;
    uint64_t values[18]={1,source.Enqueued,source.Processed,source.Written,source.Skipped,
        0,0,source.TargetDropped,source.ReentrantDrops,0,source.Failed,source.Flushes,
        source.Queued,source.QueueBytes,source.PeakQueued,source.PeakBytes,source.Dropped,source.Oversized};
    memcpy(snapshot->values,values,sizeof(values)); return snapshot;
}
