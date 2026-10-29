#include "src/runtime/xl6_log.h"
#include <limits.h>
#include "src/runtime/xl6_log_queue.h"

void* xl6LogQueueStatsTakeV1(void* snapshot) { if (!snapshot) xrtSetErrorKind(XERR_ARGUMENT); return snapshot; }
void xl6LogQueueStatsDestroyV1(void* snapshot) { xrtFree(snapshot); }
uint64_t xl6LogQueueCounterV1(void* snapshot,int64_t index)
{
    if (!snapshot) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }
    if (index<0 || index>=18) { xrtSetErrorKind(XERR_RANGE); return 0; }
    return ((xl6_log_queue_stats*)snapshot)->values[index];
}

typedef struct log_affine_sink { void* sink; void* context; struct log_affine_sink* next; } log_affine_sink;
static xonce affine_once;
static xmutex affine_lock;
static log_affine_sink* affine_sinks;
static bool affine_init(void* unused) { (void)unused; return xrtMutexInit(&affine_lock); }
bool xl6LogSinkAffinityV1(void* sink,bool add,void* context)
{
    log_affine_sink* item=NULL;
    if (!xrtOnce(&affine_once,affine_init,NULL)) return false;
    if (add) { item=xrtMalloc(sizeof(*item)); if (!item) return false; item->sink=sink; item->context=context; }
    if (!xrtMutexLock(&affine_lock)) { xrtFree(item); return false; }
    if (add) { item->next=affine_sinks; affine_sinks=item; }
    else {
        log_affine_sink** link=&affine_sinks;
        while (*link && (*link)->sink!=sink) link=&(*link)->next;
        if (*link) { item=*link; *link=item->next; xrtFree(item); }
    }
    return xrtMutexUnlock(&affine_lock);
}
void* xl6LogSinkAffinityContextV1(void* sink)
{
    void* context=NULL;
    if (!sink) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (!xrtOnce(&affine_once,affine_init,NULL) || !xrtMutexLock(&affine_lock)) return NULL;
    for (log_affine_sink* item=affine_sinks;item;item=item->next)
        if (item->sink==sink) { context=item->context; break; }
    if (!xrtMutexUnlock(&affine_lock)) return NULL;
    if (!context) xrtSetErrorKind(XERR_TYPE);
    return context;
}
bool xl6LogSinkWorkerSafeV1(void* sink)
{
    bool safe=true;
    if (!sink) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!xrtOnce(&affine_once,affine_init,NULL) || !xrtMutexLock(&affine_lock)) return false;
    for (log_affine_sink* item=affine_sinks;item;item=item->next) if (item->sink==sink) { safe=false; break; }
    if (!xrtMutexUnlock(&affine_lock)) return false;
    if (!safe) xrtSetErrorKind(XERR_STATE);
    return safe;
}

