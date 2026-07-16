#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}

static void require_ok(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
}

static int text_contains(str text, const char* needle)
{
    return text != NULL && needle != NULL && strstr((const char*)text, needle) != NULL;
}

typedef struct log_capture_state {
    int count;
    char last[128];
} log_capture_state;

static void capture_log(const xlogevent* event, ptr userData)
{
    log_capture_state* state = (log_capture_state*)userData;
    if ( state == NULL || event == NULL || event->sMessage == NULL ) {
        return;
    }
    state->count += 1;
    snprintf(state->last, sizeof(state->last), "%s", event->sMessage);
}

int main(void)
{
    str tempDir;
    str baseDir;
    str defaultPath;
    str defaultRollingPath;
    str defaultRollingBackup;
    str logPath;
    str rollingPath;
    str rollingBackup;
    str text;
    xlogger* logger;
    xlogger* rolling;
    xlogger* customLogger;
    xlogappender* appender;
    log_capture_state capture;
    int i;

    xrtInit();

    tempDir = xrtPathTemp();
    baseDir = xrtPathJoin(2, tempDir, "xl6_stdlib_log_example");
    defaultPath = xrtPathJoin(2, baseDir, "default.log");
    defaultRollingPath = xrtPathJoin(2, baseDir, "default_rolling.log");
    defaultRollingBackup = xrtFormat("%s.1", defaultRollingPath);
    logPath = xrtPathJoin(2, baseDir, "stdlib_log.log");
    rollingPath = xrtPathJoin(2, baseDir, "rolling.log");
    rollingBackup = xrtFormat("%s.1", rollingPath);
    xrtDirCreateAll(baseDir);
    xrtFileDelete(defaultPath);
    xrtFileDelete(defaultRollingPath);
    xrtFileDelete(defaultRollingBackup);
    xrtFileDelete(logPath);
    xrtFileDelete(rollingPath);
    xrtFileDelete(rollingBackup);

    require_line("level", xrtStrComp(xlogLevelName(XLOG_WARN), (str)"WARN", 0, FALSE) == 0);
    require_ok("debugLevel", xrtStrComp(xlogLevelName(XLOG_DEBUG), (str)"DEBUG", 0, FALSE) == 0);
    require_ok("errorLevel", xrtStrComp(xlogLevelName(XLOG_ERROR), (str)"ERROR", 0, FALSE) == 0);
    require_ok("fatalLevel", xrtStrComp(xlogLevelName(XLOG_FATAL), (str)"FATAL", 0, FALSE) == 0);
    xlogSetLevel(xlogDefault(), XLOG_OFF);
    require_ok("defaultLevel", xlogGetLevel(xlogDefault()) == XLOG_OFF);
    require_ok("defaultConsole", xlogAddConsole(xlogDefault(), XLOG_OFF, FALSE) != NULL);
    appender = xlogAddFile(xlogDefault(), defaultPath, XLOG_OFF);
    require_ok("defaultFile", appender != NULL);
    xlogAppenderSetFormat(appender, XLOG_FORMAT_JSON);
    appender = xlogAddRollingFile(xlogDefault(), defaultRollingPath, 96, 1, XLOG_OFF);
    require_ok("defaultRolling", appender != NULL);
    xlogAppenderSetFormat(appender, XLOG_FORMAT_TEXT);
    xloggerInfo(xlogDefault(), "silent-write");
    xloggerTrace(xlogDefault(), "silent-trace");
    xloggerDebug(xlogDefault(), "silent-debug");
    xloggerInfo(xlogDefault(), "silent-info");
    xloggerWarn(xlogDefault(), "silent-warn");
    xloggerError(xlogDefault(), "silent-error");
    xloggerFatal(xlogDefault(), "silent-fatal");
    xlogFlush(xlogDefault());

    logger = xlogCreate((str)"example");
    require_ok("create", logger != NULL);
    xlogSetLevel(logger, XLOG_TRACE);
    appender = xlogAddFile(logger, logPath, XLOG_TRACE);
    require_ok("addFile", appender != NULL);
    xlogAppenderSetFormat(appender, XLOG_FORMAT_SIMPLE);
    xloggerInfo(logger, "hello-log");
    xloggerWarn(logger, "warn-log");
    xlogFlush(logger);
    xlogDestroy(logger);

    text = xrtFileReadAll(logPath, XRT_CP_UTF8, NULL);
    require_line("file", text_contains(text, "hello-log") && text_contains(text, "warn-log"));
    xrtFree(text);

    rolling = xlogCreate((str)"rolling");
    require_ok("createRolling", rolling != NULL);
    appender = xlogAddRollingFile(rolling, rollingPath, 96, 2, XLOG_TRACE);
    require_ok("addRolling", appender != NULL);
    xlogAppenderSetFormat(appender, XLOG_FORMAT_SIMPLE);
    for ( i = 0; i < 8; ++i ) {
        xloggerInfo(rolling, "rolling-line-%02d", i);
    }
    xlogFlush(rolling);
    xlogDestroy(rolling);

    require_line("rolling", xrtFileExists(rollingPath) && xrtFileExists(rollingBackup));

    memset(&capture, 0, sizeof(capture));
    customLogger = xlogCreate((str)"custom-example");
    require_ok("createCustom", customLogger != NULL);
    xlogSetLevel(customLogger, XLOG_TRACE);
    appender = xlogAddCustom(customLogger, (str)"capture", XLOG_INFO, capture_log, &capture);
    require_ok("customAppender", appender != NULL);
    require_ok("customName", xrtStrComp(xlogAppenderName(appender), (str)"capture", 0, FALSE) == 0);
    xlogAppenderSetFormat(appender, XLOG_FORMAT_JSON);
    require_ok("customFormat", xlogAppenderGetFormat(appender) == XLOG_FORMAT_JSON);
    xlogAppenderSetColor(appender, FALSE);
    require_ok("customColor", !xlogAppenderGetColor(appender));
    xloggerInfo(customLogger, "captured");
    xlogFlush(customLogger);
    require_ok("customCaptured", capture.count == 1 && strcmp(capture.last, "captured") == 0);
    require_ok("customRemove", xlogRemoveAppender(customLogger, appender));
    xloggerInfo(customLogger, "ignored");
    xlogFlush(customLogger);
    require_line("custom", capture.count == 1);
    xlogDestroy(customLogger);

    xrtFree(tempDir);
    xrtFree(baseDir);
    xrtFree(defaultPath);
    xrtFree(defaultRollingPath);
    xrtFree(defaultRollingBackup);
    xrtFree(logPath);
    xrtFree(rollingPath);
    xrtFree(rollingBackup);
    xrtUnit();
    return 0;
}
