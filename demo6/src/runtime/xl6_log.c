#include "src/runtime/xl6_log.h"
#include <limits.h>

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
