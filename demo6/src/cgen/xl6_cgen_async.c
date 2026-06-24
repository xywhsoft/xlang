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

    xl6OutputPuts("xrtChannelCreate(", ctx->out);
    if ( capArg != NULL ) {
        xl6CgenExpr(ctx, capArg);
    } else {
        xl6OutputPuts("0", ctx->out);
    }
    xl6OutputPutc(')', ctx->out);
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
        xl6OutputPuts("xrtMutexCreate()", ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(callee->text, "cond") ) {
        xl6OutputPuts("xrtCondCreate()", ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(callee->text, "rwlock") ) {
        xl6OutputPuts("xrtRWLockCreate()", ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(callee->text, "sem") ) {
        xl6OutputPuts("xrtSemCreate(", ctx->out);
        xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 0));
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 1));
        xl6OutputPutc(')', ctx->out);
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
    xl6OutputPuts("xl6_thread_start(", ctx->out);
    if ( targetType != NULL && entryExpr != NULL ) {
        xl6CgenFunctionValueExpr(ctx, entryExpr, targetType);
    } else {
        xl6OutputPuts("(XL6_Function){0}", ctx->out);
    }
    xl6OutputPrintf(ctx->out, ", %d)", xl6CgenExprIsFunctionBorrow(ctx, entryExpr) ? 1 : 0);
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

static void xl6CgenAsyncArgStructDecl(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* params = xl6CgenAsyncParams(node);
    size_t i;

    if ( params == NULL || params->childCount == 0 ) {
        return;
    }

    xl6OutputPuts("typedef struct ", ctx->out);
    xl6CgenAsyncArgsStructName(ctx, node);
    xl6OutputPuts(" {\n", ctx->out);
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];

        xl6OutputPuts("    ", ctx->out);
        if ( param != NULL && param->childCount > 0 ) {
            xl6OutputPuts(xl6CgenTypeName(ctx, param->children[0]), ctx->out);
        } else {
            xl6OutputPuts("int", ctx->out);
        }
        xl6OutputPutc(' ', ctx->out);
        xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : NULL);
        xl6OutputPuts(";\n", ctx->out);
    }
    xl6OutputPuts("} ", ctx->out);
    xl6CgenAsyncArgsStructName(ctx, node);
    xl6OutputPuts(";\n\n", ctx->out);
}

