static void xl6CgenEmitLogRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_LOG_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_LOG_RUNTIME\n", out);
    xl6OutputPuts("#include <stddef.h>\n#include <stdint.h>\n#include <stdbool.h>\n", out);
    xl6OutputPuts("#ifndef XL6_XVALUE_TYPEDEF\n#define XL6_XVALUE_TYPEDEF\ntypedef struct xvalue_struct* xvalue;\n#endif\n", out);
    xl6OutputPuts("typedef struct xlogger xlogger;\ntypedef struct xlogappender xlogappender;\n", out);
    xl6OutputPuts("typedef struct { long long iTime; int iLevel; const char* sLogger; const char* sFile; uint32_t iLine; const char* sFunc; uint64_t iThreadId; const char* sMessage; } xlogevent;\n", out);
    xl6OutputPuts("typedef struct { xlogger* logger; } xl6_log_logger;\n", out);
    xl6OutputPuts("typedef struct { xlogger* logger; xlogappender* appender; int removeOnDestroy; } xl6_log_appender;\n\n", out);

    xl6OutputPuts("void* xrtCalloc(size_t iCount, size_t iSize);\nvoid xrtFree(void* pMem);\nchar* xrtCopyStr(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("xlogger* xlogCreate(const char* sName);\nxlogger* xlogAddRef(xlogger* pLogger);\nvoid xlogRelease(xlogger* pLogger);\nxlogger* xlogDefault(void);\nvoid xlogSetDefault(xlogger* pLogger);\n", out);
    xl6OutputPuts("void xlogSetLevel(xlogger* pLogger, int iLevel);\nint xlogGetLevel(xlogger* pLogger);\n", out);
    xl6OutputPuts("xlogappender* xlogAddConsole(xlogger* pLogger, int iMinLevel, bool bColor);\n", out);
    xl6OutputPuts("xlogappender* xlogAddFile(xlogger* pLogger, const char* sPath, int iMinLevel);\n", out);
    xl6OutputPuts("xlogappender* xlogAddRollingFile(xlogger* pLogger, const char* sPath, uint64_t iMaxSize, uint32_t iMaxBackup, int iMinLevel);\n", out);
    xl6OutputPuts("xlogappender* xlogAddCustomEx(xlogger* pLogger, const char* sName, int iMinLevel, void (*Proc)(const xlogevent*, void*), void* pUserData, void (*FreeUserData)(void*));\n", out);
    xl6OutputPuts("bool xlogRemoveAppender(xlogger* pLogger, xlogappender* pAppender);\n", out);
    xl6OutputPuts("void xlogAppenderSetLevel(xlogappender* pAppender, int iMinLevel);\nvoid xlogAppenderSetFormat(xlogappender* pAppender, int iFormat);\nvoid xlogAppenderSetColor(xlogappender* pAppender, bool bColor);\n", out);
    xl6OutputPuts("const char* xlogAppenderName(const xlogappender* pAppender);\nint xlogAppenderGetLevel(const xlogappender* pAppender);\nint xlogAppenderGetFormat(const xlogappender* pAppender);\nbool xlogAppenderGetColor(const xlogappender* pAppender);\n", out);
    xl6OutputPuts("const char* xlogLevelName(int iLevel);\nvoid xlogWrite(xlogger* pLogger, int iLevel, const char* sFile, uint32_t iLine, const char* sFunc, const char* sFmt, ...);\nvoid xlogFlush(xlogger* pLogger);\n\n", out);

    xl6OutputPuts("static int xl6_log_level(long long level)\n{\n    if ( level < 0 ) { return 0; }\n    if ( level > 6 ) { return 6; }\n    return (int)level;\n}\n", out);
    xl6OutputPuts("static int xl6_log_format(long long format)\n{\n    if ( format < 0 ) { return 0; }\n    if ( format > 2 ) { return 2; }\n    return (int)format;\n}\n\n", out);

    /* Logger 包装器始终独占一个 XRT 引用。 */
    xl6OutputPuts("static xl6_log_logger* xl6_log_wrap_owned(xlogger* logger)\n{\n    xl6_log_logger* value;\n    if ( logger == 0 ) { return 0; }\n    value = (xl6_log_logger*)xrtCalloc(1u, sizeof(*value));\n    if ( value == 0 ) { xlogRelease(logger); return 0; }\n    value->logger = logger;\n    return value;\n}\n", out);
    xl6OutputPuts("static xlogger* xl6_log_get(void* handle)\n{\n    return handle != 0 ? ((xl6_log_logger*)handle)->logger : 0;\n}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_log_create(const char* name)\n{\n    return xl6_log_wrap_owned(xlogCreate(name != 0 ? name : \"xlang\"));\n}\n", out);
    xl6OutputPuts("void* xl6_xrt_log_wrap_handle(void* handle)\n{\n    return handle;\n}\n", out);
    xl6OutputPuts("void* xl6_xrt_log_default(void)\n{\n    return xl6_log_wrap_owned(xlogAddRef(xlogDefault()));\n}\n", out);
    xl6OutputPuts("long long xl6_xrt_log_destroy(void* handle)\n{\n    xl6_log_logger* value = (xl6_log_logger*)handle;\n    if ( value != 0 ) { xlogRelease(value->logger); xrtFree(value); }\n    return 0;\n}\n", out);
    xl6OutputPuts("int xl6_xrt_log_set_default(void* handle)\n{\n    xlogger* logger = xl6_log_get(handle);\n    if ( logger == 0 ) { return 0; }\n    xlogSetDefault(logger);\n    return 1;\n}\n", out);
    xl6OutputPuts("void xl6_xrt_log_set_level(void* handle, long long level)\n{\n    xlogSetLevel(xl6_log_get(handle), xl6_log_level(level));\n}\n", out);
    xl6OutputPuts("long long xl6_xrt_log_get_level(void* handle)\n{\n    return (long long)xlogGetLevel(xl6_log_get(handle));\n}\n\n", out);

    xl6OutputPuts("static xl6_log_appender* xl6_log_appender_alloc(xlogger* logger)\n{\n    xl6_log_appender* value;\n    if ( logger == 0 ) { return 0; }\n    value = (xl6_log_appender*)xrtCalloc(1u, sizeof(*value));\n    if ( value != 0 ) { value->logger = xlogAddRef(logger); }\n    return value;\n}\n", out);
    xl6OutputPuts("static void* xl6_log_appender_finish(xl6_log_appender* value, xlogappender* appender, int removeOnDestroy)\n{\n    if ( value == 0 ) { return 0; }\n    if ( appender == 0 ) { xlogRelease(value->logger); xrtFree(value); return 0; }\n    value->appender = appender;\n    value->removeOnDestroy = removeOnDestroy;\n    return value;\n}\n", out);
    xl6OutputPuts("void* xl6_xrt_log_appender_wrap_handle(void* handle)\n{\n    return handle;\n}\n", out);
    xl6OutputPuts("long long xl6_xrt_log_appender_destroy(void* handle)\n{\n    xl6_log_appender* value = (xl6_log_appender*)handle;\n    if ( value != 0 ) {\n        if ( value->removeOnDestroy && value->logger != 0 && value->appender != 0 ) {\n            xlogRemoveAppender(value->logger, value->appender);\n        }\n        xlogRelease(value->logger);\n        xrtFree(value);\n    }\n    return 0;\n}\n", out);
    xl6OutputPuts("void* xl6_xrt_log_console_appender(void* handle, long long level, int color, long long format)\n{\n    xlogger* logger = xl6_log_get(handle);\n    xl6_log_appender* value = xl6_log_appender_alloc(logger);\n    xlogappender* appender;\n    if ( value == 0 ) { return 0; }\n    appender = xlogAddConsole(logger, xl6_log_level(level), color ? true : false);\n    if ( appender != 0 ) { xlogAppenderSetFormat(appender, xl6_log_format(format)); }\n    return xl6_log_appender_finish(value, appender, 0);\n}\n", out);
    xl6OutputPuts("void* xl6_xrt_log_file_appender(void* handle, const char* path, long long level, long long format)\n{\n    xlogger* logger = xl6_log_get(handle);\n    xl6_log_appender* value = xl6_log_appender_alloc(logger);\n    xlogappender* appender;\n    if ( value == 0 ) { return 0; }\n    appender = xlogAddFile(logger, path != 0 ? path : \"\", xl6_log_level(level));\n    if ( appender != 0 ) { xlogAppenderSetFormat(appender, xl6_log_format(format)); }\n    return xl6_log_appender_finish(value, appender, 0);\n}\n", out);
    xl6OutputPuts("void* xl6_xrt_log_rolling_file_appender(void* handle, const char* path, long long maxSize, long long maxBackup, long long level, long long format)\n{\n    xlogger* logger = xl6_log_get(handle);\n    xl6_log_appender* value = xl6_log_appender_alloc(logger);\n    uint64_t size = maxSize > 0 ? (uint64_t)maxSize : 10485760u;\n    uint32_t backup = maxBackup > 0 && maxBackup <= 1024 ? (uint32_t)maxBackup : 5u;\n    xlogappender* appender;\n    if ( value == 0 ) { return 0; }\n    appender = xlogAddRollingFile(logger, path != 0 ? path : \"\", size, backup, xl6_log_level(level));\n    if ( appender != 0 ) { xlogAppenderSetFormat(appender, xl6_log_format(format)); }\n    return xl6_log_appender_finish(value, appender, 0);\n}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_log_appender_name(void* handle)\n{\n    xl6_log_appender* value = (xl6_log_appender*)handle;\n    const char* name = value != 0 ? xlogAppenderName(value->appender) : \"\";\n    return xrtCopyStr(name != 0 ? name : \"\", 0u);\n}\n", out);
    xl6OutputPuts("void xl6_xrt_log_appender_set_level(void* handle, long long level)\n{\n    xl6_log_appender* value = (xl6_log_appender*)handle;\n    if ( value != 0 ) { xlogAppenderSetLevel(value->appender, xl6_log_level(level)); }\n}\n", out);
    xl6OutputPuts("long long xl6_xrt_log_appender_get_level(void* handle)\n{\n    xl6_log_appender* value = (xl6_log_appender*)handle;\n    return value != 0 ? (long long)xlogAppenderGetLevel(value->appender) : 6;\n}\n", out);
    xl6OutputPuts("void xl6_xrt_log_appender_set_format(void* handle, long long format)\n{\n    xl6_log_appender* value = (xl6_log_appender*)handle;\n    if ( value != 0 ) { xlogAppenderSetFormat(value->appender, xl6_log_format(format)); }\n}\n", out);
    xl6OutputPuts("long long xl6_xrt_log_appender_get_format(void* handle)\n{\n    xl6_log_appender* value = (xl6_log_appender*)handle;\n    return value != 0 ? (long long)xlogAppenderGetFormat(value->appender) : 0;\n}\n", out);
    xl6OutputPuts("void xl6_xrt_log_appender_set_color(void* handle, int color)\n{\n    xl6_log_appender* value = (xl6_log_appender*)handle;\n    if ( value != 0 ) { xlogAppenderSetColor(value->appender, color ? true : false); }\n}\n", out);
    xl6OutputPuts("int xl6_xrt_log_appender_get_color(void* handle)\n{\n    xl6_log_appender* value = (xl6_log_appender*)handle;\n    return value != 0 && xlogAppenderGetColor(value->appender);\n}\n\n", out);

    /* 普通配置函数继续提供最短路径，对象 API 用于后续修改 appender。 */
    xl6OutputPuts("int xl6_xrt_log_add_console(void* handle, long long level, int color, long long format)\n{\n    void* value = xl6_xrt_log_console_appender(handle, level, color, format);\n    if ( value == 0 ) { return 0; }\n    xl6_xrt_log_appender_destroy(value);\n    return 1;\n}\n", out);
    xl6OutputPuts("int xl6_xrt_log_add_file(void* handle, const char* path, long long level, long long format)\n{\n    void* value = xl6_xrt_log_file_appender(handle, path, level, format);\n    if ( value == 0 ) { return 0; }\n    xl6_xrt_log_appender_destroy(value);\n    return 1;\n}\n", out);
    xl6OutputPuts("int xl6_xrt_log_add_rolling_file(void* handle, const char* path, long long maxSize, long long maxBackup, long long level, long long format)\n{\n    void* value = xl6_xrt_log_rolling_file_appender(handle, path, maxSize, maxBackup, level, format);\n    if ( value == 0 ) { return 0; }\n    xl6_xrt_log_appender_destroy(value);\n    return 1;\n}\n\n", out);

    xl6OutputPuts("#ifdef XL6_FUNCTION_VAR_RUNTIME\n", out);
    xl6OutputPuts("typedef struct { XL6_Function fn; } xl6_log_callback;\n", out);
    xl6OutputPuts("xvalue xvoCreateTable(void);\nxvalue xvoCreateInt(long long value);\nxvalue xvoCreateTime(long long value);\nxvalue xvoCreateText(void* text, uint32_t size, bool colloc);\nbool xvoTableSetValue(xvalue table, const void* key, uint32_t keySize, xvalue value, bool colloc);\nvoid xvoUnref(xvalue value);\n", out);
    xl6OutputPuts("static void xl6_log_event_set(xvalue event, const char* name, xvalue value)\n{\n    if ( value != 0 && !xvoTableSetValue(event, name, 0u, value, true) ) { xvoUnref(value); }\n}\n", out);
    xl6OutputPuts("static void xl6_log_event_set_text(xvalue event, const char* name, const char* text)\n{\n    xl6_log_event_set(event, name, xvoCreateText((void*)(text != 0 ? text : \"\"), 0u, false));\n}\n", out);
    xl6OutputPuts("static xvalue xl6_log_event_value(const xlogevent* event)\n{\n    xvalue value = xvoCreateTable();\n    if ( value == 0 || event == 0 ) { return value; }\n    xl6_log_event_set(value, \"time\", xvoCreateTime(event->iTime));\n    xl6_log_event_set(value, \"level\", xvoCreateInt(event->iLevel));\n    xl6_log_event_set_text(value, \"logger\", event->sLogger);\n    xl6_log_event_set_text(value, \"file\", event->sFile);\n    xl6_log_event_set(value, \"line\", xvoCreateInt(event->iLine));\n    xl6_log_event_set_text(value, \"function\", event->sFunc);\n    xl6_log_event_set(value, \"threadId\", xvoCreateInt((long long)event->iThreadId));\n    xl6_log_event_set_text(value, \"message\", event->sMessage);\n    return value;\n}\n", out);
    xl6OutputPuts("static void xl6_log_callback_free(void* userData)\n{\n    xl6_log_callback* callback = (xl6_log_callback*)userData;\n    if ( callback != 0 ) { xl6_function_release_value(callback->fn); xrtFree(callback); }\n}\n", out);
    xl6OutputPuts("static void xl6_log_callback_call(const xlogevent* event, void* userData)\n{\n    xl6_log_callback* callback = (xl6_log_callback*)userData;\n    xvalue value;\n    xvalue result;\n    if ( callback == 0 ) { return; }\n    value = xl6_log_event_value(event);\n    if ( value == 0 ) { return; }\n    result = xl6_function_call_var(callback->fn, 1, value);\n    xvoUnref(result);\n}\n", out);
    xl6OutputPuts("void* xl6_xrt_log_custom_appender(void* handle, const char* name, XL6_Function fn, long long level)\n{\n    xlogger* logger = xl6_log_get(handle);\n    xl6_log_appender* value = xl6_log_appender_alloc(logger);\n    xl6_log_callback* callback;\n    xlogappender* appender;\n    if ( value == 0 ) { return 0; }\n    callback = (xl6_log_callback*)xrtCalloc(1u, sizeof(*callback));\n    if ( callback == 0 ) { return xl6_log_appender_finish(value, 0, 1); }\n    callback->fn = xl6_function_copy_value(fn);\n    appender = xlogAddCustomEx(logger, name != 0 ? name : \"xlang\", xl6_log_level(level), xl6_log_callback_call, callback, xl6_log_callback_free);\n    if ( appender == 0 ) { xl6_log_callback_free(callback); }\n    return xl6_log_appender_finish(value, appender, 1);\n}\n", out);
    xl6OutputPuts("#endif\n\n", out);

    xl6OutputPuts("void xl6_xrt_log_write(void* handle, long long level, const char* message)\n{\n    xlogWrite(xl6_log_get(handle), xl6_log_level(level), \"\", 0u, \"\", \"%s\", message != 0 ? message : \"\");\n}\n", out);
    xl6OutputPuts("void xl6_xrt_log_flush(void* handle)\n{\n    xlogFlush(xl6_log_get(handle));\n}\n\n", out);

    xl6OutputPuts("static xlogger* xl6_log_default_logger(void)\n{\n    return xlogDefault();\n}\n", out);
    xl6OutputPuts("void xl6_xrt_log_default_set_level(long long level)\n{\n    xlogSetLevel(xl6_log_default_logger(), xl6_log_level(level));\n}\n", out);
    xl6OutputPuts("long long xl6_xrt_log_default_get_level(void)\n{\n    return (long long)xlogGetLevel(xl6_log_default_logger());\n}\n", out);
    xl6OutputPuts("bool xl6_xrt_log_default_add_console(long long level, bool color, long long format)\n{\n    xlogappender* value = xlogAddConsole(xl6_log_default_logger(), xl6_log_level(level), color);\n    if ( value == 0 ) { return false; }\n    xlogAppenderSetFormat(value, xl6_log_format(format));\n    return true;\n}\n", out);
    xl6OutputPuts("bool xl6_xrt_log_default_add_file(const char* path, long long level, long long format)\n{\n    xlogappender* value = xlogAddFile(xl6_log_default_logger(), path != 0 ? path : \"\", xl6_log_level(level));\n    if ( value == 0 ) { return false; }\n    xlogAppenderSetFormat(value, xl6_log_format(format));\n    return true;\n}\n", out);
    xl6OutputPuts("bool xl6_xrt_log_default_add_rolling_file(const char* path, long long maxSize, long long maxBackup, long long level, long long format)\n{\n    uint64_t size = maxSize > 0 ? (uint64_t)maxSize : 10485760u;\n    uint32_t backup = maxBackup > 0 && maxBackup <= 1024 ? (uint32_t)maxBackup : 5u;\n    xlogappender* value = xlogAddRollingFile(xl6_log_default_logger(), path != 0 ? path : \"\", size, backup, xl6_log_level(level));\n    if ( value == 0 ) { return false; }\n    xlogAppenderSetFormat(value, xl6_log_format(format));\n    return true;\n}\n", out);
    xl6OutputPuts("void xl6_xrt_log_default_write(long long level, const char* message)\n{\n    xlogWrite(xl6_log_default_logger(), xl6_log_level(level), \"\", 0u, \"\", \"%s\", message != 0 ? message : \"\");\n}\n", out);
    xl6OutputPuts("void xl6_xrt_log_default_flush(void)\n{\n    xlogFlush(xl6_log_default_logger());\n}\n", out);
    xl6OutputPuts("void* xl6_xrt_log_level_name(long long level)\n{\n    const char* name = xlogLevelName(xl6_log_level(level));\n    return xrtCopyStr(name != 0 ? name : \"UNKN\", 0u);\n}\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
