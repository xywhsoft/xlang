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
    xlogappender* appender;
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