static void xl6CgenAsyncResultFreeFunc(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* returns;
    const XL6_AstNode* resultType;
    XL6_AstNode* classNode;
    int isGenericClass;

    returns = node != NULL && node->childCount > 2 ? node->children[2] : NULL;
    resultType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    if ( !xl6CgenClassTypeNeedsFunctionFieldCleanup(ctx, resultType) ) {
        return;
    }

    classNode = xl6CgenClassNodeForType(ctx, resultType);
    if ( classNode == NULL ) {
        return;
    }
    isGenericClass = resultType != NULL && resultType->childCount > 0 && xl6CgenClassHasTypeParams(classNode);

    xl6OutputPuts("static void ", ctx->out);
    xl6CgenAsyncResultFreeSymbol(ctx, node);
    xl6OutputPuts("(void* pValue)\n{\n", ctx->out);
    xl6OutputPuts("    if ( pValue != 0 ) {\n", ctx->out);
    xl6OutputPuts("        ", ctx->out);
    if ( isGenericClass ) {
        xl6CgenGenericClassMemberSymbol(ctx, classNode, resultType, "destroy");
    } else {
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
    }
    xl6OutputPuts("((", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, resultType), ctx->out);
    xl6OutputPuts("*)pValue);\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    xrtFree(pValue);\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenAsyncResultFreeValue(XL6_CgenContext* ctx, const XL6_AstNode* funcNode, const XL6_AstNode* resultType)
{
    if ( xl6CgenIsFunctionType(resultType) ) {
        xl6OutputPuts("xl6_async_free_function_value", ctx->out);
        return;
    }
    if ( xl6CgenClassTypeNeedsFunctionFieldCleanup(ctx, resultType) ) {
        xl6CgenAsyncResultFreeSymbol(ctx, funcNode);
        return;
    }
    xl6OutputPuts("xrtFree", ctx->out);
}

static void xl6CgenAsyncReturnStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* resultType = ctx->currentReturnType;
    int id = ctx->tempId++;

    if ( node == NULL || node->childCount == 0 || resultType == NULL ) {
        xl6CgenEmitCleanupsSince(ctx, 0);
        xl6CgenIndent(ctx);
        xl6OutputPuts("xl6_async_result_ok(pOut);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("return 0;\n", ctx->out);
        return;
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts(xl6CgenTypeName(ctx, resultType), ctx->out);
    xl6OutputPrintf(ctx->out, " __xl6_async_return_%d = ", id);
    if ( xl6CgenIsXValueType(resultType) ) {
        xl6CgenXValueFromExprForTarget(ctx, resultType, node->children[0]);
    } else if ( xl6CgenIsFunctionType(resultType) ) {
        xl6CgenFunctionValueExpr(ctx, node->children[0], resultType);
    } else {
        xl6CgenExpr(ctx, node->children[0]);
    }
    xl6OutputPuts(";\n", ctx->out);
    if ( xl6CgenIsFunctionType(resultType) && xl6CgenExprIsFunctionBorrow(ctx, node->children[0]) ) {
        char returnName[64];
        snprintf(returnName, sizeof(returnName), "__xl6_async_return_%d", id);
        xl6CgenRetainFunctionValue(ctx, returnName);
    }
    {
        char returnName[64];
        snprintf(returnName, sizeof(returnName), "__xl6_async_return_%d", id);
        xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, resultType, returnName, node->children[0]);
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts(xl6CgenTypeName(ctx, resultType), ctx->out);
    xl6OutputPrintf(ctx->out, "* __xl6_async_box_%d = (", id);
    xl6OutputPuts(xl6CgenTypeName(ctx, resultType), ctx->out);
    xl6OutputPuts("*)xrtMalloc(sizeof(", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, resultType), ctx->out);
    xl6OutputPuts("));\n", ctx->out);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if ( __xl6_async_box_%d == 0 ) {\n", id);
    ctx->indent += 1;
    xl6CgenEmitCleanupsSince(ctx, 0);
    xl6CgenIndent(ctx);
    xl6OutputPuts("return xl6_async_result_error(pOut, \"out of memory while completing async task\");\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "*__xl6_async_box_%d = __xl6_async_return_%d;\n", id, id);
    xl6CgenIndent(ctx);
    xl6OutputPuts("pOut->iStatus = 0;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "pOut->pValue = __xl6_async_box_%d;\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts("pOut->sError = 0;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("pOut->pfnFreeValue = ", ctx->out);
    xl6CgenAsyncResultFreeValue(ctx, ctx->currentReturnFunc, resultType);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("pOut->pfnFreeError = 0;\n", ctx->out);
    xl6CgenEmitCleanupsSince(ctx, 0);
    xl6CgenIndent(ctx);
    xl6OutputPuts("return 0;\n", ctx->out);
}

static void xl6CgenAwaitExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* resultType;
    int requireValue;

    resultType = xl6CgenCloneAwaitResultType(ctx, node);
    requireValue = resultType != NULL && !xl6CTextEquals(resultType->text, "null");

    if ( !requireValue ) {
        xl6OutputPuts("(xl6_future_await_value(", ctx->out);
        if ( node != NULL && node->childCount > 0 ) {
            xl6CgenExpr(ctx, node->children[0]);
        } else {
            xl6OutputPuts("0", ctx->out);
        }
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d, 0, 0), 0)", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
        xl6AstFree(resultType);
        return;
    }

    xl6OutputPuts("(*(", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, resultType), ctx->out);
    xl6OutputPuts("*)xl6_future_await_value(", ctx->out);
    if ( node != NULL && node->childCount > 0 ) {
        xl6CgenExpr(ctx, node->children[0]);
    } else {
        xl6OutputPuts("0", ctx->out);
    }
    xl6OutputPrintf(ctx->out, ", \"\", %d, %d, 1, sizeof(", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
    xl6OutputPuts(xl6CgenTypeName(ctx, resultType), ctx->out);
    xl6OutputPuts(")))", ctx->out);
    xl6AstFree(resultType);
}

static void xl6CgenAsyncWorker(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* params = xl6CgenAsyncParams(node);
    const XL6_AstNode* body = xl6CgenAsyncBody(node);
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevReturnParams;
    const XL6_AstNode* prevReturns;
    const XL6_AstNode* prevReturnFunc;
    const char* prevErrorExitLabel;
    int prevAsyncWorker;
    size_t varMark;
    size_t cleanupMark;
    size_t i;

    xl6OutputPuts("static int ", ctx->out);
    xl6CgenAsyncWorkerSymbol(ctx, node);
    xl6OutputPuts("(void* __xl6_arg, xfuture_result* pOut)\n{\n", ctx->out);
    ctx->indent += 1;

    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    prevReturnType = ctx->currentReturnType;
    prevReturnParams = ctx->currentReturnParams;
    prevReturns = ctx->currentReturns;
    prevReturnFunc = ctx->currentReturnFunc;
    prevErrorExitLabel = ctx->currentErrorExitLabel;
    prevAsyncWorker = ctx->currentAsyncWorker;

    if ( params != NULL && params->childCount > 0 ) {
        xl6CgenIndent(ctx);
        xl6CgenAsyncArgsStructName(ctx, node);
        xl6OutputPuts("* __xl6_args = (", ctx->out);
        xl6CgenAsyncArgsStructName(ctx, node);
        xl6OutputPuts("*)__xl6_arg;\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( __xl6_args == 0 ) {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("return xl6_async_result_error(pOut, \"missing async argument frame\");\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];

            xl6CgenIndent(ctx);
            xl6OutputPuts(param != NULL && param->childCount > 0 ? xl6CgenTypeName(ctx, param->children[0]) : "int", ctx->out);
            xl6OutputPutc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : NULL);
            xl6OutputPuts(" = __xl6_args->", ctx->out);
            xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : NULL);
            xl6OutputPuts(";\n", ctx->out);
        }
        xl6CgenIndent(ctx);
        xl6OutputPuts("xrtFree(__xl6_args);\n", ctx->out);
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPuts("(void)__xl6_arg;\n", ctx->out);
    }

    ctx->currentReturnType = xl6CgenAsyncResultType(node);
    ctx->currentReturnParams = params;
    ctx->currentReturns = xl6CgenAsyncReturns(node);
    ctx->currentReturnFunc = node;
    ctx->currentErrorExitLabel = "__xl6_async_error";
    ctx->currentAsyncWorker = 1;
    xl6CgenRegisterParamTypes(ctx, params);
    if ( body != NULL ) {
        xl6CgenBlock(ctx, body);
    }
    if ( !xl6CgenBlockEndsWithReturn(body) ) {
        xl6CgenEmitCleanupsSince(ctx, cleanupMark);
        xl6CgenIndent(ctx);
        xl6OutputPuts("xl6_async_result_ok(pOut);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("return 0;\n", ctx->out);
    }

    ctx->indent -= 1;
    xl6OutputPuts("__xl6_async_error:\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( xl6_error_has() ) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("XL6_Error* __xl6_error = xl6_error_take();\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("const char* __xl6_message = xl6_error_message(__xl6_error);\n", ctx->out);
    xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    xl6CgenIndent(ctx);
    xl6OutputPuts("xl6_async_result_error(pOut, __xl6_message);\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("xl6_error_free(__xl6_error);\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("return -1;\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("return -1;\n", ctx->out);

    ctx->currentReturnType = prevReturnType;
    ctx->currentReturnParams = prevReturnParams;
    ctx->currentReturns = prevReturns;
    ctx->currentReturnFunc = prevReturnFunc;
    ctx->currentErrorExitLabel = prevErrorExitLabel;
    ctx->currentAsyncWorker = prevAsyncWorker;
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    xl6CgenFreeVarsSince(ctx, varMark);

    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenAsyncFunc(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* params = xl6CgenAsyncParams(node);
    size_t i;

    xl6CgenAsyncArgStructDecl(ctx, node);
    xl6CgenAsyncResultFreeFunc(ctx, node);
    xl6CgenAsyncWorker(ctx, node);

    xl6CgenAsyncFuncProto(ctx, node);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    if ( params != NULL && params->childCount > 0 ) {
        xl6CgenIndent(ctx);
        xl6CgenAsyncArgsStructName(ctx, node);
        xl6OutputPuts("* __xl6_args = (", ctx->out);
        xl6CgenAsyncArgsStructName(ctx, node);
        xl6OutputPuts("*)xrtMalloc(sizeof(", ctx->out);
        xl6CgenAsyncArgsStructName(ctx, node);
        xl6OutputPuts("));\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("xfuture* __xl6_future;\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( __xl6_args == 0 ) {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("return 0;\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];

            xl6CgenIndent(ctx);
            xl6OutputPuts("__xl6_args->", ctx->out);
            xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : NULL);
            xl6OutputPuts(" = ", ctx->out);
            xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : NULL);
            xl6OutputPuts(";\n", ctx->out);
        }
        xl6CgenIndent(ctx);
        xl6OutputPuts("__xl6_future = xTaskRunThread(", ctx->out);
        xl6CgenAsyncWorkerSymbol(ctx, node);
        xl6OutputPuts(", __xl6_args, 0);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( __xl6_future == 0 ) {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("xrtFree(__xl6_args);\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("return __xl6_future;\n", ctx->out);
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPuts("return xTaskRunThread(", ctx->out);
        xl6CgenAsyncWorkerSymbol(ctx, node);
        xl6OutputPuts(", 0, 0);\n", ctx->out);
    }
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}
