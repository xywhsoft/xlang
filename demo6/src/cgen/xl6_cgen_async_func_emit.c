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
        xl6CgenBeginTempScope(ctx, node);
        for ( i = 0; i < params->childCount; ++i ) {
            xl6CgenAsyncTakeArgFromFrame(ctx, params->children[i]);
        }
        xl6CgenIndent(ctx);
        xl6OutputPuts("xrtFree(__xl6_args);\n", ctx->out);
    } else {
        xl6CgenBeginTempScope(ctx, node);
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
    xl6CgenEmitManagedParamOwners(ctx, params);
    xl6CgenRegisterAsyncParamOwnerCleanups(ctx, params);
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
    xl6CgenAsyncArgsFreeFunc(ctx, node);
    xl6CgenAsyncResultFreeFunc(ctx, node);
    xl6CgenAsyncClassAwaitReleaseHelper(ctx, node);
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
        xl6OutputPrintf(ctx->out,
                        "xl6_error_raise(\"AsyncError\", \"async task args allocation failed\", \"\", %d, %d);\n",
                        node->line,
                        node->column);
        xl6CgenIndent(ctx);
        xl6OutputPuts("return 0;\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("memset(__xl6_args, 0, sizeof(*__xl6_args));\n", ctx->out);
        for ( i = 0; i < params->childCount; ++i ) {
            xl6CgenAsyncCopyArgToFrame(ctx, params->children[i]);
        }
        xl6CgenIndent(ctx);
        xl6OutputPuts("(void)xl6_error_active_get();\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("__xl6_future = xTaskRunThread(", ctx->out);
        xl6CgenAsyncWorkerSymbol(ctx, node);
        xl6OutputPuts(", __xl6_args, 0);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( __xl6_future == 0 ) {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6CgenAsyncArgsFreeSymbol(ctx, node);
        xl6OutputPuts("(__xl6_args);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out,
                        "xl6_error_raise(\"AsyncError\", \"async task start failed\", \"\", %d, %d);\n",
                        node->line,
                        node->column);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("return __xl6_future;\n", ctx->out);
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPuts("(void)xl6_error_active_get();\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("return xl6_async_require_future(xTaskRunThread(", ctx->out);
        xl6CgenAsyncWorkerSymbol(ctx, node);
        xl6OutputPrintf(ctx->out, ", 0, 0), \"\", %d, %d);\n", node->line, node->column);
    }
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}