static bool log_level(int64_t value, bool threshold)
{
    if (value < XLOG_TRACE || value > (threshold ? XLOG_OFF : XLOG_FATAL)) {
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    return true;
}

static bool log_handle(const void* handle)
{
    if (handle) return true;
    xrtSetErrorKind(XERR_ARGUMENT);
    return false;
}

static xstrview log_copy(xstrview source)
{
    xstrview copy = {xrtStrDupN(source.Data, source.Size), source.Size};
    if (!copy.Data) copy.Size = 0;
    return copy;
}

static bool log_message(xstrview* message, int64_t size)
{
    if (size < -1) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (size >= 0 && (uint64_t)size < message->Size) message->Size = (size_t)size;
    return true;
}

void* xl6LogCreateV1(xstrview name, int64_t level)
{ return log_level(level, true) ? xrtLogCreate(name, (xloglevel)level) : NULL; }
void* xl6LogTakeV1(void* logger) { return log_handle(logger) ? logger : NULL; }
void xl6LogDestroyV1(void* logger) { xrtLogFree(logger); }
void* xl6LogHandleV1(void* logger) { return log_handle(logger) ? logger : NULL; }
xstrview xl6LogNameV1(void* logger)
{ return log_handle(logger) ? log_copy(xrtLogName(logger)) : (xstrview){0}; }
int64_t xl6LogLevelV1(void* logger)
{ return log_handle(logger) ? xrtLogLevel(logger) : XLOG_OFF; }
bool xl6LogSetLevelV1(void* logger, int64_t level)
{ return log_handle(logger) && log_level(level, true) && xrtLogSetLevel(logger, (xloglevel)level); }
bool xl6LogEnabledV1(void* logger, int64_t level)
{ return log_handle(logger) && log_level(level, false) && level >= xrtLogLevel(logger); }
bool xl6LogAttachV1(void* logger, void* sink)
{ return log_handle(logger) && log_handle(sink) && xrtLogAttach(logger, sink); }
bool xl6LogDetachV1(void* logger, void* sink)
{ return log_handle(logger) && log_handle(sink) && xrtLogDetach(logger, sink); }
int64_t xl6LogDetachAllV1(void* logger)
{ return log_handle(logger) ? (int64_t)xrtLogDetachAll(logger) : 0; }
int64_t xl6LogSinkCountV1(void* logger)
{ return log_handle(logger) ? (int64_t)xrtLogSinkCount(logger) : 0; }
int64_t xl6LogWriteV1(void* logger, int64_t level, xstrview message, int64_t size)
{
    if (!log_handle(logger) || !log_level(level, false) || !log_message(&message,size)) return XLOG_RESULT_ERROR;
    return xrtLog(logger, (xloglevel)level, message);
}
bool xl6LogFlushV1(void* logger)
{ return log_handle(logger) && xrtLogFlush(logger); }
void* xl6LogDefaultV1(void) { return xrtLogDefault(); }
bool xl6LogSetDefaultV1(void* logger) { return xrtLogSetDefault(logger); }
bool xl6LogReleaseDefaultV1(void* expected)
{
    xlogger* logger=xrtLogTakeDefaultIf(expected);
    if (!logger) return true;
    bool ok=xrtLogFlush(logger);
    xrtLogFree(logger);
    return ok;
}
int64_t xl6LogDefaultWriteV1(int64_t level, xstrview message, int64_t size)
{
    xlogger* logger;
    xlogresult result;
    if (!log_level(level,false) || !log_message(&message,size)) return XLOG_RESULT_ERROR;
    logger = xrtLogDefault();
    if (!logger) return XLOG_RESULT_SKIPPED;
    result = xrtLog(logger,(xloglevel)level,message);
    /* XRT release preserves the pending write error, including last-owner cleanup. */
    xrtLogFree(logger);
    return result;
}
bool xl6LogDefaultFlushV1(void)
{
    xlogger* logger = xrtLogDefault();
    bool result;
    if (!logger) return true;
    result = xrtLogFlush(logger);
    xrtLogFree(logger);
    return result;
}
xstrview xl6LogLevelNameV1(int64_t level)
{ return log_level(level, true) ? log_copy(xrtStrView(xrtLogLevelName((xloglevel)level))) : (xstrview){0}; }
void* xl6LogSinkTakeV1(void* sink) { return log_handle(sink) ? sink : NULL; }
void xl6LogSinkDestroyV1(void* sink) { xrtLogSinkFree(sink); }
void* xl6LogSinkHandleV1(void* sink) { return log_handle(sink) ? sink : NULL; }
xstrview xl6LogSinkNameV1(void* sink)
{ return log_handle(sink) ? log_copy(xrtLogSinkName(sink)) : (xstrview){0}; }
int64_t xl6LogSinkLevelV1(void* sink)
{ return log_handle(sink) ? xrtLogSinkLevel(sink) : XLOG_OFF; }
bool xl6LogSinkSetLevelV1(void* sink, int64_t level)
{ return log_handle(sink) && log_level(level, true) && xrtLogSinkSetLevel(sink, (xloglevel)level); }
bool xl6LogSinkFlushV1(void* sink)
{ return log_handle(sink) && xrtLogSinkFlush(sink); }
void* xl6LogStatsV1(void* owner, bool sink)
{
    xlogstats* snapshot;
    if (!log_handle(owner)) return NULL;
    snapshot = xrtMalloc(sizeof(*snapshot));
    if (!snapshot) return NULL;
    if (!(sink ? xrtLogSinkStats(owner, snapshot) : xrtLogStats(owner, snapshot))) {
        xrtFree(snapshot);
        return NULL;
    }
    return snapshot;
}
void* xl6LogStatsTakeV1(void* snapshot) { return log_handle(snapshot) ? snapshot : NULL; }
void xl6LogStatsDestroyV1(void* snapshot) { xrtFree(snapshot); }
uint64_t xl6LogStatsCounterV1(void* snapshot, int64_t counter)
{
    const xlogstats* stats = snapshot;
    if (!log_handle(stats)) return 0;
    switch (counter) {
    case 0: return stats->Submitted;
    case 1: return stats->Written;
    case 2: return stats->Skipped;
    case 3: return stats->Dropped;
    case 4: return stats->Failed;
    default: xrtSetErrorKind(XERR_RANGE); return 0;
    }
}
