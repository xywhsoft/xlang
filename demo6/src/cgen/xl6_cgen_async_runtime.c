#include "xl6_cgen_async_runtime_await.c"
#include "xl6_cgen_async_runtime_prelude.c"
#include "xl6_cgen_async_runtime_task_spawn.c"
#include "xl6_cgen_async_runtime_typed_tasks.c"

static void xl6CgenEmitAsyncRuntime(
	XL6_Output* out,
	int hasTaskSpawn,
	int hasTypedContainers,
	int hasSpecialCollections,
	int hasWeakRuntime,
	int hasStringRuntime,
	int hasFunctionVarRuntime)
{
	xl6CgenEmitAsyncRuntimePrelude(out, hasStringRuntime);

    xl6OutputPuts("xvalue* xrtValueRetain(const xvalue* pValue);\n", out);
    xl6OutputPuts("void xrtValueRelease(xvalue* pVal);\n", out);
    xl6OutputPuts("xvalue* xrtValueArray(void);\n", out);
    xl6OutputPuts("static void xl6_async_free_xvalue_value(void* pValue)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue** value = (xvalue**)pValue;\n", out);
    xl6OutputPuts("    if ( value != 0 && *value != 0 ) {\n", out);
    xl6OutputPuts("        xrtValueRelease(*value);\n", out);
    xl6OutputPuts("        *value = 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xrtFree(pValue);\n", out);
    xl6OutputPuts("}\n", out);

    if ( hasWeakRuntime ) {
        xl6OutputPuts("static void xl6_async_free_weak_value(void* pValue)\n", out);
        xl6OutputPuts("{\n", out);
        xl6OutputPuts("    xl6_weak* value = (xl6_weak*)pValue;\n", out);
        xl6OutputPuts("    if ( value != 0 ) {\n", out);
        xl6OutputPuts("        xl6WeakUnit(value);\n", out);
        xl6OutputPuts("    }\n", out);
        xl6OutputPuts("    xrtFree(pValue);\n", out);
        xl6OutputPuts("}\n", out);
    }

    /* 只有函数值进入异步边界时，才生成 callable 转换与释放闭包。 */
    if ( hasFunctionVarRuntime ) {
        xl6OutputPuts("const xl6_type_desc* xl6TypeFunction(void);\n", out);
        if ( hasTypedContainers ) {
            xl6OutputPuts("bool xl6TypedArrayAppendValue(xtarray pArray, xvalue* pVal);\n", out);
        }
        xl6OutputPuts("bool xl6TypeUnboxValue(const xl6_type_desc* pType, xvalue* pVal, void* pOut);\n", out);
        xl6OutputPuts("void xl6TypeDropValue(const xl6_type_desc* pType, void* pObj);\n", out);
        xl6OutputPuts("static xvalue* xl6_function_to_xvalue(XL6_Function fn);\n", out);
        xl6OutputPuts("static void xl6_async_free_function_value(void* pValue)\n", out);
        xl6OutputPuts("{\n", out);
        xl6OutputPuts("    XL6_Function* fn = (XL6_Function*)pValue;\n", out);
        xl6OutputPuts("    if ( fn != 0 && fn->release != 0 && fn->env != 0 ) {\n", out);
        xl6OutputPuts("        fn->release(fn->env);\n", out);
        xl6OutputPuts("        fn->env = 0;\n", out);
        xl6OutputPuts("        fn->release = 0;\n", out);
        xl6OutputPuts("    }\n", out);
        xl6OutputPuts("    xrtFree(pValue);\n", out);
        xl6OutputPuts("}\n\n", out);
    }

    if ( hasTypedContainers ) {
        xl6OutputPuts("static void xl6_async_free_typed_array_value(void* pValue)\n", out);
        xl6OutputPuts("{\n", out);
        xl6OutputPuts("    xtarray* value = (xtarray*)pValue;\n", out);
        xl6OutputPuts("    if ( value != 0 && *value != 0 ) {\n", out);
        xl6OutputPuts("        xl6TypedArrayRelease(*value);\n", out);
        xl6OutputPuts("        *value = 0;\n", out);
        xl6OutputPuts("    }\n", out);
        xl6OutputPuts("    xrtFree(pValue);\n", out);
        xl6OutputPuts("}\n", out);
        xl6OutputPuts("static void xl6_async_free_typed_list_value(void* pValue)\n", out);
        xl6OutputPuts("{\n", out);
        xl6OutputPuts("    xtlist* value = (xtlist*)pValue;\n", out);
        xl6OutputPuts("    if ( value != 0 && *value != 0 ) {\n", out);
        xl6OutputPuts("        xl6TypedListRelease(*value);\n", out);
        xl6OutputPuts("        *value = 0;\n", out);
        xl6OutputPuts("    }\n", out);
        xl6OutputPuts("    xrtFree(pValue);\n", out);
        xl6OutputPuts("}\n", out);
        xl6OutputPuts("static void xl6_async_free_typed_set_value(void* pValue)\n", out);
        xl6OutputPuts("{\n", out);
        xl6OutputPuts("    xtset* value = (xtset*)pValue;\n", out);
        xl6OutputPuts("    if ( value != 0 && *value != 0 ) {\n", out);
        xl6OutputPuts("        xl6TypedSetRelease(*value);\n", out);
        xl6OutputPuts("        *value = 0;\n", out);
        xl6OutputPuts("    }\n", out);
        xl6OutputPuts("    xrtFree(pValue);\n", out);
        xl6OutputPuts("}\n", out);
        xl6OutputPuts("static void xl6_async_free_typed_dict_value(void* pValue)\n", out);
        xl6OutputPuts("{\n", out);
        xl6OutputPuts("    xtdict* value = (xtdict*)pValue;\n", out);
        xl6OutputPuts("    if ( value != 0 && *value != 0 ) {\n", out);
        xl6OutputPuts("        xl6TypedDictRelease(*value);\n", out);
        xl6OutputPuts("        *value = 0;\n", out);
        xl6OutputPuts("    }\n", out);
        xl6OutputPuts("    xrtFree(pValue);\n", out);
        xl6OutputPuts("}\n\n", out);
    }

    if ( hasSpecialCollections ) {
        /* 仅在程序实际使用专用结构时生成对应句柄释放函数。 */
        xl6OutputPuts("static void xl6_async_free_typed_stack_value(void* pValue)\n", out);
        xl6OutputPuts("{\n", out);
        xl6OutputPuts("    xtstack* value = (xtstack*)pValue;\n", out);
        xl6OutputPuts("    if ( value != 0 && *value != 0 ) { xl6SpecialStackRelease(*value); *value = 0; }\n", out);
        xl6OutputPuts("    xrtFree(pValue);\n", out);
        xl6OutputPuts("}\n", out);
        xl6OutputPuts("static void xl6_async_free_typed_avl_value(void* pValue)\n", out);
        xl6OutputPuts("{\n", out);
        xl6OutputPuts("    xtavltree* value = (xtavltree*)pValue;\n", out);
        xl6OutputPuts("    if ( value != 0 && *value != 0 ) { xl6SpecialTreeRelease(*value); *value = 0; }\n", out);
        xl6OutputPuts("    xrtFree(pValue);\n", out);
        xl6OutputPuts("}\n", out);
        xl6OutputPuts("static void xl6_async_free_typed_queue_value(void* pValue)\n", out);
        xl6OutputPuts("{\n", out);
        xl6OutputPuts("    xtqueue* value = (xtqueue*)pValue;\n", out);
        xl6OutputPuts("    if ( value != 0 && *value != 0 ) { xl6SpecialQueueRelease(*value); *value = 0; }\n", out);
        xl6OutputPuts("    xrtFree(pValue);\n", out);
        xl6OutputPuts("}\n\n", out);
    }

    xl6OutputPuts("static uint64 xl6_timeout_ms_to_us(long long timeoutMs)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( timeoutMs < 0 ) { return UINT64_MAX; }\n", out);
    xl6OutputPuts("    if ( (uint64)timeoutMs > (UINT64_MAX / 1000u) ) { return UINT64_MAX; }\n", out);
    xl6OutputPuts("    return ((uint64)timeoutMs * 1000u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_async_free_error_value(void* pValue)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Error** box = (XL6_Error**)pValue;\n", out);
    xl6OutputPuts("    if ( box != 0 ) { xl6_error_free(*box); }\n", out);
    xl6OutputPuts("    xrtFree(box);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_channel_require_handle(void* pHandle, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pHandle != 0 ) { return pHandle; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"ChannelError\", \"channel create failed\", file, line, column);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static xl6_channel_result xl6_channel_recv_select(xl6_channel pChannel, void** ppItem, int bWait, long long timeoutMs)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( timeoutMs >= 0 ) { return xl6ChannelRecvFor(pChannel, ppItem, timeoutMs); }\n", out);
    xl6OutputPuts("    return xl6ChannelRecv(pChannel, ppItem, bWait != 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_sync_require_handle(void* pHandle, const char* message, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pHandle != 0 ) { return pHandle; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"SyncError\", message != 0 ? message : \"sync handle create failed\", file, line, column);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_thread_require_handle(void* pThread, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pThread != 0 ) { return pThread; }\n", out);
    xl6OutputPuts("    if ( xl6_error_has() ) { return 0; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"ThreadError\", \"thread start failed\", file, line, column);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xfuture* xl6_async_require_future(xfuture* pFuture, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pFuture != 0 ) { return pFuture; }\n", out);
    xl6OutputPuts("    if ( xl6_error_has() ) { return 0; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"AsyncError\", \"async task start failed\", file, line, column);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xfuture* xl6_future_add_ref_if_needed(xfuture* pFuture)\n{\n    return pFuture != 0 ? xl6FutureRef(pFuture) : 0;\n}\n\n", out);
    xl6OutputPuts("static XL6_Error* xl6_future_error_object(xfuture* pFuture)\n{\n    const xerror* error = pFuture != 0 ? xl6FutureError(pFuture) : 0;\n    return error != 0 ? xl6_error_from_xrt(error, \"\", 0, 0) : 0;\n}\n\n", out);
    xl6OutputPuts("static const char* xl6_future_debug_name(xfuture* pFuture)\n{\n    const char* name = pFuture != 0 ? xl6FutureName(pFuture) : 0;\n    return name != 0 ? name : \"\";\n}\n\n", out);
    xl6OutputPuts("static xvalue* xl6_future_to_xvalue(xfuture* future, int releaseSource)\n{\n    xvalue* value = future != 0 ? xl6TypeBoxValue(xl6TypeFuture(), &future) : xrtValueNull();\n    if ( releaseSource && future != 0 ) { xl6FutureDestroy(future); }\n    return value;\n}\n", out);
    xl6OutputPuts("void* xl6_xrt_task_group_create(void)\n{\n    return xl6TaskGroupCreate();\n}\n", out);
    xl6OutputPuts("void* xl6_xrt_task_group_wrap(void* handle)\n{\n    return handle;\n}\n", out);
    xl6OutputPuts("long long xl6_xrt_task_group_destroy(void* value)\n{\n    xl6_task_group* group = (xl6_task_group*)value;\n    if ( group != 0 ) { xl6TaskGroupClose(group); xl6TaskGroupCancel(group); xl6TaskGroupDestroy(group); }\n    return 0;\n}\n", out);
    xl6OutputPuts("static bool xl6_xrt_task_group_unbox_future(xvalue* value, xfuture** outFuture)\n{\n    if ( outFuture == 0 ) { return false; }\n    *outFuture = 0;\n    return xl6TypeUnboxValue(xl6TypeFuture(), value, outFuture) && *outFuture != 0;\n}\n", out);
    xl6OutputPuts("int xl6_xrt_task_group_add(void* value, xvalue* task)\n{\n    xfuture* future = 0; bool ok;\n    if ( value == 0 || !xl6_xrt_task_group_unbox_future(task, &future) ) { return false; }\n    ok = xl6TaskGroupAdd((xl6_task_group*)value, future);\n    xl6FutureDestroy(future);\n    return ok;\n}\n", out);
    xl6OutputPuts("int xl6_xrt_task_group_bind_parent(void* value, xvalue* task)\n{\n    xfuture* future = 0; bool ok;\n    if ( value == 0 || !xl6_xrt_task_group_unbox_future(task, &future) ) { return false; }\n    ok = xl6TaskGroupBindParent((xl6_task_group*)value, future);\n    xl6FutureDestroy(future);\n    return ok;\n}\n", out);
    xl6OutputPuts("void* xl6_xrt_task_group_child(void* value)\n{\n    return value != 0 ? xl6TaskGroupChild((xl6_task_group*)value) : 0;\n}\n", out);
    xl6OutputPuts("void xl6_xrt_task_group_close(void* value)\n{\n    if ( value != 0 ) { xl6TaskGroupClose((xl6_task_group*)value); }\n}\n", out);
    xl6OutputPuts("void xl6_xrt_task_group_cancel(void* value)\n{\n    if ( value != 0 ) { xl6TaskGroupCancel((xl6_task_group*)value); }\n}\n", out);
    xl6OutputPuts("int xl6_xrt_task_group_count(void* value)\n{\n    return value != 0 ? xl6TaskGroupCount((xl6_task_group*)value) : 0;\n}\n", out);
    xl6OutputPuts("int xl6_xrt_task_group_reap(void* value)\n{\n    return value != 0 ? xl6TaskGroupReap((xl6_task_group*)value) : 0;\n}\n", out);
    xl6OutputPuts("int xl6_xrt_task_group_join(void* value)\n{\n    return value != 0 && xl6TaskGroupJoin((xl6_task_group*)value);\n}\n", out);
    xl6OutputPuts("int xl6_xrt_task_group_join_timeout(void* value, long long timeoutMs)\n{\n    return value != 0 && xl6TaskGroupJoinFor((xl6_task_group*)value, timeoutMs);\n}\n", out);
    xl6OutputPuts("int xl6_xrt_task_group_join_until(void* value, long long deadlineMs)\n{\n    return value != 0 && xl6TaskGroupJoinUntil((xl6_task_group*)value, deadlineMs);\n}\n", out);
    xl6OutputPuts("int xl6_xrt_task_group_wait(void* value)\n{\n    return value != 0 && xl6TaskGroupJoin((xl6_task_group*)value);\n}\n", out);
    xl6OutputPuts("int xl6_xrt_task_group_wait_timeout(void* value, long long timeoutMs)\n{\n    return value != 0 && xl6TaskGroupJoinFor((xl6_task_group*)value, timeoutMs);\n}\n", out);
    xl6OutputPuts("int xl6_xrt_task_group_wait_until(void* value, long long deadlineMs)\n{\n    return value != 0 && xl6TaskGroupJoinUntil((xl6_task_group*)value, deadlineMs);\n}\n", out);
    xl6OutputPuts("xfuture* xl6_xrt_task_group_join_async(void* value)\n{\n    xfuture* future;\n    if ( value == 0 ) { return 0; }\n    future = xl6TaskGroupFuture((xl6_task_group*)value);\n    if ( future != 0 ) { xl6TaskGroupClose((xl6_task_group*)value); }\n    return future;\n}\n\n", out);

    xl6OutputPuts("static void xl6_thread_release(xthread* pThread)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pThread == 0 ) { return; }\n", out);
    xl6OutputPuts("    xrtThreadWait(pThread);\n", out);
    xl6OutputPuts("    xrtThreadDestroy(pThread);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#ifdef XL6_FUNCTION_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct XL6_ThreadArg {\n", out);
    xl6OutputPuts("    XL6_Function fn;\n", out);
    xl6OutputPuts("} XL6_ThreadArg;\n\n", out);
    xl6OutputPuts("static int xl6_thread_entry(void* pArg)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_ThreadArg* arg = (XL6_ThreadArg*)pArg;\n", out);
    xl6OutputPuts("    XL6_Function fn;\n", out);
    xl6OutputPuts("    int code = 1;\n", out);
    xl6OutputPuts("    if ( arg == 0 ) { return 1; }\n", out);
    xl6OutputPuts("    fn = arg->fn;\n", out);
    xl6OutputPuts("    xrtFree(arg);\n", out);
    xl6OutputPuts("    if ( fn.invoke != 0 ) {\n", out);
    xl6OutputPuts("        code = ((int (*)(void*))fn.invoke)(fn.invoke_env);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( fn.release != 0 && fn.env != 0 ) {\n", out);
    xl6OutputPuts("        fn.release(fn.env);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return code;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void* xl6_thread_start(XL6_Function fn, int retainBorrow, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_ThreadArg* arg;\n", out);
    xl6OutputPuts("    xthread* pThread;\n", out);
    xl6OutputPuts("    if ( fn.invoke == 0 ) {\n", out);
    xl6OutputPuts("        if ( !retainBorrow && fn.release != 0 && fn.env != 0 ) { fn.release(fn.env); }\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ThreadError\", \"thread requires a callable func value\", file, line, column);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( retainBorrow && fn.retain != 0 && fn.env != 0 ) {\n", out);
    xl6OutputPuts("        fn.retain(fn.env);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    arg = (XL6_ThreadArg*)xrtMalloc(sizeof(XL6_ThreadArg));\n", out);
    xl6OutputPuts("    if ( arg == 0 ) {\n", out);
    xl6OutputPuts("        if ( fn.release != 0 && fn.env != 0 ) { fn.release(fn.env); }\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ThreadError\", \"out of memory while starting thread\", file, line, column);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    arg->fn = fn;\n", out);
    xl6OutputPuts("    (void)xl6_error_active_get();\n", out);
    xl6OutputPuts("    pThread = xrtThreadCreate(xl6_thread_entry, arg, 0);\n", out);
    xl6OutputPuts("    if ( pThread == 0 ) {\n", out);
    xl6OutputPuts("        if ( fn.release != 0 && fn.env != 0 ) { fn.release(fn.env); }\n", out);
    xl6OutputPuts("        xrtFree(arg);\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ThreadError\", \"thread start failed\", file, line, column);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return pThread;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("#endif\n\n", out);

    xl6OutputPuts("static void xl6_async_result_ok(xl6_future_result* pOut)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pOut == 0 ) { return; }\n", out);
    xl6OutputPuts("    pOut->iStatus = 0;\n", out);
    xl6OutputPuts("    pOut->pValue = 0;\n", out);
    xl6OutputPuts("    pOut->Error = 0;\n", out);
    xl6OutputPuts("    pOut->iFlags = 0;\n", out);
    xl6OutputPuts("    pOut->pfnFreeValue = 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_channel_send_value(xl6_channel pChannel, const void* pValue, size_t iSize, int bWait, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* pBox;\n", out);
    xl6OutputPuts("    xl6_channel_result iRet;\n", out);
    xl6OutputPuts("    if ( pChannel == 0 || pValue == 0 || iSize == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ChannelError\", \"invalid channel send\", file, line, column);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    pBox = xrtMalloc(iSize);\n", out);
    xl6OutputPuts("    if ( pBox == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ChannelError\", \"out of memory while sending channel value\", file, line, column);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    memcpy(pBox, pValue, iSize);\n", out);
    xl6OutputPuts("    iRet = xl6ChannelSend(pChannel, pBox, bWait != 0);\n", out);
    xl6OutputPuts("    if ( iRet == XL6_CHANNEL_OK ) { return true; }\n", out);
    xl6OutputPuts("    xrtFree(pBox);\n", out);
    xl6OutputPuts("    if ( iRet == XL6_CHANNEL_FULL || iRet == XL6_CHANNEL_CLOSED ) { return false; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"ChannelError\", \"channel send failed\", file, line, column);\n", out);
    xl6OutputPuts("    return false;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_channel_recv_into(xl6_channel pChannel, void* pOut, size_t iSize, int bWait, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* pBox = 0;\n", out);
    xl6OutputPuts("    xl6_channel_result iRet;\n", out);
    xl6OutputPuts("    if ( pChannel == 0 || pOut == 0 || iSize == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ChannelError\", \"invalid channel receive\", file, line, column);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    iRet = xl6ChannelRecv(pChannel, &pBox, bWait != 0);\n", out);
    xl6OutputPuts("    if ( iRet == XL6_CHANNEL_OK ) {\n", out);
    xl6OutputPuts("        if ( pBox != 0 ) { memcpy(pOut, pBox, iSize); xrtFree(pBox); }\n", out);
    xl6OutputPuts("        return true;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( iRet == XL6_CHANNEL_EMPTY || iRet == XL6_CHANNEL_CLOSED || iRet == XL6_CHANNEL_TIMEOUT ) { return false; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"ChannelError\", \"channel receive failed\", file, line, column);\n", out);
    xl6OutputPuts("    return false;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static bool xl6_channel_recv_into_timeout(xl6_channel pChannel, void* pOut, size_t iSize, long long timeoutMs, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* pBox = 0;\n", out);
    xl6OutputPuts("    xl6_channel_result iRet;\n", out);
    xl6OutputPuts("    if ( pChannel == 0 || pOut == 0 || iSize == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ChannelError\", \"invalid channel receive\", file, line, column);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    iRet = xl6_channel_recv_select(pChannel, &pBox, 1, timeoutMs);\n", out);
    xl6OutputPuts("    if ( iRet == XL6_CHANNEL_OK ) {\n", out);
    xl6OutputPuts("        if ( pBox != 0 ) { memcpy(pOut, pBox, iSize); xrtFree(pBox); }\n", out);
    xl6OutputPuts("        return true;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( iRet == XL6_CHANNEL_EMPTY || iRet == XL6_CHANNEL_CLOSED || iRet == XL6_CHANNEL_TIMEOUT ) { return false; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"ChannelError\", \"channel receive failed\", file, line, column);\n", out);
    xl6OutputPuts("    return false;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_channel_send_string(xl6_channel pChannel, const char* value, int bWait, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t size = value != 0 ? strlen(value) + 1u : 1u;\n", out);
    xl6OutputPuts("    char* copy = (char*)xrtMalloc(size);\n", out);
    xl6OutputPuts("    xl6_channel_result result;\n", out);
    xl6OutputPuts("    if ( pChannel == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ChannelError\", \"invalid channel send\", file, line, column);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( copy == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ChannelError\", \"out of memory while sending channel string\", file, line, column);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( value != 0 ) { memcpy(copy, value, size); } else { copy[0] = 0; }\n", out);
    xl6OutputPuts("    result = xl6ChannelSend(pChannel, copy, bWait != 0);\n", out);
    xl6OutputPuts("    if ( result == XL6_CHANNEL_OK ) { return true; }\n", out);
    xl6OutputPuts("    xrtFree(copy);\n", out);
    xl6OutputPuts("    if ( result == XL6_CHANNEL_FULL || result == XL6_CHANNEL_CLOSED ) { return false; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"ChannelError\", \"channel send failed\", file, line, column);\n", out);
    xl6OutputPuts("    return false;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_channel_send_xvalue(xl6_channel pChannel, xvalue* value, int bWait, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue** pBox;\n", out);
    xl6OutputPuts("    xl6_channel_result iRet;\n", out);
    xl6OutputPuts("    if ( pChannel == 0 || value == 0 ) {\n", out);
    xl6OutputPuts("        if ( value != 0 ) { xrtValueRelease(value); }\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ChannelError\", \"invalid channel send\", file, line, column);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( value == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ChannelError\", \"out of memory while sharing channel value\", file, line, column);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    pBox = (xvalue**)xrtMalloc(sizeof(xvalue*));\n", out);
    xl6OutputPuts("    if ( pBox == 0 ) {\n", out);
    xl6OutputPuts("        xrtValueRelease(value);\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ChannelError\", \"out of memory while sending channel value\", file, line, column);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    *pBox = value;\n", out);
    xl6OutputPuts("    iRet = xl6ChannelSend(pChannel, pBox, bWait != 0);\n", out);
    xl6OutputPuts("    if ( iRet == XL6_CHANNEL_OK ) { return true; }\n", out);
    xl6OutputPuts("    xrtValueRelease(value);\n", out);
    xl6OutputPuts("    xrtFree(pBox);\n", out);
    xl6OutputPuts("    if ( iRet == XL6_CHANNEL_FULL || iRet == XL6_CHANNEL_CLOSED ) { return false; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"ChannelError\", \"channel send failed\", file, line, column);\n", out);
    xl6OutputPuts("    return false;\n", out);
    xl6OutputPuts("}\n\n", out);

	if ( hasStringRuntime ) {
		xl6OutputPuts("static bool xl6_channel_recv_string(xl6_channel pChannel, const char** pOut, int bWait, const char* file, int line, int column)\n", out);
		xl6OutputPuts("{\n", out);
		xl6OutputPuts("    char* box = 0;\n", out);
		xl6OutputPuts("    char* copy;\n", out);
		xl6OutputPuts("    size_t size;\n", out);
		xl6OutputPuts("    xl6_channel_result result;\n", out);
		xl6OutputPuts("    if ( pChannel == 0 || pOut == 0 ) {\n", out);
		xl6OutputPuts("        xl6_error_raise(\"ChannelError\", \"invalid channel receive\", file, line, column);\n", out);
		xl6OutputPuts("        return false;\n", out);
		xl6OutputPuts("    }\n", out);
		xl6OutputPuts("    result = xl6ChannelRecv(pChannel, (void**)&box, bWait != 0);\n", out);
		xl6OutputPuts("    if ( result == XL6_CHANNEL_OK ) {\n", out);
		xl6OutputPuts("        size = box != 0 ? strlen(box) : 0u;\n", out);
		xl6OutputPuts("        copy = xl6_xrt_string_dup(box != 0 ? box : \"\", size);\n", out);
		xl6OutputPuts("        if ( copy == 0 ) { xrtFree(box); xl6_error_raise(\"ChannelError\", \"out of memory while receiving channel string\", file, line, column); return false; }\n", out);
		xl6OutputPuts("        xrtFree(box);\n", out);
		xl6OutputPuts("        if ( *pOut != 0 ) { xrtFree((void*)*pOut); }\n", out);
		xl6OutputPuts("        *pOut = copy;\n", out);
		xl6OutputPuts("        return true;\n", out);
		xl6OutputPuts("    }\n", out);
		xl6OutputPuts("    if ( result == XL6_CHANNEL_EMPTY || result == XL6_CHANNEL_CLOSED || result == XL6_CHANNEL_TIMEOUT ) { return false; }\n", out);
		xl6OutputPuts("    xl6_error_raise(\"ChannelError\", \"channel receive failed\", file, line, column);\n", out);
		xl6OutputPuts("    return false;\n", out);
		xl6OutputPuts("}\n\n", out);
		xl6OutputPuts("static bool xl6_channel_recv_string_timeout(xl6_channel pChannel, const char** pOut, long long timeoutMs, const char* file, int line, int column)\n", out);
		xl6OutputPuts("{\n", out);
		xl6OutputPuts("    char* box = 0;\n", out);
		xl6OutputPuts("    char* copy;\n", out);
		xl6OutputPuts("    size_t size;\n", out);
		xl6OutputPuts("    xl6_channel_result result;\n", out);
		xl6OutputPuts("    if ( pChannel == 0 || pOut == 0 ) {\n", out);
		xl6OutputPuts("        xl6_error_raise(\"ChannelError\", \"invalid channel receive\", file, line, column);\n", out);
		xl6OutputPuts("        return false;\n", out);
		xl6OutputPuts("    }\n", out);
		xl6OutputPuts("    result = xl6_channel_recv_select(pChannel, (void**)&box, 1, timeoutMs);\n", out);
		xl6OutputPuts("    if ( result == XL6_CHANNEL_OK ) {\n", out);
		xl6OutputPuts("        size = box != 0 ? strlen(box) : 0u;\n", out);
		xl6OutputPuts("        copy = xl6_xrt_string_dup(box != 0 ? box : \"\", size);\n", out);
		xl6OutputPuts("        if ( copy == 0 ) { xrtFree(box); xl6_error_raise(\"ChannelError\", \"out of memory while receiving channel string\", file, line, column); return false; }\n", out);
		xl6OutputPuts("        xrtFree(box);\n", out);
		xl6OutputPuts("        if ( *pOut != 0 ) { xrtFree((void*)*pOut); }\n", out);
		xl6OutputPuts("        *pOut = copy;\n", out);
		xl6OutputPuts("        return true;\n", out);
		xl6OutputPuts("    }\n", out);
		xl6OutputPuts("    if ( result == XL6_CHANNEL_EMPTY || result == XL6_CHANNEL_CLOSED || result == XL6_CHANNEL_TIMEOUT ) { return false; }\n", out);
		xl6OutputPuts("    xl6_error_raise(\"ChannelError\", \"channel receive failed\", file, line, column);\n", out);
		xl6OutputPuts("    return false;\n", out);
		xl6OutputPuts("}\n\n", out);
	}

    xl6OutputPuts("static bool xl6_channel_recv_xvalue(xl6_channel pChannel, xvalue** pOut, int bWait, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue** pBox = 0;\n", out);
    xl6OutputPuts("    xl6_channel_result iRet;\n", out);
    xl6OutputPuts("    if ( pChannel == 0 || pOut == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ChannelError\", \"invalid channel receive\", file, line, column);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    iRet = xl6ChannelRecv(pChannel, (void**)&pBox, bWait != 0);\n", out);
    xl6OutputPuts("    if ( iRet == XL6_CHANNEL_OK ) {\n", out);
    xl6OutputPuts("        if ( *pOut != 0 ) { xrtValueRelease(*pOut); }\n", out);
    xl6OutputPuts("        *pOut = pBox != 0 ? *pBox : 0;\n", out);
    xl6OutputPuts("        xrtFree(pBox);\n", out);
    xl6OutputPuts("        return true;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( iRet == XL6_CHANNEL_EMPTY || iRet == XL6_CHANNEL_CLOSED || iRet == XL6_CHANNEL_TIMEOUT ) { return false; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"ChannelError\", \"channel receive failed\", file, line, column);\n", out);
    xl6OutputPuts("    return false;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static bool xl6_channel_recv_xvalue_timeout(xl6_channel pChannel, xvalue** pOut, long long timeoutMs, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue** pBox = 0;\n", out);
    xl6OutputPuts("    xl6_channel_result iRet;\n", out);
    xl6OutputPuts("    if ( pChannel == 0 || pOut == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ChannelError\", \"invalid channel receive\", file, line, column);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    iRet = xl6_channel_recv_select(pChannel, (void**)&pBox, 1, timeoutMs);\n", out);
    xl6OutputPuts("    if ( iRet == XL6_CHANNEL_OK ) {\n", out);
    xl6OutputPuts("        if ( *pOut != 0 ) { xrtValueRelease(*pOut); }\n", out);
    xl6OutputPuts("        *pOut = pBox != 0 ? *pBox : 0;\n", out);
    xl6OutputPuts("        xrtFree(pBox);\n", out);
    xl6OutputPuts("        return true;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( iRet == XL6_CHANNEL_EMPTY || iRet == XL6_CHANNEL_CLOSED || iRet == XL6_CHANNEL_TIMEOUT ) { return false; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"ChannelError\", \"channel receive failed\", file, line, column);\n", out);
    xl6OutputPuts("    return false;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_channel_destroy(xl6_channel pChannel)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* box = 0;\n", out);
    xl6OutputPuts("    if ( pChannel == 0 ) { return; }\n", out);
    xl6OutputPuts("    xl6ChannelClose(pChannel);\n", out);
    xl6OutputPuts("    while ( xl6ChannelRecv(pChannel, &box, false) == XL6_CHANNEL_OK ) { xrtFree(box); box = 0; }\n", out);
    xl6OutputPuts("    xl6ChannelDestroy(pChannel);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void xl6_channel_destroy_xvalue(xl6_channel pChannel)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue** box = 0;\n", out);
    xl6OutputPuts("    if ( pChannel == 0 ) { return; }\n", out);
    xl6OutputPuts("    xl6ChannelClose(pChannel);\n", out);
    xl6OutputPuts("    while ( xl6ChannelRecv(pChannel, (void**)&box, false) == XL6_CHANNEL_OK ) { if ( box != 0 && *box != 0 ) { xrtValueRelease(*box); } xrtFree(box); box = 0; }\n", out);
    xl6OutputPuts("    xl6ChannelDestroy(pChannel);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static int xl6_async_result_native_error(xl6_future_result* pOut, const xerror* error)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pOut == 0 ) { return -1; }\n", out);
    xl6OutputPuts("    memset(pOut, 0, sizeof(*pOut));\n", out);
    xl6OutputPuts("    pOut->iStatus = -1;\n", out);
    xl6OutputPuts("    pOut->Error = xrtErrorRef(error);\n", out);
    xl6OutputPuts("    return -1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_async_result_error(xl6_future_result* pOut, const char* message)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xerrordesc desc;\n", out);
    xl6OutputPuts("    xerror* error;\n", out);
    xl6OutputPuts("    memset(&desc, 0, sizeof(desc));\n", out);
    xl6OutputPuts("    desc.Kind = XERR_STATE;\n", out);
    xl6OutputPuts("    desc.Domain = \"xlang\";\n", out);
    xl6OutputPuts("    desc.Operation = \"async\";\n", out);
    xl6OutputPuts("    desc.Code = 1;\n", out);
    xl6OutputPuts("    desc.Message = message != 0 ? message : \"async task failed\";\n", out);
    xl6OutputPuts("    desc.Data = \"AsyncError\";\n", out);
    xl6OutputPuts("    error = xrtErrorBuild(&desc);\n", out);
    xl6OutputPuts("    xl6_async_result_native_error(pOut, error);\n", out);
    xl6OutputPuts("    xrtErrorFree(error);\n", out);
    xl6OutputPuts("    return -1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_async_result_language_error(xl6_future_result* pOut, const XL6_Error* languageError)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xerror* cause = xl6_error_to_xrt(languageError, \"async\");\n", out);
    xl6OutputPuts("    xerrordesc desc;\n", out);
    xl6OutputPuts("    xerror* error;\n", out);
    xl6OutputPuts("    if ( cause == 0 ) { return xl6_async_result_error(pOut, \"failed to preserve asynchronous error\"); }\n", out);
    xl6OutputPuts("    memset(&desc, 0, sizeof(desc));\n", out);
    xl6OutputPuts("    desc.Kind = XERR_STATE;\n", out);
    xl6OutputPuts("    desc.Domain = \"xlang\";\n", out);
    xl6OutputPuts("    desc.Operation = \"async\";\n", out);
    xl6OutputPuts("    desc.Code = 1;\n", out);
    xl6OutputPuts("    desc.Message = xrtErrorMessage(cause);\n", out);
    xl6OutputPuts("    desc.Data = \"AsyncError\";\n", out);
    xl6OutputPuts("    desc.Cause = cause;\n", out);
    xl6OutputPuts("    error = xrtErrorBuild(&desc);\n", out);
    xl6OutputPuts("    xrtErrorFree(cause);\n", out);
    xl6OutputPuts("    if ( error == 0 ) { return xl6_async_result_error(pOut, \"failed to wrap asynchronous error\"); }\n", out);
    xl6OutputPuts("    xl6_async_result_native_error(pOut, error);\n", out);
    xl6OutputPuts("    xrtErrorFree(error);\n", out);
    xl6OutputPuts("    return -1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6CgenEmitAsyncTaskSpawnRuntime(out, hasTaskSpawn);

    xl6CgenEmitAsyncTypedTaskRuntime(
        out,
        hasTypedContainers,
        hasFunctionVarRuntime
    );

	xl6CgenEmitAsyncAwaitRuntime(
		out,
		hasWeakRuntime,
		hasTypedContainers,
		hasStringRuntime);
}

