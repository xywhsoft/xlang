/* Logging facades contain language-level composition only. Physical ABI and
 * ownership are verified by the resident logging contract catalog. */
const char* xl6StdlibLoggerClassSource =
    "enum LogLevel as int\n"
    "    trace = 0\n    debug = 1\n    info = 2\n    warn = 3\n    error = 4\n    fatal = 5\n    off = 6\nend\n"
    "enum LogResult as int\n"
    "    skipped = 0\n    written = 1\n    dropped = 2\nend\n"
    "enum LogTextStyle as int\n    full = 0\n    simple = 1\n    message = 2\nend\n"
    "enum LogConsoleTarget as int\n    stdout = 0\n    stderr = 1\n    split = 2\nend\n"
    "enum LogColor as int\n    automatic = 0\n    never = 1\n    always = 2\nend\n"
    "class LogTextOptions\n"
    "    LogTextStyle style = LogTextStyle.full\n"
    "    int flags = -1\n"
    "    int utcOffsetSeconds = 0\n"
    "end\n"
    "class LogConsoleOptions\n"
    "    LogLevel level = LogLevel.info\n"
    "    LogConsoleTarget target = LogConsoleTarget.split\n"
    "    LogColor color = LogColor.automatic\n"
    "    LogLevel errorLevel = LogLevel.error\n"
    "    bool flushEachRecord = true\n"
    "    LogTextOptions text = LogTextOptions()\n"
    "end\n"
    "class LogStats\n"
    "    native point handle\n"
    "    extern \"xl6LogStatsTakeV1\" create(point snapshot)\n"
    "    extern \"xl6LogStatsDestroyV1\" destroy()\n"
    "    extern \"xl6LogStatsCounterV1\" func counterNative(int counter) as uint64\n"
    "    get submitted() as uint64\n        return self.counterNative(0)\n    end\n"
    "    get written() as uint64\n        return self.counterNative(1)\n    end\n"
    "    get skipped() as uint64\n        return self.counterNative(2)\n    end\n"
    "    get dropped() as uint64\n        return self.counterNative(3)\n    end\n"
    "    get failed() as uint64\n        return self.counterNative(4)\n    end\n"
    "end\n"
    "class LogSink\n"
    "    native point handle\n"
    "    extern \"xl6LogSinkTakeV1\" create(point sink)\n"
    "    extern \"xl6LogConsoleV1\" create(int level, int target, int color, int errorLevel, bool flush, int style, int flags, int utcOffsetSeconds)\n"
    "    extern \"xl6LogSinkDestroyV1\" destroy()\n"
    "    extern \"xl6LogSinkHandleV1\" func nativeHandle() as point\n"
    "    extern \"xl6LogSinkNameV1\" get name() as string\n"
    "    extern \"xl6LogSinkLevelV1\" func levelNative() as LogLevel\n"
    "    extern \"xl6LogSinkSetLevelV1\" func setLevelNative(int level) as bool\n"
    "    extern \"xl6LogSinkFlushV1\" func flushNative() as bool\n"
    "    func getLevel() as LogLevel\n        return self.levelNative()\n    end\n"
    "    func setLevel(LogLevel level)\n        if not self.setLevelNative(int(level)) then throw RuntimeError \"log sink level failed\" end\n    end\n"
    "    func flush()\n        if not self.flushNative() then throw IOError \"log sink flush failed\" end\n    end\n"
    "    func stats() as LogStats\n        return LogStats(__xl_std_runtime_log_stats(self.nativeHandle(), true))\n    end\n"
    "end\n"
    "class Logger\n"
    "    native point handle\n"
    "    extern \"xl6LogCreateV1\" create(string name, int level)\n"
    "    extern \"xl6LogTakeV1\" create(point logger)\n"
    "    extern \"xl6LogDestroyV1\" destroy()\n"
    "    extern \"xl6LogHandleV1\" func nativeHandle() as point\n"
    "    extern \"xl6LogNameV1\" get name() as string\n"
    "    extern \"xl6LogLevelV1\" func levelNative() as LogLevel\n"
    "    extern \"xl6LogSetLevelV1\" func setLevelNative(int level) as bool\n"
    "    extern \"xl6LogEnabledV1\" func enabledNative(int level) as bool\n"
    "    extern \"xl6LogAttachV1\" func addSink(LogSink sink) as bool\n"
    "    extern \"xl6LogDetachV1\" func removeSink(LogSink sink) as bool\n"
    "    extern \"xl6LogDetachAllV1\" func clearSinks() as int\n"
    "    extern \"xl6LogSinkCountV1\" func sinkCount() as int\n"
    "    extern \"xl6LogWriteV1\" func writeNative(int level, string message, int size) as LogResult\n"
    "    extern \"xl6LogFlushV1\" func flushNative() as bool\n"
    "    func getLevel() as LogLevel\n        return self.levelNative()\n    end\n"
    "    func setLevel(LogLevel level)\n        if not self.setLevelNative(int(level)) then throw RuntimeError \"log level failed\" end\n    end\n"
    "    func enabled(LogLevel level) as bool\n        return self.enabledNative(int(level))\n    end\n"
    "    func write(LogLevel level, string message, int size = -1) as LogResult\n"
    "        return self.writeNative(int(level), message, size)\n    end\n"
    "    func trace(string message, int size = -1) as LogResult\n        return self.write(LogLevel.trace, message, size)\n    end\n"
    "    func debug(string message, int size = -1) as LogResult\n        return self.write(LogLevel.debug, message, size)\n    end\n"
    "    func info(string message, int size = -1) as LogResult\n        return self.write(LogLevel.info, message, size)\n    end\n"
    "    func warn(string message, int size = -1) as LogResult\n        return self.write(LogLevel.warn, message, size)\n    end\n"
    "    func error(string message, int size = -1) as LogResult\n        return self.write(LogLevel.error, message, size)\n    end\n"
    "    func fatal(string message, int size = -1) as LogResult\n        return self.write(LogLevel.fatal, message, size)\n    end\n"
    "    func flush()\n        if not self.flushNative() then throw IOError \"log flush failed\" end\n    end\n"
    "    func stats() as LogStats\n        return LogStats(__xl_std_runtime_log_stats(self.nativeHandle(), false))\n    end\n"
    "end\n";

