#include "src/runtime/xl6_log.h"
#include <limits.h>

void* xl6LogConsoleV1(int64_t level, int64_t target, int64_t color,
    int64_t errorLevel, bool flush, int64_t style, int64_t flags, int64_t utcOffset)
{
    xlogconsoleconfig config;
    if (level < XLOG_TRACE || level > XLOG_OFF ||
        errorLevel < XLOG_TRACE || errorLevel > XLOG_OFF ||
        target < XLOG_CONSOLE_STDOUT || target > XLOG_CONSOLE_SPLIT ||
        color < XLOG_CONSOLE_COLOR_AUTO || color > XLOG_CONSOLE_COLOR_ALWAYS ||
        style < XLOG_TEXT_FULL || style > XLOG_TEXT_MESSAGE ||
        flags < -1 || flags > UINT32_MAX || utcOffset < -86399 || utcOffset > 86399) {
        xrtSetErrorKind(XERR_RANGE);
        return NULL;
    }
    if (!xrtLogConsoleConfigInit(&config) ||
        !xrtLogTextConfigInit(&config.Text,(xlogtextstyle)style)) return NULL;
    config.Level = (xloglevel)level;
    config.Target = (xlogconsoletarget)target;
    config.Color = (xlogconsolecolor)color;
    config.ErrorLevel = (xloglevel)errorLevel;
    config.Flush = flush;
    if (flags >= 0) config.Text.Flags = (uint32_t)flags;
    config.Text.UtcOffset = (int)utcOffset;
    return xl6LogSinkBindNativeV1(xrtLogConsole(&config));
}
