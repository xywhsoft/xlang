#define XRT_MODULE_DIR
#define XRT_MODULE_FILE_TEXT
#define XRT_MODULE_LOGGER_CONSOLE
#define XRT_MODULE_LOGGER_FILE_JSON
#define XRT_MODULE_LOGGER_FILE_TEXT
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



typedef struct log_capture_state {
    int Count;
    char Last[128];
} log_capture_state;



/* 输出与 xlang 示例一致的发布门禁结果。 */
static void require_line(const char* name, bool ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}



/* 失败时终止示例，但不额外输出成功行。 */
static void require_ok(const char* name, bool ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
}



/* 判断借用视图是否等于零结尾文本。 */
static bool view_equal(xstrview value, cstr expected)
{
    size_t size = expected != NULL ? strlen(expected) : 0;

    return (value.Size == size) &&
        ((size == 0) || (memcmp(value.Data, expected, size) == 0));
}



/* 读取 UTF-8 文本并查找测试消息。 */
static bool file_contains(cstr path, cstr first, cstr second)
{
    size_t size = 0;
    str text = xrtFileReadText(
        path,
        XENCODING_UTF8,
        XUTF_STRICT,
        &size
    );
    bool result = (text != NULL) &&
        (strstr(text, first) != NULL) &&
        (strstr(text, second) != NULL);

    (void)size;
    xrtFree(text);
    return result;
}



/* 创建具有明确文本样式和滚动策略的文件 Sink。 */
static xlogsink* make_text_file(
    cstr path,
    xloglevel level,
    xlogtextstyle style,
    uint64 maxBytes,
    uint32 backups
)
{
    xlogfileoptions options;
    xlogtextconfig text;

    if ( !xrtLogFileOptionsInit(&options, path) ||
         !xrtLogTextConfigInit(&text, style) ) {
        return NULL;
    }
    options.Level = level;
    options.MaxBytes = maxBytes;
    options.BackupCount = backups;
    return xrtLogTextFile(&options, &text);
}



/* 捕获一条借用记录，并在返回前完成复制。 */
static xlogresult capture_log(const xlogrecord* record, ptr userData)
{
    log_capture_state* state = (log_capture_state*)userData;
    size_t size;

    if ( (state == NULL) || (record == NULL) ) {
        return XLOG_RESULT_ERROR;
    }
    size = record->Message.Size;
    if ( size >= sizeof(state->Last) ) {
        size = sizeof(state->Last) - 1;
    }
    if ( size > 0 ) {
        memcpy(state->Last, record->Message.Data, size);
    }
    state->Last[size] = 0;
    state->Count++;
    return XLOG_RESULT_WRITTEN;
}



