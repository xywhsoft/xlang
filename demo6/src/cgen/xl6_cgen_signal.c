static void xl6CgenEmitSignalRuntime(XL6_Output* out, int hasChannelBridge)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_SIGNAL_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_SIGNAL_RUNTIME\n", out);
    xl6OutputPuts("#include <stddef.h>\n#include <stdint.h>\n#include <stdbool.h>\n", out);
    xl6OutputPuts("#ifndef XL6_XVALUE_TYPEDEF\n#define XL6_XVALUE_TYPEDEF\ntypedef struct xvalue_struct* xvalue;\n#endif\n", out);
    xl6OutputPuts("typedef struct xrtsignal_handle xrtsignal_handle;\n", out);
    xl6OutputPuts("typedef struct xrtsignal_event { int iCode; int iPlatformCode; uint64_t iCount; long long iTime; const char* sName; } xrtsignal_event;\n", out);
    xl6OutputPuts("typedef void (*xrtsignal_fn)(const xrtsignal_event* pEvent, void* pUserData);\n", out);
    xl6OutputPuts("typedef void (*xrtsignal_free_fn)(void* pUserData);\n\n", out);

    xl6OutputPuts("void* xrtCalloc(size_t iCount, size_t iSize);\nvoid xrtFree(void* pMem);\nchar* xrtCopyStr(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("bool xrtSignalSupported(int iCode);\nconst char* xrtSignalName(int iCode);\n", out);
    xl6OutputPuts("xrtsignal_handle* xrtSignalOn(int iCode, xrtsignal_fn Proc, void* pUserData, xrtsignal_free_fn FreeProc);\n", out);
    xl6OutputPuts("xrtsignal_handle* xrtSignalOnce(int iCode, xrtsignal_fn Proc, void* pUserData, xrtsignal_free_fn FreeProc);\n", out);
    xl6OutputPuts("bool xrtSignalOff(xrtsignal_handle* pHandle);\nvoid xrtSignalHandleRelease(xrtsignal_handle* pHandle);\nbool xrtSignalHandleActive(xrtsignal_handle* pHandle);\nint xrtSignalHandleCode(xrtsignal_handle* pHandle);\n", out);
    xl6OutputPuts("bool xrtSignalRestore(int iCode);\nvoid xrtSignalRestoreAll(void);\nbool xrtSignalRaise(int iCode);\nuint64_t xrtSignalCount(int iCode);\nbool xrtSignalReceived(int iCode);\nvoid xrtSignalClear(int iCode);\n", out);
    xl6OutputPuts("xvalue xvoCreateTable(void);\nxvalue xvoCreateInt(long long value);\nxvalue xvoCreateTime(long long value);\nxvalue xvoCreateText(void* text, uint32_t size, bool colloc);\nbool xvoTableSetValue(xvalue table, const void* key, uint32_t keySize, xvalue value, bool colloc);\nvoid xvoUnref(xvalue value);\n\n", out);

    xl6OutputPuts("typedef struct { XL6_Function fn; int argc; } xl6_signal_callback;\n", out);
    xl6OutputPuts("static void xl6_signal_event_set(xvalue event, const char* name, xvalue value)\n{\n    if ( value != 0 && !xvoTableSetValue(event, name, 0u, value, true) ) { xvoUnref(value); }\n}\n", out);
    xl6OutputPuts("static void xl6_signal_event_set_text(xvalue event, const char* name, const char* text)\n{\n    xl6_signal_event_set(event, name, xvoCreateText((void*)(text != 0 ? text : \"\"), 0u, false));\n}\n", out);
    xl6OutputPuts("static xvalue xl6_signal_event_value(const xrtsignal_event* event)\n{\n    xvalue value = xvoCreateTable();\n    if ( value == 0 || event == 0 ) { return value; }\n    xl6_signal_event_set(value, \"code\", xvoCreateInt(event->iCode));\n    xl6_signal_event_set(value, \"platformCode\", xvoCreateInt(event->iPlatformCode));\n    xl6_signal_event_set(value, \"count\", xvoCreateInt((long long)event->iCount));\n    xl6_signal_event_set(value, \"time\", xvoCreateTime(event->iTime));\n    xl6_signal_event_set_text(value, \"name\", event->sName);\n    return value;\n}\n", out);
    xl6OutputPuts("static void xl6_signal_callback_free(void* userData)\n{\n    xl6_signal_callback* callback = (xl6_signal_callback*)userData;\n    if ( callback != 0 ) { xl6_function_release_value(callback->fn); xrtFree(callback); }\n}\n", out);
    xl6OutputPuts("static void xl6_signal_callback_call(const xrtsignal_event* event, void* userData)\n{\n    xl6_signal_callback* callback = (xl6_signal_callback*)userData;\n    xvalue result;\n    if ( callback == 0 ) { return; }\n    if ( callback->argc <= 0 ) {\n        result = xl6_function_call_var(callback->fn, 0);\n    } else {\n        xvalue value = xl6_signal_event_value(event);\n        if ( value == 0 ) { return; }\n        result = xl6_function_call_var(callback->fn, 1, value);\n    }\n    xvoUnref(result);\n}\n", out);
    xl6OutputPuts("static void* xl6_signal_register(long long code, XL6_Function fn, int argc, int once)\n{\n    xl6_signal_callback* callback = (xl6_signal_callback*)xrtCalloc(1u, sizeof(*callback));\n    xrtsignal_handle* handle;\n    if ( callback == 0 ) { return 0; }\n    callback->fn = xl6_function_copy_value(fn);\n    callback->argc = argc > 0 ? 1 : 0;\n    handle = once ? xrtSignalOnce((int)code, xl6_signal_callback_call, callback, xl6_signal_callback_free) : xrtSignalOn((int)code, xl6_signal_callback_call, callback, xl6_signal_callback_free);\n    if ( handle == 0 ) { xl6_signal_callback_free(callback); return 0; }\n    return handle;\n}\n\n", out);

    if ( hasChannelBridge ) {
        xl6OutputPuts("typedef struct { xchannel channel; } xl6_signal_channel_callback;\n", out);
        xl6OutputPuts("static void xl6_signal_channel_callback_free(void* userData)\n{\n    xrtFree(userData);\n}\n", out);
        xl6OutputPuts("static void xl6_signal_channel_callback_call(const xrtsignal_event* event, void* userData)\n{\n    xl6_signal_channel_callback* callback = (xl6_signal_channel_callback*)userData;\n    xvalue value;\n    if ( callback == 0 || callback->channel == 0 ) { return; }\n    value = xl6_signal_event_value(event);\n    if ( value == 0 ) { return; }\n    (void)xl6_channel_send_xvalue(callback->channel, value, 0, \"\", 0, 0);\n}\n", out);
        xl6OutputPuts("void* xl6_xrt_signal_send_to(int64_t code, xchannel channel)\n{\n    xl6_signal_channel_callback* callback;\n    xrtsignal_handle* handle;\n    if ( channel == 0 ) { return 0; }\n    callback = (xl6_signal_channel_callback*)xrtCalloc(1u, sizeof(*callback));\n    if ( callback == 0 ) { return 0; }\n    callback->channel = channel;\n    handle = xrtSignalOn((int)code, xl6_signal_channel_callback_call, callback, xl6_signal_channel_callback_free);\n    if ( handle == 0 ) { xl6_signal_channel_callback_free(callback); return 0; }\n    return handle;\n}\n\n", out);
    }

    xl6OutputPuts("void* xl6_xrt_signal_handle_wrap(void* handle)\n{\n    return handle;\n}\n", out);
    xl6OutputPuts("long long xl6_xrt_signal_handle_destroy(void* handle)\n{\n    if ( handle != 0 ) { xrtSignalHandleRelease((xrtsignal_handle*)handle); }\n    return 0;\n}\n", out);
    xl6OutputPuts("int xl6_xrt_signal_handle_close(void* handle)\n{\n    return xrtSignalOff((xrtsignal_handle*)handle) ? 1 : 0;\n}\n", out);
    xl6OutputPuts("int xl6_xrt_signal_handle_active(void* handle)\n{\n    return xrtSignalHandleActive((xrtsignal_handle*)handle) ? 1 : 0;\n}\n", out);
    xl6OutputPuts("long long xl6_xrt_signal_handle_code(void* handle)\n{\n    return (long long)xrtSignalHandleCode((xrtsignal_handle*)handle);\n}\n", out);
    xl6OutputPuts("void* xl6_xrt_signal_on0(int64_t code, XL6_Function fn, bool once)\n{\n    return xl6_signal_register(code, fn, 0, once ? 1 : 0);\n}\n", out);
    xl6OutputPuts("void* xl6_xrt_signal_on1(int64_t code, XL6_Function fn, bool once)\n{\n    return xl6_signal_register(code, fn, 1, once ? 1 : 0);\n}\n", out);
    xl6OutputPuts("bool xl6_xrt_signal_supported(int64_t code)\n{\n    return xrtSignalSupported((int)code) ? true : false;\n}\n", out);
    xl6OutputPuts("void* xl6_xrt_signal_name(int64_t code)\n{\n    const char* name = xrtSignalName((int)code);\n    return xrtCopyStr(name != 0 ? name : \"\", 0u);\n}\n", out);
    xl6OutputPuts("bool xl6_xrt_signal_restore(int64_t code)\n{\n    return xrtSignalRestore((int)code) ? true : false;\n}\n", out);
    xl6OutputPuts("void xl6_xrt_signal_reset(void)\n{\n    xrtSignalRestoreAll();\n}\n", out);
    xl6OutputPuts("bool xl6_xrt_signal_raise(int64_t code)\n{\n    return xrtSignalRaise((int)code) ? true : false;\n}\n", out);
    xl6OutputPuts("int64_t xl6_xrt_signal_count(int64_t code)\n{\n    return (int64_t)xrtSignalCount((int)code);\n}\n", out);
    xl6OutputPuts("bool xl6_xrt_signal_received(int64_t code)\n{\n    return xrtSignalReceived((int)code) ? true : false;\n}\n", out);
    xl6OutputPuts("void xl6_xrt_signal_clear(int64_t code)\n{\n    xrtSignalClear((int)code);\n}\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
