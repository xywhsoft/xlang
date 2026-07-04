static void xl6CgenEmitLogRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_LOG_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_LOG_RUNTIME\n", out);
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("#include <stdint.h>\n", out);
    xl6OutputPuts("typedef struct xlogger xlogger;\n", out);
    xl6OutputPuts("typedef struct xlogappender xlogappender;\n", out);
    xl6OutputPuts("typedef struct { xlogger* logger; int owner; } xl6_log_logger;\n\n", out);

    xl6OutputPuts("void* xrtCalloc(size_t iCount, size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* pMem);\n", out);
    xl6OutputPuts("char* xrtCopyStr(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("xlogger* xlogCreate(const char* sName);\n", out);
    xl6OutputPuts("void xlogDestroy(xlogger* pLogger);\n", out);
    xl6OutputPuts("xlogger* xlogDefault(void);\n", out);
    xl6OutputPuts("void xlogSetLevel(xlogger* pLogger, int iLevel);\n", out);
    xl6OutputPuts("int xlogGetLevel(xlogger* pLogger);\n", out);
    xl6OutputPuts("xlogappender* xlogAddConsole(xlogger* pLogger, int iMinLevel, bool bColor);\n", out);
    xl6OutputPuts("xlogappender* xlogAddFile(xlogger* pLogger, const char* sPath, int iMinLevel);\n", out);
    xl6OutputPuts("xlogappender* xlogAddRollingFile(xlogger* pLogger, const char* sPath, uint64_t iMaxSize, uint32_t iMaxBackup, int iMinLevel);\n", out);
    xl6OutputPuts("void xlogAppenderSetFormat(xlogappender* pAppender, int iFormat);\n", out);
    xl6OutputPuts("const char* xlogLevelName(int iLevel);\n", out);
    xl6OutputPuts("void xlogWrite(xlogger* pLogger, int iLevel, const char* sFile, uint32_t iLine, const char* sFunc, const char* sFmt, ...);\n", out);
    xl6OutputPuts("void xlogFlush(xlogger* pLogger);\n\n", out);

    xl6OutputPuts("static int xl6_log_level(long long level)\n{\n", out);
    xl6OutputPuts("    if ( level < 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( level > 6 ) { return 6; }\n", out);
    xl6OutputPuts("    return (int)level;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_log_format(long long format)\n{\n", out);
    xl6OutputPuts("    if ( format < 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( format > 2 ) { return 2; }\n", out);
    xl6OutputPuts("    return (int)format;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xl6_log_logger* xl6_log_wrap(xlogger* logger, int owner)\n{\n", out);
    xl6OutputPuts("    xl6_log_logger* value;\n", out);
    xl6OutputPuts("    if ( logger == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    value = (xl6_log_logger*)xrtCalloc(1u, sizeof(xl6_log_logger));\n", out);
    xl6OutputPuts("    if ( value == 0 ) { if ( owner ) { xlogDestroy(logger); } return 0; }\n", out);
    xl6OutputPuts("    value->logger = logger;\n", out);
    xl6OutputPuts("    value->owner = owner;\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xlogger* xl6_log_get(void* handle)\n{\n", out);
    xl6OutputPuts("    return handle != 0 ? ((xl6_log_logger*)handle)->logger : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_log_create(const char* name)\n{\n", out);
    xl6OutputPuts("    return xl6_log_wrap(xlogCreate((const char*)(name != 0 ? name : \"xlang\")), 1);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_log_wrap_handle(void* handle)\n{\n", out);
    xl6OutputPuts("    return handle;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_log_default(void)\n{\n", out);
    xl6OutputPuts("    return xl6_log_wrap(xlogDefault(), 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_log_destroy(void* handle)\n{\n", out);
    xl6OutputPuts("    xl6_log_logger* value = (xl6_log_logger*)handle;\n", out);
    xl6OutputPuts("    if ( value == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( value->owner && value->logger != 0 ) { xlogDestroy(value->logger); }\n", out);
    xl6OutputPuts("    xrtFree(value);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_log_set_level(void* handle, long long level)\n{\n", out);
    xl6OutputPuts("    xlogSetLevel(xl6_log_get(handle), xl6_log_level(level));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_log_get_level(void* handle)\n{\n", out);
    xl6OutputPuts("    return (long long)xlogGetLevel(xl6_log_get(handle));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_log_add_console(void* handle, long long level, int color, long long format)\n{\n", out);
    xl6OutputPuts("    xlogappender* appender = xlogAddConsole(xl6_log_get(handle), xl6_log_level(level), color ? true : false);\n", out);
    xl6OutputPuts("    if ( appender == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xlogAppenderSetFormat(appender, xl6_log_format(format));\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_log_add_file(void* handle, const char* path, long long level, long long format)\n{\n", out);
    xl6OutputPuts("    xlogappender* appender = xlogAddFile(xl6_log_get(handle), (const char*)(path != 0 ? path : \"\"), xl6_log_level(level));\n", out);
    xl6OutputPuts("    if ( appender == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xlogAppenderSetFormat(appender, xl6_log_format(format));\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_log_add_rolling_file(void* handle, const char* path, long long maxSize, long long maxBackup, long long level, long long format)\n{\n", out);
    xl6OutputPuts("    uint64_t size = maxSize > 0 ? (uint64_t)maxSize : 10485760u;\n", out);
    xl6OutputPuts("    uint32_t backup = maxBackup > 0 && maxBackup <= 1024 ? (uint32_t)maxBackup : 5u;\n", out);
    xl6OutputPuts("    xlogappender* appender = xlogAddRollingFile(xl6_log_get(handle), (const char*)(path != 0 ? path : \"\"), size, backup, xl6_log_level(level));\n", out);
    xl6OutputPuts("    if ( appender == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xlogAppenderSetFormat(appender, xl6_log_format(format));\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_log_write(void* handle, long long level, const char* message)\n{\n", out);
    xl6OutputPuts("    xlogWrite(xl6_log_get(handle), xl6_log_level(level), \"\", 0u, \"\", \"%s\", message != 0 ? message : \"\");\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_log_flush(void* handle)\n{\n", out);
    xl6OutputPuts("    xlogFlush(xl6_log_get(handle));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xlogger* xl6_log_default_logger(void)\n{\n", out);
    xl6OutputPuts("    return xlogDefault();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_log_default_set_level(long long level)\n{\n", out);
    xl6OutputPuts("    xlogSetLevel(xl6_log_default_logger(), xl6_log_level(level));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_log_default_get_level(void)\n{\n", out);
    xl6OutputPuts("    return (long long)xlogGetLevel(xl6_log_default_logger());\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_log_default_add_console(long long level, bool color, long long format)\n{\n", out);
    xl6OutputPuts("    xlogappender* appender = xlogAddConsole(xl6_log_default_logger(), xl6_log_level(level), color ? true : false);\n", out);
    xl6OutputPuts("    if ( appender == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xlogAppenderSetFormat(appender, xl6_log_format(format));\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_log_default_add_file(const char* path, long long level, long long format)\n{\n", out);
    xl6OutputPuts("    xlogappender* appender = xlogAddFile(xl6_log_default_logger(), (const char*)(path != 0 ? path : \"\"), xl6_log_level(level));\n", out);
    xl6OutputPuts("    if ( appender == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xlogAppenderSetFormat(appender, xl6_log_format(format));\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_log_default_add_rolling_file(const char* path, long long maxSize, long long maxBackup, long long level, long long format)\n{\n", out);
    xl6OutputPuts("    uint64_t size = maxSize > 0 ? (uint64_t)maxSize : 10485760u;\n", out);
    xl6OutputPuts("    uint32_t backup = maxBackup > 0 && maxBackup <= 1024 ? (uint32_t)maxBackup : 5u;\n", out);
    xl6OutputPuts("    xlogappender* appender = xlogAddRollingFile(xl6_log_default_logger(), (const char*)(path != 0 ? path : \"\"), size, backup, xl6_log_level(level));\n", out);
    xl6OutputPuts("    if ( appender == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xlogAppenderSetFormat(appender, xl6_log_format(format));\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_log_default_write(long long level, const char* message)\n{\n", out);
    xl6OutputPuts("    xlogWrite(xl6_log_default_logger(), xl6_log_level(level), \"\", 0u, \"\", \"%s\", message != 0 ? message : \"\");\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_log_default_flush(void)\n{\n", out);
    xl6OutputPuts("    xlogFlush(xl6_log_default_logger());\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_log_level_name(long long level)\n{\n", out);
    xl6OutputPuts("    const char* name = xlogLevelName(xl6_log_level(level));\n", out);
    xl6OutputPuts("    return xrtCopyStr(name != 0 ? name : \"UNKN\", name != 0 ? 0u : 4u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#endif\n\n", out);
}