static const char xl6StdLogCreateBody[] =
    "func __xl_std_log_create(string name = \"xlang\", LogLevel level = LogLevel.info) as Logger\n"
    "    return Logger(name, int(level))\nend\n";
static const char xl6StdLogDefaultBody[] =
    "func __xl_std_log_default() as Logger?\n"
    "    point logger = __xl_std_runtime_log_default()\n"
    "    if logger == null then return null end\n"
    "    return Logger(logger)\nend\n";
static const char xl6StdLogSetDefaultBody[] =
    "func __xl_std_log_setDefault(Logger? logger)\n"
    "    if logger == null then\n"
    "        if not __xl_std_runtime_log_set_default(null) then throw RuntimeError \"log default clear failed\" end\n"
    "    else\n"
    "        if not __xl_std_runtime_log_set_default(logger.nativeHandle()) then throw RuntimeError \"log default set failed\" end\n"
    "    end\nend\n";
static const char xl6StdLogLevelNameBody[] =
    "func __xl_std_log_level_name(LogLevel level) as string\n"
    "    return __xl_std_runtime_log_level_name(int(level))\nend\n";
static const char xl6StdLogConsoleSinkBody[] =
    "func __xl_std_log_consoleSink(LogConsoleOptions? options = null) as LogSink\n"
    "    if options == null then return __xl_std_log_consoleSink(LogConsoleOptions()) end\n"
    "    return LogSink(int(options.level), int(options.target), int(options.color), int(options.errorLevel), options.flushEachRecord, int(options.text.style), options.text.flags, options.text.utcOffsetSeconds)\nend\n";
static const char xl6StdLogWriteBody[] =
    "func __xl_std_log_write(LogLevel level, string message, int size = -1) as LogResult\n"
    "    return __xl_std_runtime_log_default_write(int(level), message, size)\nend\n";
static const char xl6StdLogFlushBody[] =
    "func __xl_std_log_flush()\n"
    "    if not __xl_std_runtime_log_default_flush() then throw IOError \"log default flush failed\" end\nend\n";
#define XL6_LOG_SEVERITY_BODY(Name) \
static const char xl6StdLog##Name##Body[] = \
    "func __xl_std_log_" #Name "(string message, int size = -1) as LogResult\n" \
    "    return __xl_std_log_write(LogLevel." #Name ", message, size)\nend\n";
XL6_LOG_SEVERITY_BODY(trace)
XL6_LOG_SEVERITY_BODY(debug)
XL6_LOG_SEVERITY_BODY(info)
XL6_LOG_SEVERITY_BODY(warn)
XL6_LOG_SEVERITY_BODY(error)
XL6_LOG_SEVERITY_BODY(fatal)
#undef XL6_LOG_SEVERITY_BODY
static const XL6_StdlibParam xl6StdLogSetDefaultParams[]={{"point","logger"}};
static const XL6_StdlibParam xl6StdLogStatsParams[]={{"point","owner"},{"bool","sink"}};
static const XL6_StdlibParam xl6StdLogDefaultWriteParams[]={{"int","level"},{"string","message"},{"int","size"}};
