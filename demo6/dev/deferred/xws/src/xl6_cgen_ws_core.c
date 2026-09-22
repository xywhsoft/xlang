static const char xl6WebSocketCoreSource[] = XL6_WS_SOURCE(
static xstrview xl6_ws_view(const char* text)
{
    xstrview view;
    view.Data = text;
    view.Size = text != 0 ? strlen(text) : 0u;
    return view;
}

static xbytesview xl6_ws_bytes(const void* data, size_t size)
{
    xbytesview view;
    view.Data = (const unsigned char*)data;
    view.Size = size;
    return view;
}

static bool xl6_ws_buffer_append(xbuffer* buffer, xbytesview data)
{
    XL6_XrtBytesView view;
    view.Data = data.Data;
    view.Size = data.Size;
    return xrtBufferAppend(buffer, view);
}

static char* xl6_ws_copy_view(xstrview view)
{
    return xl6_xrt_string_dup(view.Data != 0 ? view.Data : "", view.Size);
}

static bool xl6_ws_function_valid(const XL6_Function* function)
{
    return function != 0 && (function->invoke_var != 0 || function->invoke_xcall != 0);
}

static void xl6_ws_function_clear(XL6_Function* function)
{
    if ( xl6_ws_function_valid(function) ) { xl6_function_release_value(*function); }
    memset(function, 0, sizeof(*function));
}

static bool xl6_ws_function_set(XL6_Function* target, XL6_Function function, bool locked)
{
    if ( target == 0 || locked || !xl6_ws_function_valid(&function) ) { return false; }
    xl6_ws_function_clear(target);
    *target = xl6_function_copy_value(function);
    return xl6_ws_function_valid(target);
}

static bool xl6_ws_function_set_value(XL6_Function* target, xvalue* value, bool locked)
{
    xrtcallable* callable;
    XL6_Function function;
    bool result;

    if ( target == 0 || value == 0 || locked ) { return false; }
    callable = xrtValueGetCallable(value);
    if ( callable == 0 ) { return false; }

    function = xl6_function_from_callable(callable);
    result = xl6_ws_function_set(target, function, false);
    xl6_function_release_value(function);
    return result;
}

static void xl6_ws_callbacks_unit(XL6_WsCallbacks* callbacks)
{
    if ( callbacks == 0 ) { return; }
    xl6_ws_function_clear(&callbacks->Open);
    xl6_ws_function_clear(&callbacks->Text);
    xl6_ws_function_clear(&callbacks->Binary);
    xl6_ws_function_clear(&callbacks->Close);
    xl6_ws_function_clear(&callbacks->Error);
    xl6_ws_function_clear(&callbacks->Ping);
    xl6_ws_function_clear(&callbacks->Pong);
    xl6_ws_function_clear(&callbacks->Backpressure);
    xl6_ws_function_clear(&callbacks->Writable);
    xl6_ws_function_clear(&callbacks->Drain);
    xl6_ws_function_clear(&callbacks->Handshake);
}

static void xl6_ws_result_release(xvalue* result)
{
    if ( result != 0 ) { xrtValueRelease(result); }
}

static xvalue* xl6_ws_text_value(const void* data, size_t size)
{
    if ( size > UINT32_MAX ) { size = UINT32_MAX; }
    return xl6_xrt_value_text_create((void*)(data != 0 ? data : ""), (uint32_t)size, false);
}

static void xl6_ws_error_clear(XL6_WsErrorSnapshot* snapshot)
{
    if ( snapshot == 0 ) { return; }
    xrtErrorFree(snapshot->Error);
    memset(snapshot, 0, sizeof(*snapshot));
}

static void xl6_ws_error_set(XL6_WsErrorSnapshot* snapshot, int result, int httpStatus, uint16_t closeCode, const xerror* error)
{
    if ( snapshot == 0 ) { return; }
    xl6_ws_error_clear(snapshot);
    snapshot->Error = xrtErrorRef(error);
    snapshot->Result = result;
    snapshot->HttpStatus = httpStatus;
    snapshot->CloseCode = closeCode;
    if ( error != 0 && xrtErrorDomain(error) != 0 && strcmp(xrtErrorDomain(error), "xrt.websocket.handshake") == 0 ) {
        snapshot->ProtocolError = xrtErrorCode(error);
    }
}

static XL6_WsErrorSnapshot* xl6_ws_error_clone(const XL6_WsErrorSnapshot* source)
{
    XL6_WsErrorSnapshot* snapshot;
    if ( source == 0 ) { return 0; }
    snapshot = (XL6_WsErrorSnapshot*)xrtCalloc(1u, sizeof(*snapshot));
    if ( snapshot == 0 ) { return 0; }
    *snapshot = *source;
    snapshot->Error = xrtErrorRef(source->Error);
    return snapshot;
}

static void xl6_ws_close_clear(XL6_WsCloseSnapshot* snapshot)
{
    if ( snapshot == 0 ) { return; }
    xrtFree(snapshot->Reason);
    memset(snapshot, 0, sizeof(*snapshot));
}

static bool xl6_ws_close_set(XL6_WsCloseSnapshot* snapshot, const xwsconnclose* closeInfo)
{
    char* reason;
    if ( snapshot == 0 || closeInfo == 0 ) { return false; }
    reason = xl6_ws_copy_view(closeInfo->Reason);
    if ( reason == 0 ) { return false; }
    xl6_ws_close_clear(snapshot);
    snapshot->Close = *closeInfo;
    snapshot->Reason = reason;
    snapshot->Close.Reason = xl6_ws_view(reason);
    return true;
}

static XL6_WsCloseSnapshot* xl6_ws_close_clone(const XL6_WsCloseSnapshot* source)
{
    XL6_WsCloseSnapshot* snapshot;
    if ( source == 0 ) { return 0; }
    snapshot = (XL6_WsCloseSnapshot*)xrtCalloc(1u, sizeof(*snapshot));
    if ( snapshot == 0 || !xl6_ws_close_set(snapshot, &source->Close) ) {
        xrtFree(snapshot);
        return 0;
    }
    return snapshot;
}

static void xl6_ws_future_value_free(void* value, void* data)
{
    (void)data;
    xrtFree(value);
}

static bool xl6_ws_promise_bool(xpromise* promise, bool value)
{
    int* result;
    if ( promise == 0 ) { return false; }
    result = (int*)xrtMalloc(sizeof(*result));
    if ( result == 0 ) { return false; }
    *result = value ? 1 : 0;
    if ( !xrtPromiseResolveOwned(promise, result, xl6_ws_future_value_free, 0) ) {
        xrtFree(result);
        return false;
    }
    return true;
}

static int xl6_ws_future_map_bool(const xl6_future_result* input, void* argument, xl6_future_result* output)
{
    int* value;
    (void)input;
    (void)argument;
    if ( output == 0 ) { return -1; }
    value = (int*)xrtMalloc(sizeof(*value));
    if ( value == 0 ) { return -1; }
    *value = 1;
    memset(output, 0, sizeof(*output));
    output->pValue = value;
    output->iFlags = XL6_FUTURE_OWN_VALUE;
    output->pfnFreeValue = xrtFree;
    return 0;
}

static xfuture* xl6_ws_future_bool(xfuture* source)
{
    xfuture* mapped;
    if ( source == 0 ) { return 0; }
    mapped = xl6FutureThen(source, xl6_ws_future_map_bool, 0);
    if ( mapped != 0 && !xl6FutureLinkCancel(mapped, source) ) {
        xl6FutureDestroy(mapped);
        mapped = 0;
    }
    xl6FutureDestroy(source);
    return mapped;
}
) "\n";



/* WebSocket 配置对象只保存拥有型快照，连接创建后不再借用 xlang 临时对象。 */
