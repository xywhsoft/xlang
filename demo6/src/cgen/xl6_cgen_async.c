static int xl6CgenIsAsyncFunc(const XL6_AstNode* node) { return node != NULL && node->kind == XL6_AST_ASYNC_FUNC; }
static const XL6_AstNode* xl6CgenAsyncReturns(const XL6_AstNode* node) { return node != NULL && node->childCount > 2 ? node->children[2] : NULL; }
static const XL6_AstNode* xl6CgenAsyncParams(const XL6_AstNode* node) { return node != NULL && node->childCount > 1 ? node->children[1] : NULL; }
static const XL6_AstNode* xl6CgenAsyncBody(const XL6_AstNode* node) { return node != NULL && node->childCount > 3 ? node->children[3] : NULL; }

static const XL6_AstNode* xl6CgenAsyncResultType(const XL6_AstNode* node)
{
    const XL6_AstNode* returns = xl6CgenAsyncReturns(node);
    return returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
}

static const char* xl6CgenAsyncParamCType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        return xl6CgenTypedContainerCType(ctx, typeNode);
    }
    return xl6CgenTypeName(ctx, typeNode);
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
            xl6OutputPuts(xl6CgenAsyncParamCType(ctx, param->children[0]), ctx->out);
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
             callee->childCount > 0 &&
             callee->children[0] != NULL &&
             callee->children[0]->kind == XL6_AST_NAME &&
             xl6CTextEquals(callee->children[0]->text, "task") &&
             (xl6CTextEquals(callee->text, "spawn") ||
              xl6CTextEquals(callee->text, "all") ||
              xl6CTextEquals(callee->text, "whenAny") ||
              xl6CTextEquals(callee->text, "race") ||
              xl6CTextEquals(callee->text, "cancel")) ) {
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
              xl6CTextEquals(callee->text, "waitUntil") ||
              xl6CTextEquals(callee->text, "setDebugName") ||
              xl6CTextEquals(callee->text, "debugName") ||
              xl6CTextEquals(callee->text, "createTimeMs") ||
              xl6CTextEquals(callee->text, "completeTimeMs") ||
              xl6CTextEquals(callee->text, "pendingContinuations") ||
              xl6CTextEquals(callee->text, "then") ||
              xl6CTextEquals(callee->text, "catch") ||
              xl6CTextEquals(callee->text, "finally") ||
              xl6CTextEquals(callee->text, "isDone") ||
              xl6CTextEquals(callee->text, "isPending") ||
              xl6CTextEquals(callee->text, "isCancelled") ||
              xl6CTextEquals(callee->text, "isFailed") ||
              xl6CTextEquals(callee->text, "stop") ||
              xl6CTextEquals(callee->text, "state") ||
              xl6CTextEquals(callee->text, "status") ||
              xl6CTextEquals(callee->text, "error") ||
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

static int xl6CgenNodeNeedsTaskSpawn(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 && node->children[0] != NULL ) {
        const XL6_AstNode* callee = node->children[0];
        if ( callee->kind == XL6_AST_MEMBER &&
             callee->childCount > 0 &&
             callee->children[0] != NULL &&
             callee->children[0]->kind == XL6_AST_NAME &&
             xl6CTextEquals(callee->children[0]->text, "task") &&
             (xl6CTextEquals(callee->text, "spawn") ||
              xl6CTextEquals(callee->text, "whenAny")) ) {
            return 1;
        }
        if ( callee->kind == XL6_AST_MEMBER &&
             (xl6CTextEquals(callee->text, "then") ||
              xl6CTextEquals(callee->text, "catch") ||
              xl6CTextEquals(callee->text, "finally")) ) {
            return 1;
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTaskSpawn(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}