/* 演示当前 Logger/Sink 契约、滚动文件和自定义回调。 */
int main(void)
{
    const char* defaultPath = "examples_out/default.log";
    const char* defaultRollingPath = "examples_out/default_rolling.log";
    const char* logPath = "examples_out/stdlib_log.log";
    const char* rollingPath = "examples_out/rolling.log";
    const char* rollingBackup = "examples_out/rolling.log.1";
    xlogconsoleconfig console;
    xlogfileoptions jsonOptions;
    xlogsinkconfig customConfig;
    log_capture_state capture;
    xlogger* defaultLogger;
    xlogger* logger;
    xlogger* rolling;
    xlogger* customLogger;
    xlogsink* sink;
    int i;

    xrtDirCreateAll("examples_out");
    xrtFileDelete(defaultPath);
    xrtFileDelete(defaultRollingPath);
    xrtFileDelete("examples_out/default_rolling.log.1");
    xrtFileDelete(logPath);
    xrtFileDelete(rollingPath);
    xrtFileDelete(rollingBackup);

    /* 默认 Logger 由应用显式拥有和安装。 */
    defaultLogger = xrtLogCreate(XRT_STR_LITERAL("xlang"), XLOG_OFF);
    require_ok("default", defaultLogger != NULL);
    require_ok("setDefault", xrtLogSetDefault(defaultLogger));
    require_ok("consoleConfig", xrtLogConsoleConfigInit(&console));
    console.Level = XLOG_OFF;
    console.Color = XLOG_CONSOLE_COLOR_NEVER;
    require_ok("defaultConsole", xrtLogAddConsole(defaultLogger, &console));
    require_ok("jsonOptions", xrtLogFileOptionsInit(&jsonOptions, defaultPath));
    jsonOptions.Level = XLOG_OFF;
    require_ok("defaultFile", xrtLogAddJsonFile(defaultLogger, &jsonOptions, NULL));
    sink = make_text_file(defaultRollingPath, XLOG_OFF, XLOG_TEXT_FULL, 96, 1);
    require_ok(
        "defaultRolling",
        (sink != NULL) && xrtLogAttach(defaultLogger, sink)
    );
    xrtLogSinkFree(sink);
    require_ok("defaultFlush", xrtLogFlush(defaultLogger));
    require_line(
        "level",
        (strcmp(xrtLogLevelName(XLOG_WARN), "WARN") == 0) &&
        (strcmp(xrtLogLevelName(XLOG_DEBUG), "DEBUG") == 0) &&
        (strcmp(xrtLogLevelName(XLOG_ERROR), "ERROR") == 0) &&
        (strcmp(xrtLogLevelName(XLOG_FATAL), "FATAL") == 0) &&
        (xrtLogLevel(defaultLogger) == XLOG_OFF)
    );

    /* 普通文件 Sink 在构造时确定格式，不提供运行期格式变更。 */
    logger = xrtLogCreate(XRT_STR_LITERAL("example"), XLOG_TRACE);
    sink = make_text_file(logPath, XLOG_TRACE, XLOG_TEXT_SIMPLE, 0, 0);
    require_ok(
        "fileSink",
        (logger != NULL) && (sink != NULL) && xrtLogAttach(logger, sink)
    );
    xrtLogSinkFree(sink);
    require_ok(
        "fileWrite",
        (xrtLog(logger, XLOG_INFO, XRT_STR_LITERAL("hello-log")) ==
            XLOG_RESULT_WRITTEN) &&
        (xrtLog(logger, XLOG_WARN, XRT_STR_LITERAL("warn-log")) ==
            XLOG_RESULT_WRITTEN) &&
        xrtLogFlush(logger)
    );
    xrtLogFree(logger);
    require_line("file", file_contains(logPath, "hello-log", "warn-log"));

    /* 小文件上限强制覆盖滚动路径。 */
    rolling = xrtLogCreate(XRT_STR_LITERAL("rolling"), XLOG_TRACE);
    sink = make_text_file(rollingPath, XLOG_TRACE, XLOG_TEXT_SIMPLE, 96, 2);
    require_ok(
        "rollingSink",
        (rolling != NULL) && (sink != NULL) && xrtLogAttach(rolling, sink)
    );
    xrtLogSinkFree(sink);
    for ( i = 0; i < 8; ++i ) {
        char message[32];

        snprintf(message, sizeof(message), "rolling-line-%02d", i);
        require_ok(
            "rollingWrite",
            xrtLog(rolling, XLOG_INFO, xrtStrView(message)) ==
                XLOG_RESULT_WRITTEN
        );
    }
    require_ok("rollingFlush", xrtLogFlush(rolling));
    xrtLogFree(rolling);
    require_line(
        "rolling",
        xrtFileExists(rollingPath) && xrtFileExists(rollingBackup)
    );

    /* 自定义 Sink 直接消费结构化记录，并可显式移除。 */
    memset(&capture, 0, sizeof(capture));
    memset(&customConfig, 0, sizeof(customConfig));
    customConfig.Name = XRT_STR_LITERAL("capture");
    customConfig.Level = XLOG_INFO;
    customConfig.Write = capture_log;
    customConfig.UserData = &capture;
    customLogger = xrtLogCreate(
        XRT_STR_LITERAL("custom-example"),
        XLOG_TRACE
    );
    sink = xrtLogSinkCreate(&customConfig);
    require_ok(
        "customSink",
        (customLogger != NULL) && (sink != NULL) &&
        view_equal(xrtLogSinkName(sink), "capture") &&
        xrtLogAttach(customLogger, sink)
    );
    require_ok(
        "customWrite",
        (xrtLog(customLogger, XLOG_INFO, XRT_STR_LITERAL("captured")) ==
            XLOG_RESULT_WRITTEN) &&
        (capture.Count == 1) &&
        (strcmp(capture.Last, "captured") == 0) &&
        xrtLogDetach(customLogger, sink)
    );
    (void)xrtLog(customLogger, XLOG_INFO, XRT_STR_LITERAL("ignored"));
    require_line("custom", capture.Count == 1);
    xrtLogSinkFree(sink);
    xrtLogFree(customLogger);

    xrtLogSetDefault(NULL);
    xrtLogFree(defaultLogger);
    xrtFileDelete(defaultPath);
    xrtFileDelete(defaultRollingPath);
    xrtFileDelete("examples_out/default_rolling.log.1");
    xrtFileDelete(logPath);
    xrtFileDelete(rollingPath);
    xrtFileDelete(rollingBackup);
    return 0;
}
