#include "src/runtime/xl6_log.h"
#include <assert.h>
#include <string.h>

static size_t received;
static char message[16];
static int mode;
static int drops;
static xlogresult capture(const xlogrecord* record, ptr context)
{
    (void)context;
    assert(record->Message.Size <= sizeof(message));
    memcpy(message, record->Message.Data, record->Message.Size);
    received = record->Message.Size;
    if (mode == -1) { xrtSetErrorKind(XERR_IO); return XLOG_RESULT_ERROR; }
    return (xlogresult)mode;
}
static void dropped(ptr context) { (void)context; ++drops; }
int main(void)
{
    xlogsinkconfig config = {0};
    xlogger* logger;
    xlogsink* sink;
    xlogstats* stats;
    xstrview name;
    config.Name = (xstrview){"capture", 7};
    config.Level = XLOG_TRACE;
    config.Write = capture;
    config.Drop = dropped;
    logger = xl6LogCreateV1((xstrview){"a\0b", 3}, XLOG_TRACE);
    assert(logger);
    name = xl6LogNameV1(logger);
    assert(name.Size == 3 && !memcmp(name.Data,"a\0b",3));
    xrtFree((void*)name.Data);
    sink = xrtLogSinkCreate(&config);
    assert(sink && xl6LogAttachV1(logger,sink));
    /* Logger now owns a second reference; releasing the caller cannot drop it. */
    xrtLogSinkFree(sink);
    mode = XLOG_RESULT_WRITTEN;
    assert(xl6LogWriteV1(logger,XLOG_INFO,(xstrview){"a\0b",3},20) == XLOG_RESULT_WRITTEN);
    assert(received == 3 && !memcmp(message,"a\0b",3));
    assert(xl6LogWriteV1(logger,XLOG_INFO,(xstrview){"a\0b",3},2) == XLOG_RESULT_WRITTEN);
    assert(received == 2 && !memcmp(message,"a\0",2));
    mode = XLOG_RESULT_DROPPED;
    assert(xl6LogWriteV1(logger,XLOG_INFO,(xstrview){"",0},-1) == XLOG_RESULT_DROPPED);
    mode = XLOG_RESULT_ERROR;
    assert(xl6LogWriteV1(logger,XLOG_INFO,(xstrview){"io",2},-1) == XLOG_RESULT_ERROR);
    assert(xrtErrorKind(xrtGetError()) == XERR_IO);
    xrtClearError();
    stats = xl6LogStatsV1(logger,false);
    assert(stats && stats->Submitted == 4 && stats->Written == 2 && stats->Dropped == 1 && stats->Failed == 1);
    assert(xl6LogStatsCounterV1(stats,0) == 4);
    xl6LogStatsDestroyV1(stats);
    assert(xl6LogSetDefaultV1(logger));
    mode = XLOG_RESULT_WRITTEN;
    assert(xl6LogDefaultWriteV1(XLOG_INFO,(xstrview){"a\0b",3},20) == XLOG_RESULT_WRITTEN);
    assert(received == 3 && !memcmp(message,"a\0b",3));
    mode = XLOG_RESULT_ERROR;
    assert(xl6LogDefaultWriteV1(XLOG_INFO,(xstrview){"io",2},-1) == XLOG_RESULT_ERROR);
    assert(xrtErrorKind(xrtGetError()) == XERR_IO);
    xrtClearError();
    xlogger* current = xl6LogDefaultV1();
    assert(current == logger);
    xl6LogDestroyV1(current);
    assert(xl6LogSetDefaultV1(NULL));
    assert(xl6LogDefaultV1() == NULL);
    assert(xl6LogDefaultFlushV1());
    assert(xl6LogDefaultWriteV1(XLOG_INFO,(xstrview){"none",4},-1) == XLOG_RESULT_SKIPPED);
    assert(xl6LogDefaultWriteV1(XLOG_INFO,(xstrview){"bad",3},-2) == XLOG_RESULT_ERROR);
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError();
    assert(xl6LogWriteV1(logger,XLOG_INFO,(xstrview){"x",1},-2) == XLOG_RESULT_ERROR);
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError();
    assert(xl6LogDetachAllV1(logger) == 1 && drops == 1);
    xl6LogDestroyV1(logger);
    return 0;
}
