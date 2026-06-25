static int xl6CgenNodeNeedsErrorRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_FOR &&
         node->childCount > 1 &&
         xl6CgenIsRangeCall(node->children[1]) &&
         node->children[1] != NULL &&
         node->children[1]->childCount > 1 &&
         node->children[1]->children[1] != NULL &&
         node->children[1]->children[1]->childCount >= 3 ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TRY ||
         node->kind == XL6_AST_THROW ||
         node->kind == XL6_AST_ASSERT ||
         node->kind == XL6_AST_AWAIT ||
         node->kind == XL6_AST_ASYNC_FUNC ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsErrorRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenEmitErrorRuntime(XL6_Output* out)
{
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* pMem);\n\n", out);
    xl6OutputPuts("#ifndef XL6_ERROR_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_ERROR_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct XL6_Error XL6_Error;\n", out);
    xl6OutputPuts("struct XL6_Error {\n", out);
    xl6OutputPuts("    int refCount;\n", out);
    xl6OutputPuts("    char* kind;\n", out);
    xl6OutputPuts("    char* message;\n", out);
    xl6OutputPuts("    char* file;\n", out);
    xl6OutputPuts("    int line;\n", out);
    xl6OutputPuts("    int column;\n", out);
    xl6OutputPuts("    XL6_Error* cause;\n", out);
    xl6OutputPuts("};\n", out);
    xl6OutputPuts("#endif\n\n", out);
    xl6OutputPuts("static XL6_Error* xl6__active_error = 0;\n\n", out);

    xl6OutputPuts("static char* xl6_error_dup(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len = text != 0 ? strlen(text) : 0;\n", out);
    xl6OutputPuts("    char* copy = (char*)xrtMalloc(len + 1);\n", out);
    xl6OutputPuts("    if ( copy == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( len > 0 ) {\n", out);
    xl6OutputPuts("        memcpy(copy, text, len);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    copy[len] = 0;\n", out);
    xl6OutputPuts("    return copy;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_error_retain(XL6_Error* error)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( error != 0 ) {\n", out);
    xl6OutputPuts("        error->refCount += 1;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_error_free(XL6_Error* error)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( error == 0 ) {\n", out);
    xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    error->refCount -= 1;\n", out);
    xl6OutputPuts("    if ( error->refCount > 0 ) {\n", out);
    xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xl6_error_free(error->cause);\n", out);
    xl6OutputPuts("    xrtFree(error->kind);\n", out);
    xl6OutputPuts("    xrtFree(error->message);\n", out);
    xl6OutputPuts("    xrtFree(error->file);\n", out);
    xl6OutputPuts("    xrtFree(error);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static XL6_Error* xl6_error_new(const char* kind, const char* message, const char* file, int line, int column, XL6_Error* cause)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Error* error = (XL6_Error*)xrtMalloc(sizeof(XL6_Error));\n", out);
    xl6OutputPuts("    if ( error == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    error->refCount = 1;\n", out);
    xl6OutputPuts("    error->kind = xl6_error_dup(kind != 0 ? kind : \"Error\");\n", out);
    xl6OutputPuts("    error->message = xl6_error_dup(message != 0 ? message : \"\");\n", out);
    xl6OutputPuts("    error->file = xl6_error_dup(file != 0 ? file : \"\");\n", out);
    xl6OutputPuts("    error->line = line;\n", out);
    xl6OutputPuts("    error->column = column;\n", out);
    xl6OutputPuts("    error->cause = cause;\n", out);
    xl6OutputPuts("    if ( error->kind == 0 || error->message == 0 || error->file == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_free(error);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return error;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_error_has(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6__active_error != 0;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static XL6_Error* xl6_error_take(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Error* error = xl6__active_error;\n", out);
    xl6OutputPuts("    xl6__active_error = 0;\n", out);
    xl6OutputPuts("    return error;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void xl6_error_clear(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Error* error = xl6_error_take();\n", out);
    xl6OutputPuts("    xl6_error_free(error);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void xl6_error_set_active(XL6_Error* error)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xl6_error_clear();\n", out);
    xl6OutputPuts("    xl6__active_error = error;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void xl6_error_reraise(XL6_Error* error)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xl6_error_retain(error);\n", out);
    xl6OutputPuts("    xl6_error_set_active(error);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void xl6_error_raise(const char* kind, const char* message, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Error* cause = xl6_error_take();\n", out);
    xl6OutputPuts("    XL6_Error* error = xl6_error_new(kind, message, file, line, column, cause);\n", out);
    xl6OutputPuts("    if ( error == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_free(cause);\n", out);
    xl6OutputPuts("        error = xl6_error_new(\"Error\", \"out of memory while creating error\", file, line, column, 0);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xl6_error_set_active(error);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static const char* xl6_error_kind(XL6_Error* error) { return error != 0 && error->kind != 0 ? error->kind : \"\"; }\n", out);
    xl6OutputPuts("static const char* xl6_error_message(XL6_Error* error) { return error != 0 && error->message != 0 ? error->message : \"\"; }\n", out);
    xl6OutputPuts("static const char* xl6_error_file(XL6_Error* error) { return error != 0 && error->file != 0 ? error->file : \"\"; }\n", out);
    xl6OutputPuts("static int xl6_error_line(XL6_Error* error) { return error != 0 ? error->line : 0; }\n", out);
    xl6OutputPuts("static int xl6_error_column(XL6_Error* error) { return error != 0 ? error->column : 0; }\n\n", out);
}

