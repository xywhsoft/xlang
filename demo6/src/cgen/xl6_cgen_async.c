static int xl6CgenIsAsyncFunc(const XL6_AstNode* node)
{
    return node != NULL && node->kind == XL6_AST_ASYNC_FUNC;
}

static const XL6_AstNode* xl6CgenAsyncReturns(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 2 ? node->children[2] : NULL;
}

static const XL6_AstNode* xl6CgenAsyncParams(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 1 ? node->children[1] : NULL;
}

static const XL6_AstNode* xl6CgenAsyncBody(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 3 ? node->children[3] : NULL;
}

static const XL6_AstNode* xl6CgenAsyncResultType(const XL6_AstNode* node)
{
    const XL6_AstNode* returns = xl6CgenAsyncReturns(node);
    return returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
}

static void xl6CgenAsyncWorkerSymbol(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    xl6OutputPuts("xl_async_worker_", ctx->out);
    xl6CgenFuncSymbol(ctx, node);
}

static void xl6CgenAsyncArgsStructName(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    xl6OutputPuts("xl_async_args_", ctx->out);
    xl6CgenFuncSymbol(ctx, node);
}

static void xl6CgenAsyncResultFreeSymbol(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    xl6OutputPuts("xl_async_result_free_", ctx->out);
    xl6CgenFuncSymbol(ctx, node);
}

static void xl6CgenAsyncEmitParamList(XL6_CgenContext* ctx, const XL6_AstNode* params)
{
    size_t i;

    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("void", ctx->out);
        return;
    }

    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];

        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        if ( param == NULL || param->childCount == 0 ) {
            xl6CgenError(ctx, param, "async 函数参数缺少类型");
            xl6OutputPuts("int ", ctx->out);
        } else {
            xl6OutputPuts(xl6CgenTypeName(ctx, param->children[0]), ctx->out);
            xl6OutputPutc(' ', ctx->out);
        }
        xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : NULL);
    }
}

static int xl6CgenNodeNeedsAsyncRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_ASYNC_FUNC ||
         node->kind == XL6_AST_AWAIT ||
         node->kind == XL6_AST_YIELD ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CgenIsFutureLikeType(node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CgenIsChannelType(node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CgenIsThreadType(node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CgenIsGeneratorType(node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CgenIsSyncHandleType(node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 && node->children[0] != NULL ) {
        const XL6_AstNode* callee = node->children[0];
        if ( callee->kind == XL6_AST_NAME && xl6CTextEquals(callee->text, "thread") ) {
            return 1;
        }
        if ( callee->kind == XL6_AST_NAME && xl6CgenIsSyncHandleTypeName(callee->text) ) {
            return 1;
        }
        if ( callee->kind == XL6_AST_MEMBER &&
             (xl6CTextEquals(callee->text, "lock") ||
              xl6CTextEquals(callee->text, "tryLock") ||
              xl6CTextEquals(callee->text, "unlock") ||
              xl6CTextEquals(callee->text, "wait") ||
              xl6CTextEquals(callee->text, "tryWait") ||
              xl6CTextEquals(callee->text, "waitTimeout") ||
              xl6CTextEquals(callee->text, "post") ||
              xl6CTextEquals(callee->text, "postMultiple") ||
              xl6CTextEquals(callee->text, "signal") ||
              xl6CTextEquals(callee->text, "broadcast") ||
              xl6CTextEquals(callee->text, "readLock") ||
              xl6CTextEquals(callee->text, "tryReadLock") ||
              xl6CTextEquals(callee->text, "readUnlock") ||
              xl6CTextEquals(callee->text, "writeLock") ||
              xl6CTextEquals(callee->text, "tryWriteLock") ||
              xl6CTextEquals(callee->text, "writeUnlock") ||
              xl6CTextEquals(callee->text, "downgrade") ||
              xl6CTextEquals(callee->text, "upgrade") ||
              xl6CTextEquals(callee->text, "stop") ||
              xl6CTextEquals(callee->text, "state") ||
              xl6CTextEquals(callee->text, "exitCode")) ) {
            return 1;
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsAsyncRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenEmitAsyncRuntime(XL6_Output* out)
{
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("#ifndef XL6_XRT_ASYNC_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XRT_ASYNC_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xrt_net_future xfuture;\n", out);
    xl6OutputPuts("typedef struct xchannel_struct* xchannel;\n", out);
    xl6OutputPuts("typedef enum xqueue_result { XQUEUE_OK = 0, XQUEUE_EMPTY = 1, XQUEUE_FULL = 2, XQUEUE_CLOSED = 3, XQUEUE_TIMEOUT = 4, XQUEUE_ERROR = -1 } xqueue_result;\n", out);
    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    int iStatus;\n", out);
    xl6OutputPuts("    void* pValue;\n", out);
    xl6OutputPuts("    char* sError;\n", out);
    xl6OutputPuts("    unsigned int iFlags;\n", out);
    xl6OutputPuts("    void (*pfnFreeValue)(void* pValue);\n", out);
    xl6OutputPuts("    void (*pfnFreeError)(void* pError);\n", out);
    xl6OutputPuts("} xfuture_result;\n", out);
    xl6OutputPuts("typedef int (*xtask_thread_fn)(void* pArg, xfuture_result* pOut);\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("#ifndef XFUTURE_RESULT_F_OWN_VALUE\n", out);
    xl6OutputPuts("#define XFUTURE_RESULT_F_OWN_VALUE 0x00000001u\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("#ifndef XFUTURE_RESULT_F_OWN_ERROR\n", out);
    xl6OutputPuts("#define XFUTURE_RESULT_F_OWN_ERROR 0x00000002u\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* pMem);\n", out);
    xl6OutputPuts("xfuture* xTaskRunThread(xtask_thread_fn pfnTask, void* pArg, size_t iStackSize);\n", out);
    xl6OutputPuts("bool xFutureWait(xfuture* pFuture);\n", out);
    xl6OutputPuts("void* xFutureValue(xfuture* pFuture);\n", out);
    xl6OutputPuts("char* xFutureError(xfuture* pFuture);\n", out);
    xl6OutputPuts("void xFutureRelease(xfuture* pFuture);\n", out);
    xl6OutputPuts("xchannel xrtChannelCreate(unsigned int iCapacity);\n", out);
    xl6OutputPuts("void xrtChannelDestroy(xchannel pChannel);\n", out);
    xl6OutputPuts("xqueue_result xrtChannelSend(xchannel pChannel, void* pItem);\n", out);
    xl6OutputPuts("xqueue_result xrtChannelTryRecv(xchannel pChannel, void** ppItem);\n", out);
    xl6OutputPuts("xqueue_result xrtChannelRecv(xchannel pChannel, void** ppItem);\n", out);
    xl6OutputPuts("void xrtChannelClose(xchannel pChannel);\n", out);
    xl6OutputPuts("unsigned int xrtChannelCount(xchannel pChannel);\n", out);
    xl6OutputPuts("bool xrtChannelIsClosed(xchannel pChannel);\n\n", out);

    xl6OutputPuts("#ifdef XL6_FUNCTION_TYPEDEF\n", out);
    xl6OutputPuts("static void xl6_async_free_function_value(void* pValue)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Function* fn = (XL6_Function*)pValue;\n", out);
    xl6OutputPuts("    if ( fn != 0 && fn->release != 0 && fn->env != 0 ) {\n", out);
    xl6OutputPuts("        fn->release(fn->env);\n", out);
    xl6OutputPuts("        fn->env = 0;\n", out);
    xl6OutputPuts("        fn->release = 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xrtFree(pValue);\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("#endif\n\n", out);

    xl6OutputPuts("void* xrtMutexCreate(void);\n", out);
    xl6OutputPuts("void xrtMutexDestroy(void* pMutex);\n", out);
    xl6OutputPuts("void xrtMutexLock(void* pMutex);\n", out);
    xl6OutputPuts("bool xrtMutexTryLock(void* pMutex);\n", out);
    xl6OutputPuts("void xrtMutexUnlock(void* pMutex);\n", out);
    xl6OutputPuts("void* xrtSemCreate(unsigned int iInitValue, unsigned int iMaxValue);\n", out);
    xl6OutputPuts("void xrtSemDestroy(void* pSem);\n", out);
    xl6OutputPuts("void xrtSemWait(void* pSem);\n", out);
    xl6OutputPuts("bool xrtSemTryWait(void* pSem);\n", out);
    xl6OutputPuts("int xrtSemWaitTimeout(void* pSem, unsigned int iTimeout);\n", out);
    xl6OutputPuts("bool xrtSemPost(void* pSem);\n", out);
    xl6OutputPuts("bool xrtSemPostMultiple(void* pSem, unsigned int iCount);\n", out);
    xl6OutputPuts("void* xrtCondCreate(void);\n", out);
    xl6OutputPuts("void xrtCondDestroy(void* pCond);\n", out);
    xl6OutputPuts("void xrtCondWait(void* pCond, void* pMutex);\n", out);
    xl6OutputPuts("int xrtCondWaitTimeout(void* pCond, void* pMutex, unsigned int iTimeout);\n", out);
    xl6OutputPuts("void xrtCondSignal(void* pCond);\n", out);
    xl6OutputPuts("void xrtCondBroadcast(void* pCond);\n", out);
    xl6OutputPuts("void* xrtRWLockCreate(void);\n", out);
    xl6OutputPuts("void xrtRWLockDestroy(void* pRWLock);\n", out);
    xl6OutputPuts("void xrtRWLockReadLock(void* pRWLock);\n", out);
    xl6OutputPuts("bool xrtRWLockTryReadLock(void* pRWLock);\n", out);
    xl6OutputPuts("void xrtRWLockReadUnlock(void* pRWLock);\n", out);
    xl6OutputPuts("void xrtRWLockWriteLock(void* pRWLock);\n", out);
    xl6OutputPuts("bool xrtRWLockTryWriteLock(void* pRWLock);\n", out);
    xl6OutputPuts("void xrtRWLockWriteUnlock(void* pRWLock);\n", out);
    xl6OutputPuts("void xrtRWLockDowngrade(void* pRWLock);\n", out);
    xl6OutputPuts("bool xrtRWLockUpgrade(void* pRWLock);\n", out);
    xl6OutputPuts("typedef unsigned int (*xl6_xrt_thread_fn)(void* pArg);\n", out);
    xl6OutputPuts("void* xrtThreadCreate(xl6_xrt_thread_fn pProc, void* pParam, size_t iStackSize);\n", out);
    xl6OutputPuts("void xrtThreadDestroy(void* pThread);\n", out);
    xl6OutputPuts("void xrtThreadWait(void* pThread);\n", out);
    xl6OutputPuts("int xrtThreadWaitTimeout(void* pThread, unsigned int iTimeout);\n", out);
    xl6OutputPuts("void xrtThreadStop(void* pThread);\n", out);
    xl6OutputPuts("int xrtThreadGetState(void* pThread);\n", out);
    xl6OutputPuts("unsigned int xrtThreadGetExitCode(void* pThread);\n\n", out);

    xl6OutputPuts("static void* xl6_channel_require_handle(void* pHandle, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pHandle != 0 ) { return pHandle; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"ChannelError\", \"channel create failed\", file, line, column);\n", out);
    xl6OutputPuts("    return 0;\n", out);
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
    xl6OutputPuts("    xl6_error_raise(\"ThreadError\", \"thread start failed\", file, line, column);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xfuture* xl6_async_require_future(xfuture* pFuture, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pFuture != 0 ) { return pFuture; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"AsyncError\", \"async task start failed\", file, line, column);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_thread_release(void* pThread)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pThread == 0 ) { return; }\n", out);
    xl6OutputPuts("    xrtThreadWait(pThread);\n", out);
    xl6OutputPuts("    xrtThreadDestroy(pThread);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#ifdef XL6_FUNCTION_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct XL6_ThreadArg {\n", out);
    xl6OutputPuts("    XL6_Function fn;\n", out);
    xl6OutputPuts("} XL6_ThreadArg;\n\n", out);
    xl6OutputPuts("static unsigned int xl6_thread_entry(void* pArg)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_ThreadArg* arg = (XL6_ThreadArg*)pArg;\n", out);
    xl6OutputPuts("    XL6_Function fn;\n", out);
    xl6OutputPuts("    int code = 1;\n", out);
    xl6OutputPuts("    if ( arg == 0 ) { return 1u; }\n", out);
    xl6OutputPuts("    fn = arg->fn;\n", out);
    xl6OutputPuts("    xrtFree(arg);\n", out);
    xl6OutputPuts("    if ( fn.invoke != 0 ) {\n", out);
    xl6OutputPuts("        code = ((int (*)(void*))fn.invoke)(fn.env);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( fn.release != 0 && fn.env != 0 ) {\n", out);
    xl6OutputPuts("        fn.release(fn.env);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return (unsigned int)code;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void* xl6_thread_start(XL6_Function fn, int retainBorrow)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_ThreadArg* arg;\n", out);
    xl6OutputPuts("    void* pThread;\n", out);
    xl6OutputPuts("    if ( fn.invoke == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( retainBorrow && fn.retain != 0 && fn.env != 0 ) {\n", out);
    xl6OutputPuts("        fn.retain(fn.env);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    arg = (XL6_ThreadArg*)xrtMalloc(sizeof(XL6_ThreadArg));\n", out);
    xl6OutputPuts("    if ( arg == 0 ) {\n", out);
    xl6OutputPuts("        if ( fn.release != 0 && fn.env != 0 ) { fn.release(fn.env); }\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    arg->fn = fn;\n", out);
    xl6OutputPuts("    pThread = xrtThreadCreate(xl6_thread_entry, arg, 0);\n", out);
    xl6OutputPuts("    if ( pThread == 0 ) {\n", out);
    xl6OutputPuts("        if ( fn.release != 0 && fn.env != 0 ) { fn.release(fn.env); }\n", out);
    xl6OutputPuts("        xrtFree(arg);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return pThread;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("#endif\n\n", out);

    xl6OutputPuts("static void xl6_async_result_ok(xfuture_result* pOut)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pOut == 0 ) { return; }\n", out);
    xl6OutputPuts("    pOut->iStatus = 0;\n", out);
    xl6OutputPuts("    pOut->pValue = 0;\n", out);
    xl6OutputPuts("    pOut->sError = 0;\n", out);
    xl6OutputPuts("    pOut->iFlags = 0;\n", out);
    xl6OutputPuts("    pOut->pfnFreeValue = 0;\n", out);
    xl6OutputPuts("    pOut->pfnFreeError = 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_channel_send_value(xchannel pChannel, const void* pValue, size_t iSize, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* pBox;\n", out);
    xl6OutputPuts("    xqueue_result iRet;\n", out);
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
    xl6OutputPuts("    iRet = xrtChannelSend(pChannel, pBox);\n", out);
    xl6OutputPuts("    if ( iRet == XQUEUE_OK ) { return true; }\n", out);
    xl6OutputPuts("    xrtFree(pBox);\n", out);
    xl6OutputPuts("    if ( iRet == XQUEUE_FULL || iRet == XQUEUE_CLOSED ) { return false; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"ChannelError\", \"channel send failed\", file, line, column);\n", out);
    xl6OutputPuts("    return false;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_channel_recv_into(xchannel pChannel, void* pOut, size_t iSize, int bWait, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* pBox = 0;\n", out);
    xl6OutputPuts("    xqueue_result iRet;\n", out);
    xl6OutputPuts("    if ( pChannel == 0 || pOut == 0 || iSize == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ChannelError\", \"invalid channel receive\", file, line, column);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    iRet = bWait ? xrtChannelRecv(pChannel, &pBox) : xrtChannelTryRecv(pChannel, &pBox);\n", out);
    xl6OutputPuts("    if ( iRet == XQUEUE_OK ) {\n", out);
    xl6OutputPuts("        if ( pBox != 0 ) { memcpy(pOut, pBox, iSize); xrtFree(pBox); }\n", out);
    xl6OutputPuts("        return true;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( iRet == XQUEUE_EMPTY || iRet == XQUEUE_CLOSED || iRet == XQUEUE_TIMEOUT ) { return false; }\n", out);
    xl6OutputPuts("    xl6_error_raise(\"ChannelError\", \"channel receive failed\", file, line, column);\n", out);
    xl6OutputPuts("    return false;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_async_result_error(xfuture_result* pOut, const char* message)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pOut == 0 ) { return -1; }\n", out);
    xl6OutputPuts("    pOut->iStatus = -1;\n", out);
    xl6OutputPuts("    pOut->pValue = 0;\n", out);
    xl6OutputPuts("    pOut->sError = xl6_error_dup(message != 0 ? message : \"async task failed\");\n", out);
    xl6OutputPuts("    pOut->iFlags = pOut->sError != 0 ? XFUTURE_RESULT_F_OWN_ERROR : 0;\n", out);
    xl6OutputPuts("    pOut->pfnFreeValue = 0;\n", out);
    xl6OutputPuts("    pOut->pfnFreeError = pOut->sError != 0 ? xrtFree : 0;\n", out);
    xl6OutputPuts("    return -1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_async_zero_value(size_t valueSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* value;\n", out);
    xl6OutputPuts("    if ( valueSize == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    value = (unsigned char*)xrtMalloc(valueSize);\n", out);
    xl6OutputPuts("    if ( value == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    memset(value, 0, valueSize);\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void* xl6_future_await_value(xfuture* pFuture, const char* file, int line, int column, int requireValue, size_t valueSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* value;\n", out);
    xl6OutputPuts("    char* error;\n", out);
    xl6OutputPuts("    if ( pFuture == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"AsyncError\", \"await on null future\", file, line, column);\n", out);
    xl6OutputPuts("        return xl6_async_zero_value(valueSize);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( !xFutureWait(pFuture) ) {\n", out);
    xl6OutputPuts("        error = xFutureError(pFuture);\n", out);
    xl6OutputPuts("        xl6_error_raise(\"AsyncError\", error != 0 ? error : \"async task failed\", file, line, column);\n", out);
    xl6OutputPuts("        return xl6_async_zero_value(valueSize);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    error = xFutureError(pFuture);\n", out);
    xl6OutputPuts("    if ( error != 0 && error[0] != 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"AsyncError\", error, file, line, column);\n", out);
    xl6OutputPuts("        return xl6_async_zero_value(valueSize);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    value = xFutureValue(pFuture);\n", out);
    xl6OutputPuts("    if ( requireValue && value == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"AsyncError\", \"async task returned empty value\", file, line, column);\n", out);
    xl6OutputPuts("        return xl6_async_zero_value(valueSize);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_future_release_if_needed(xfuture* pFuture)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( pFuture != 0 ) {\n", out);
    xl6OutputPuts("        xFutureRelease(pFuture);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_future_await_copy_release(xfuture* pFuture, void* pOut, size_t valueSize, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* value;\n", out);
    xl6OutputPuts("    value = xl6_future_await_value(pFuture, file, line, column, 1, valueSize);\n", out);
    xl6OutputPuts("    if ( value != 0 && pOut != 0 && valueSize > 0 ) {\n", out);
    xl6OutputPuts("        memcpy(pOut, value, valueSize);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( xl6_error_has() && value != 0 ) {\n", out);
    xl6OutputPuts("        xrtFree(value);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xl6_future_release_if_needed(pFuture);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_future_await_void_release(xfuture* pFuture, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xl6_future_await_value(pFuture, file, line, column, 0, 0);\n", out);
    xl6OutputPuts("    xl6_future_release_if_needed(pFuture);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_future_await_i32_release(xfuture* pFuture, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int result = 0;\n", out);
    xl6OutputPuts("    xl6_future_await_copy_release(pFuture, &result, sizeof(result), file, line, column);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static long long xl6_future_await_i64_release(xfuture* pFuture, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    long long result = 0;\n", out);
    xl6OutputPuts("    xl6_future_await_copy_release(pFuture, &result, sizeof(result), file, line, column);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static unsigned long long xl6_future_await_u64_release(xfuture* pFuture, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned long long result = 0;\n", out);
    xl6OutputPuts("    xl6_future_await_copy_release(pFuture, &result, sizeof(result), file, line, column);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static float xl6_future_await_f32_release(xfuture* pFuture, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    float result = 0.0f;\n", out);
    xl6OutputPuts("    xl6_future_await_copy_release(pFuture, &result, sizeof(result), file, line, column);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static double xl6_future_await_f64_release(xfuture* pFuture, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    double result = 0.0;\n", out);
    xl6OutputPuts("    xl6_future_await_copy_release(pFuture, &result, sizeof(result), file, line, column);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const char* xl6_future_await_cstr_release(xfuture* pFuture, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* result = 0;\n", out);
    xl6OutputPuts("    xl6_future_await_copy_release(pFuture, &result, sizeof(result), file, line, column);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_future_await_ptr_release(xfuture* pFuture, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* result = 0;\n", out);
    xl6OutputPuts("    xl6_future_await_copy_release(pFuture, &result, sizeof(result), file, line, column);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#ifdef XL6_FUNCTION_TYPEDEF\n", out);
    xl6OutputPuts("static XL6_Function xl6_future_await_function_release(xfuture* pFuture, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Function result;\n", out);
    xl6OutputPuts("    void* value;\n", out);
    xl6OutputPuts("    memset(&result, 0, sizeof(result));\n", out);
    xl6OutputPuts("    value = xl6_future_await_value(pFuture, file, line, column, 1, sizeof(XL6_Function));\n", out);
    xl6OutputPuts("    if ( value != 0 ) {\n", out);
    xl6OutputPuts("        result = *(XL6_Function*)value;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( result.retain != 0 && result.env != 0 ) {\n", out);
    xl6OutputPuts("        result.retain(result.env);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( xl6_error_has() && value != 0 ) {\n", out);
    xl6OutputPuts("        xrtFree(value);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xl6_future_release_if_needed(pFuture);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("#endif\n\n", out);
    xl6CgenEmitGeneratorRuntime(out);
}

static void xl6CgenAsyncFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    xl6OutputPuts("xfuture* ", ctx->out);
    xl6CgenFuncSymbol(ctx, node);
    xl6OutputPutc('(', ctx->out);
    xl6CgenAsyncEmitParamList(ctx, xl6CgenAsyncParams(node));
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenIsChannelGenericInstant(const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArgs;

    if ( node == NULL || node->kind != XL6_AST_GENERIC_INSTANT || node->childCount < 2 ) {
        return 0;
    }
    nameNode = node->children[0];
    typeArgs = node->children[1];
    return nameNode != NULL &&
           nameNode->kind == XL6_AST_NAME &&
           xl6CTextEquals(nameNode->text, "channel") &&
           typeArgs != NULL &&
           typeArgs->childCount == 1;
}

static const XL6_AstNode* xl6CgenChannelElementType(const XL6_AstNode* channelType)
{
    const XL6_AstNode* args;

    if ( !xl6CgenIsChannelType(channelType) ||
         channelType->childCount == 0 ||
         channelType->children[0] == NULL ||
         channelType->children[0]->kind != XL6_AST_LIST ) {
        return NULL;
    }
    args = channelType->children[0];
    return args->childCount > 0 ? args->children[0] : NULL;
}

static int xl6CgenChannelCtorCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* callee)
{
    const XL6_AstNode* args;
    const XL6_AstNode* capArg;

    if ( !xl6CgenIsChannelGenericInstant(callee) ) {
        return 0;
    }

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    capArg = args != NULL && args->childCount > 0 && args->children[0] != NULL &&
             args->children[0]->childCount > 0 ? args->children[0]->children[0] : NULL;

    xl6OutputPuts("xl6_channel_require_handle(xrtChannelCreate(", ctx->out);
    if ( capArg != NULL ) {
        xl6CgenExpr(ctx, capArg);
    } else {
        xl6OutputPuts("0", ctx->out);
    }
    xl6OutputPrintf(ctx->out, "), \"\", %d, %d)", callee->line, callee->column);
    return 1;
}

static void xl6CgenChannelReceiverExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    if ( receiver != NULL && receiver->kind == XL6_AST_NAME ) {
        xl6CgenFputsCIdent(ctx->out, receiver->text);
        return;
    }
    xl6CgenExpr(ctx, receiver);
}

static int xl6CgenChannelMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* itemType;
    const XL6_AstNode* args;
    const XL6_AstNode* arg;
    const XL6_AstNode* argExpr;
    const char* cType;

    if ( call == NULL || member == NULL || member->kind != XL6_AST_MEMBER ) {
        return 0;
    }
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return 0;
    }
    receiverType = xl6CgenFindVarType(ctx, receiver->text);
    if ( !xl6CgenIsChannelType(receiverType) ) {
        return 0;
    }

    if ( xl6CTextEquals(member->text, "close") ) {
        xl6OutputPuts("(xrtChannelClose(", ctx->out);
        xl6CgenChannelReceiverExpr(ctx, receiver);
        xl6OutputPuts("), 0)", ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(member->text, "count") ) {
        xl6OutputPuts("xrtChannelCount(", ctx->out);
        xl6CgenChannelReceiverExpr(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(member->text, "isClosed") ) {
        xl6OutputPuts("xrtChannelIsClosed(", ctx->out);
        xl6CgenChannelReceiverExpr(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        return 1;
    }

    itemType = xl6CgenChannelElementType(receiverType);
    cType = itemType != NULL ? xl6CgenTypeName(ctx, itemType) : "int";
    args = call->childCount > 1 ? call->children[1] : NULL;
    arg = args != NULL && args->childCount == 1 ? args->children[0] : NULL;
    argExpr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;

    if ( xl6CTextEquals(member->text, "send") ) {
        xl6OutputPuts("xl6_channel_send_value(", ctx->out);
        xl6CgenChannelReceiverExpr(ctx, receiver);
        xl6OutputPuts(", &(", ctx->out);
        xl6OutputPuts(cType, ctx->out);
        xl6OutputPuts("){", ctx->out);
        xl6CgenExpr(ctx, argExpr);
        xl6OutputPuts("}, sizeof(", ctx->out);
        xl6OutputPuts(cType, ctx->out);
        xl6OutputPrintf(ctx->out, "), \"\", %d, %d)", member->line, member->column);
        return 1;
    }

    if ( xl6CTextEquals(member->text, "recvInto") ||
         xl6CTextEquals(member->text, "tryRecvInto") ) {
        xl6OutputPuts("xl6_channel_recv_into(", ctx->out);
        xl6CgenChannelReceiverExpr(ctx, receiver);
        xl6OutputPuts(", &", ctx->out);
        if ( argExpr != NULL && argExpr->kind == XL6_AST_NAME ) {
            xl6CgenFputsCIdent(ctx->out, argExpr->text);
        } else {
            xl6CgenExpr(ctx, argExpr);
        }
        xl6OutputPuts(", sizeof(", ctx->out);
        xl6OutputPuts(cType, ctx->out);
        xl6OutputPrintf(ctx->out, "), %d, \"\", %d, %d)",
                        xl6CTextEquals(member->text, "recvInto") ? 1 : 0,
                        member->line,
                        member->column);
        return 1;
    }

    return 0;
}

static const XL6_AstNode* xl6CgenCallArgExpr(const XL6_AstNode* call, size_t index)
{
    const XL6_AstNode* args;
    const XL6_AstNode* arg;

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    arg = args != NULL && index < args->childCount ? args->children[index] : NULL;
    return arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
}

static int xl6CgenSyncCtorCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* callee)
{
    if ( callee == NULL || callee->kind != XL6_AST_NAME || !xl6CgenIsSyncHandleTypeName(callee->text) ) {
        return 0;
    }
    if ( xl6CTextEquals(callee->text, "mutex") ) {
        xl6OutputPrintf(ctx->out,
                        "xl6_sync_require_handle(xrtMutexCreate(), \"mutex create failed\", \"\", %d, %d)",
                        callee->line,
                        callee->column);
        return 1;
    }
    if ( xl6CTextEquals(callee->text, "cond") ) {
        xl6OutputPrintf(ctx->out,
                        "xl6_sync_require_handle(xrtCondCreate(), \"cond create failed\", \"\", %d, %d)",
                        callee->line,
                        callee->column);
        return 1;
    }
    if ( xl6CTextEquals(callee->text, "rwlock") ) {
        xl6OutputPrintf(ctx->out,
                        "xl6_sync_require_handle(xrtRWLockCreate(), \"rwlock create failed\", \"\", %d, %d)",
                        callee->line,
                        callee->column);
        return 1;
    }
    if ( xl6CTextEquals(callee->text, "sem") ) {
        xl6OutputPuts("xl6_sync_require_handle(xrtSemCreate(", ctx->out);
        xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 0));
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 1));
        xl6OutputPrintf(ctx->out, "), \"sem create failed\", \"\", %d, %d)", callee->line, callee->column);
        return 1;
    }
    return 0;
}

static void xl6CgenSyncReceiver(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    if ( receiver != NULL && receiver->kind == XL6_AST_NAME ) {
        xl6CgenFputsCIdent(ctx->out, receiver->text);
        return;
    }
    xl6CgenExpr(ctx, receiver);
}

static int xl6CgenSyncMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const char* typeName;

    if ( call == NULL || member == NULL || member->kind != XL6_AST_MEMBER ) {
        return 0;
    }
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return 0;
    }
    receiverType = xl6CgenFindVarType(ctx, receiver->text);
    if ( !xl6CgenIsSyncHandleType(receiverType) ) {
        return 0;
    }
    typeName = receiverType->text;

    if ( xl6CTextEquals(typeName, "mutex") ) {
        if ( xl6CTextEquals(member->text, "lock") ) {
            xl6OutputPuts("(xrtMutexLock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "tryLock") ) {
            xl6OutputPuts("xrtMutexTryLock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "unlock") ) {
            xl6OutputPuts("(xrtMutexUnlock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
    }

    if ( xl6CTextEquals(typeName, "sem") ) {
        if ( xl6CTextEquals(member->text, "wait") ) {
            xl6OutputPuts("(xrtSemWait(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "tryWait") ) {
            xl6OutputPuts("xrtSemTryWait(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "waitTimeout") ) {
            xl6OutputPuts("(xrtSemWaitTimeout(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 0));
            xl6OutputPuts(") == 0)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "post") ) {
            xl6OutputPuts("xrtSemPost(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "postMultiple") ) {
            xl6OutputPuts("xrtSemPostMultiple(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 0));
            xl6OutputPutc(')', ctx->out);
            return 1;
        }
    }

    if ( xl6CTextEquals(typeName, "cond") ) {
        if ( xl6CTextEquals(member->text, "wait") ) {
            xl6OutputPuts("(xrtCondWait(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 0));
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "waitTimeout") ) {
            xl6OutputPuts("(xrtCondWaitTimeout(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 0));
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 1));
            xl6OutputPuts(") == 0)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "signal") ) {
            xl6OutputPuts("(xrtCondSignal(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "broadcast") ) {
            xl6OutputPuts("(xrtCondBroadcast(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
    }

    if ( xl6CTextEquals(typeName, "rwlock") ) {
        if ( xl6CTextEquals(member->text, "readLock") ) {
            xl6OutputPuts("(xrtRWLockReadLock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "tryReadLock") ) {
            xl6OutputPuts("xrtRWLockTryReadLock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "readUnlock") ) {
            xl6OutputPuts("(xrtRWLockReadUnlock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "writeLock") ) {
            xl6OutputPuts("(xrtRWLockWriteLock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "tryWriteLock") ) {
            xl6OutputPuts("xrtRWLockTryWriteLock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "writeUnlock") ) {
            xl6OutputPuts("(xrtRWLockWriteUnlock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "downgrade") ) {
            xl6OutputPuts("(xrtRWLockDowngrade(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "upgrade") ) {
            xl6OutputPuts("xrtRWLockUpgrade(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
            return 1;
        }
    }

    return 0;
}

static XL6_AstNode* xl6CgenThreadEntryType(const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* params;
    XL6_AstNode* returns;
    XL6_AstNode* intType;

    typeNode = xl6CgenNewSimpleType("function", at);
    params = xl6AstNew(XL6_AST_LIST, NULL);
    returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    intType = xl6CgenNewSimpleType("int", at);
    if ( typeNode == NULL || params == NULL || returns == NULL || intType == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(params);
        xl6AstFree(returns);
        xl6AstFree(intType);
        return NULL;
    }
    xl6AstAddChild(returns, intType);
    xl6AstAddChild(typeNode, params);
    xl6AstAddChild(typeNode, returns);
    return typeNode;
}

static int xl6CgenThreadCtorCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* callee)
{
    const XL6_AstNode* entryExpr;
    XL6_AstNode* targetType;

    if ( callee == NULL || callee->kind != XL6_AST_NAME || !xl6CTextEquals(callee->text, "thread") ) {
        return 0;
    }

    entryExpr = xl6CgenCallArgExpr(call, 0);
    targetType = xl6CgenThreadEntryType(callee);
    xl6OutputPuts("xl6_thread_require_handle(xl6_thread_start(", ctx->out);
    if ( targetType != NULL && entryExpr != NULL ) {
        xl6CgenFunctionValueExpr(ctx, entryExpr, targetType);
    } else {
        xl6OutputPuts("(XL6_Function){0}", ctx->out);
    }
    xl6OutputPrintf(ctx->out,
                    ", %d), \"\", %d, %d)",
                    xl6CgenExprIsFunctionBorrow(ctx, entryExpr) ? 1 : 0,
                    callee->line,
                    callee->column);
    xl6AstFree(targetType);
    return 1;
}

static void xl6CgenThreadReceiver(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    if ( receiver != NULL && receiver->kind == XL6_AST_NAME ) {
        xl6CgenFputsCIdent(ctx->out, receiver->text);
        return;
    }
    xl6CgenExpr(ctx, receiver);
}

static int xl6CgenThreadMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;

    if ( call == NULL || member == NULL || member->kind != XL6_AST_MEMBER ) {
        return 0;
    }
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return 0;
    }
    receiverType = xl6CgenFindVarType(ctx, receiver->text);
    if ( !xl6CgenIsThreadType(receiverType) ) {
        return 0;
    }

    if ( xl6CTextEquals(member->text, "wait") ) {
        xl6OutputPuts("(xrtThreadWait(", ctx->out);
        xl6CgenThreadReceiver(ctx, receiver);
        xl6OutputPuts("), true)", ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(member->text, "waitTimeout") ) {
        xl6OutputPuts("(xrtThreadWaitTimeout(", ctx->out);
        xl6CgenThreadReceiver(ctx, receiver);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 0));
        xl6OutputPuts(") == 0)", ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(member->text, "stop") ) {
        xl6OutputPuts("(xrtThreadStop(", ctx->out);
        xl6CgenThreadReceiver(ctx, receiver);
        xl6OutputPuts("), true)", ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(member->text, "state") ) {
        xl6OutputPuts("xrtThreadGetState(", ctx->out);
        xl6CgenThreadReceiver(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(member->text, "exitCode") ) {
        xl6OutputPuts("(int)xrtThreadGetExitCode(", ctx->out);
        xl6CgenThreadReceiver(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        return 1;
    }
    return 0;
}