static void xl6CgenEmitDefaultErrorReturn(XL6_CgenContext* ctx)
{
    if ( ctx->currentReturns == NULL || ctx->currentReturns->childCount == 0 ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("return;\n", ctx->out);
        return;
    }

    xl6CgenIndent(ctx);
    if ( ctx->currentReturns->childCount > 1 ) {
        if ( ctx->currentReturnFunc != NULL ) {
            xl6CgenFuncReturnStructName(ctx, ctx->currentReturnFunc);
        } else {
            xl6CgenFunctionReturnStructName(ctx, ctx->currentReturnParams, ctx->currentReturns);
        }
    } else {
        xl6OutputPuts(xl6CgenTypeName(ctx, ctx->currentReturns->children[0]), ctx->out);
    }
    xl6OutputPuts(" __xl6_error_return;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("memset(&__xl6_error_return, 0, sizeof(__xl6_error_return));\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("return __xl6_error_return;\n", ctx->out);
}

static void xl6CgenEmitExceptionPropagate(XL6_CgenContext* ctx, const XL6_AstNode* at)
{
    XL6_CgenTryTarget* target;

    (void)at;
    if ( ctx == NULL || !ctx->needsErrorRuntime ) {
        return;
    }

    target = xl6CgenCurrentTryTarget(ctx);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( xl6_error_has() ) {\n", ctx->out);
    ctx->indent += 1;
    if ( target != NULL ) {
        xl6CgenEmitCleanupsSince(ctx, target->cleanupMark);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out,
                "goto __xl6_try_%s_%d;\n",
                target->targetFinally ? "finally" : "catch",
                target->id);
    } else {
        xl6CgenEmitCleanupsSince(ctx, 0);
        if ( ctx->currentErrorExitLabel != NULL ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "goto %s;\n", ctx->currentErrorExitLabel);
        } else {
            xl6CgenEmitDefaultErrorReturn(ctx);
        }
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenEmitGotoIfError(XL6_CgenContext* ctx, const char* label)
{
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( xl6_error_has() ) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "goto %s;\n", label);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenThrowStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* expr;

    expr = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    xl6CgenIndent(ctx);
    if ( expr == NULL ) {
        if ( ctx->currentCatchErrorName != NULL ) {
            xl6OutputPrintf(ctx->out, "xl6_error_reraise(%s);\n", ctx->currentCatchErrorName);
        } else {
            xl6OutputPuts("xl6_error_raise(\"Error\", \"rethrow without active catch\", \"\", 0, 0);\n", ctx->out);
        }
    } else {
        xl6OutputPuts("xl6_error_raise(\"Error\", ", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d);\n", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
    }
    xl6CgenEmitExceptionPropagate(ctx, node);
}

static void xl6CgenAssertStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* cond;
    const XL6_AstNode* message;

    cond = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    message = node != NULL && node->childCount > 1 ? node->children[1] : NULL;

    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( !(", ctx->out);
    xl6CgenExpr(ctx, cond);
    xl6OutputPuts(") ) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("xl6_error_raise(\"AssertError\", ", ctx->out);
    if ( message != NULL ) {
        xl6CgenExpr(ctx, message);
    } else {
        xl6OutputPuts("\"assert failed\"", ctx->out);
    }
    xl6OutputPrintf(ctx->out, ", \"\", %d, %d);\n", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
    xl6CgenEmitExceptionPropagate(ctx, node);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static int xl6CgenTryHasCatch(const XL6_AstNode* node)
{
    const XL6_AstNode* catchNode = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    return catchNode != NULL && catchNode->kind == XL6_AST_CATCH;
}

static int xl6CgenTryHasFinally(const XL6_AstNode* node)
{
    const XL6_AstNode* finallyBlock = node != NULL && node->childCount > 2 ? node->children[2] : NULL;
    return finallyBlock != NULL && finallyBlock->childCount > 0;
}

static void xl6CgenTryStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* tryBlock;
    const XL6_AstNode* catchNode;
    const XL6_AstNode* catchName;
    const XL6_AstNode* catchBlock;
    const XL6_AstNode* finallyBlock;
    const XL6_AstNode* errorType;
    const char* prevCatchErrorName;
    char catchCName[512];
    char catchLabel[64];
    char finallyLabel[64];
    char endLabel[64];
    int id;
    int hasCatch;
    int hasFinally;
    size_t tryCleanupMark;
    size_t catchCleanupMark;
    size_t catchVarMark;

    tryBlock = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    catchNode = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    finallyBlock = node != NULL && node->childCount > 2 ? node->children[2] : NULL;
    hasCatch = xl6CgenTryHasCatch(node);
    hasFinally = xl6CgenTryHasFinally(node);
    id = ctx->tempId++;

    snprintf(catchLabel, sizeof(catchLabel), "__xl6_try_catch_%d", id);
    snprintf(finallyLabel, sizeof(finallyLabel), "__xl6_try_finally_%d", id);
    snprintf(endLabel, sizeof(endLabel), "__xl6_try_end_%d", id);

    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;

    tryCleanupMark = ctx->cleanupCount;
    xl6CgenPushTryTarget(ctx, tryCleanupMark, id, hasCatch ? 0 : 1);
    xl6CgenBlock(ctx, tryBlock);
    xl6CgenPopTryTarget(ctx);
    xl6CgenEmitGotoIfError(ctx, hasCatch ? catchLabel : finallyLabel);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "goto %s;\n", hasFinally ? finallyLabel : endLabel);

    if ( hasCatch ) {
        catchName = catchNode->childCount > 0 ? catchNode->children[0] : NULL;
        catchBlock = catchNode->childCount > 1 ? catchNode->children[1] : NULL;
        xl6CgenCIdentText(catchCName, sizeof(catchCName), catchName != NULL ? catchName->text : "__xl6_error");
        catchVarMark = ctx->varCount;
        catchCleanupMark = ctx->cleanupCount;

        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s:\n", catchLabel);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "XL6_Error* %s = xl6_error_take();\n", catchCName);
        errorType = xl6CgenOwnSimpleType(ctx, "error", catchName != NULL ? catchName : catchNode);
        xl6CgenAddVarType(ctx, catchName != NULL ? catchName->text : "__xl6_error", errorType);
        xl6CgenAddErrorCleanup(ctx, catchName != NULL ? catchName->text : "__xl6_error", errorType);

        prevCatchErrorName = ctx->currentCatchErrorName;
        ctx->currentCatchErrorName = catchCName;
        if ( hasFinally ) {
            xl6CgenPushTryTarget(ctx, catchCleanupMark, id, 1);
        }
        xl6CgenBlock(ctx, catchBlock);
        if ( hasFinally ) {
            xl6CgenPopTryTarget(ctx);
        }
        ctx->currentCatchErrorName = prevCatchErrorName;
        xl6CgenCloseScope(ctx, catchCleanupMark);
        xl6CgenFreeVarsSince(ctx, catchVarMark);
        xl6CgenEmitGotoIfError(ctx, hasFinally ? finallyLabel : endLabel);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "goto %s;\n", hasFinally ? finallyLabel : endLabel);
    }

    if ( hasFinally ) {
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s:\n", finallyLabel);
        ctx->indent += 1;
        xl6CgenBlock(ctx, finallyBlock);
        xl6CgenEmitExceptionPropagate(ctx, node);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "goto %s;\n", endLabel);
    }

    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s:\n", endLabel);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts(";\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}
