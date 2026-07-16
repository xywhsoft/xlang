static int xl6CgenNodeNeedsErrorRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CALL &&
         node->childCount > 0 &&
         node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         xl6CTextEquals(node->children[0]->text, "__xl_std_xrt_file_read_text_strict") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_INDEX ||
         node->kind == XL6_AST_SAFE_INDEX ) {
        /*
         * 索引读写需要运行时边界诊断：
         * 赋值失败、错误 key 类型等场景应进入异常系统，而不是静默 fatal。
         */
        return 1;
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
    xl6OutputPuts("#include <stdio.h>\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* pMem);\n\n", out);
    xl6OutputPuts("void* xrtTempMemory(size_t iSize);\n\n", out);
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
    xl6OutputPuts("#if defined(_WIN32) || defined(_WIN64)\n", out);
    xl6OutputPuts("#include <windows.h>\n", out);
    xl6OutputPuts("static DWORD xl6__active_error_tls_index = TLS_OUT_OF_INDEXES;\n", out);
    xl6OutputPuts("static volatile LONG xl6__active_error_tls_state = 0;\n", out);
    xl6OutputPuts("\n", out);
    xl6OutputPuts("static int xl6_error_tls_ensure(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    LONG state = InterlockedCompareExchange(&xl6__active_error_tls_state, 1, 0);\n", out);
    xl6OutputPuts("    if ( state == 0 ) {\n", out);
    xl6OutputPuts("        DWORD index = TlsAlloc();\n", out);
    xl6OutputPuts("        if ( index == TLS_OUT_OF_INDEXES ) {\n", out);
    xl6OutputPuts("            InterlockedCompareExchange(&xl6__active_error_tls_state, 3, 1);\n", out);
    xl6OutputPuts("            return 0;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("        xl6__active_error_tls_index = index;\n", out);
    xl6OutputPuts("        InterlockedCompareExchange(&xl6__active_error_tls_state, 2, 1);\n", out);
    xl6OutputPuts("        return 1;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    while ( state == 1 ) {\n", out);
    xl6OutputPuts("        state = InterlockedCompareExchange(&xl6__active_error_tls_state, 1, 1);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return state == 2;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static XL6_Error* xl6_error_active_get(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( !xl6_error_tls_ensure() ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return (XL6_Error*)TlsGetValue(xl6__active_error_tls_index);\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void xl6_error_active_set(XL6_Error* error)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( xl6_error_tls_ensure() ) {\n", out);
    xl6OutputPuts("        TlsSetValue(xl6__active_error_tls_index, error);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("\nstatic void xl6_error_tls_unit(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( InterlockedCompareExchange(&xl6__active_error_tls_state, 3, 2) == 2 ) {\n", out);
    xl6OutputPuts("        TlsFree(xl6__active_error_tls_index);\n", out);
    xl6OutputPuts("        xl6__active_error_tls_index = TLS_OUT_OF_INDEXES;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("#else\n", out);
    xl6OutputPuts("#ifndef XL6_THREAD_LOCAL\n", out);
    xl6OutputPuts("#define XL6_THREAD_LOCAL __thread\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("static XL6_THREAD_LOCAL XL6_Error* xl6__active_error = 0;\n\n", out);
    xl6OutputPuts("static XL6_Error* xl6_error_active_get(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6__active_error;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void xl6_error_active_set(XL6_Error* error)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xl6__active_error = error;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("\nstatic void xl6_error_tls_unit(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("#endif\n\n", out);

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
    xl6OutputPuts("    return xl6_error_active_get() != 0;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static XL6_Error* xl6_error_take(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Error* error = xl6_error_active_get();\n", out);
    xl6OutputPuts("    xl6_error_active_set(0);\n", out);
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
    xl6OutputPuts("    xl6_error_active_set(error);\n", out);
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
    xl6OutputPuts("static const char* xl6_error_text_copy(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len = text != 0 ? strlen(text) : 0;\n", out);
    xl6OutputPuts("    char* copy;\n\n", out);
    xl6OutputPuts("    if ( len == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    copy = (char*)xrtTempMemory(len + 1);\n", out);
    xl6OutputPuts("    if ( copy == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    memcpy(copy, text, len + 1);\n", out);
    xl6OutputPuts("    return copy;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static const char* xl6_error_kind(XL6_Error* error) { return xl6_error_text_copy(error != 0 ? error->kind : 0); }\n", out);
    xl6OutputPuts("static const char* xl6_error_message(XL6_Error* error) { return xl6_error_text_copy(error != 0 ? error->message : 0); }\n", out);
    xl6OutputPuts("static const char* xl6_error_file(XL6_Error* error) { return xl6_error_text_copy(error != 0 ? error->file : 0); }\n", out);
    xl6OutputPuts("static int xl6_error_line(XL6_Error* error) { return error != 0 ? error->line : 0; }\n", out);
    xl6OutputPuts("static int xl6_error_column(XL6_Error* error) { return error != 0 ? error->column : 0; }\n\n", out);

    xl6OutputPuts("static const char* xl6_error_parent_kind(const char* kind)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( kind == 0 || kind[0] == 0 || strcmp(kind, \"Error\") == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"AssertionError\") == 0 || strcmp(kind, \"NullReferenceError\") == 0 || strcmp(kind, \"NotImplementedError\") == 0 || strcmp(kind, \"NativeError\") == 0 ) { return \"RuntimeError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"RangeError\") == 0 ) { return \"ValueError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"IndexError\") == 0 || strcmp(kind, \"KeyError\") == 0 || strcmp(kind, \"AttributeError\") == 0 || strcmp(kind, \"NameError\") == 0 ) { return \"LookupError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"FileError\") == 0 || strcmp(kind, \"EncodingError\") == 0 || strcmp(kind, \"NetworkError\") == 0 || strcmp(kind, \"TimeoutError\") == 0 ) { return \"IOError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"CompressionError\") == 0 || strcmp(kind, \"ArchiveError\") == 0 ) { return \"IOError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"ModuleNotFoundError\") == 0 ) { return \"ImportError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"FutureError\") == 0 || strcmp(kind, \"TaskError\") == 0 || strcmp(kind, \"CancelledError\") == 0 || strcmp(kind, \"GeneratorError\") == 0 ) { return \"AsyncError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"SyntaxError\") == 0 ) { return \"ParseError\"; }\n", out);
    xl6OutputPuts("    if ( strcmp(kind, \"CsvError\") == 0 || strcmp(kind, \"ConfigError\") == 0 || strcmp(kind, \"TomlError\") == 0 || strcmp(kind, \"PlistError\") == 0 || strcmp(kind, \"XmlError\") == 0 ) { return \"ParseError\"; }\n", out);
    xl6OutputPuts("    return \"Error\";\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_error_is(XL6_Error* error, const char* target)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* kind = error != 0 ? error->kind : 0;\n", out);
    xl6OutputPuts("    if ( target == 0 || target[0] == 0 ) { return 1; }\n", out);
    xl6OutputPuts("    while ( kind != 0 && kind[0] != 0 ) {\n", out);
    xl6OutputPuts("        if ( strcmp(kind, target) == 0 ) { return 1; }\n", out);
    xl6OutputPuts("        kind = xl6_error_parent_kind(kind);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_error_print_uncaught(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Error* error = xl6_error_active_get();\n", out);
    xl6OutputPuts("    const char* kind;\n", out);
    xl6OutputPuts("    const char* message;\n", out);
    xl6OutputPuts("    const char* file;\n", out);
    xl6OutputPuts("    int line;\n", out);
    xl6OutputPuts("    int column;\n\n", out);
    xl6OutputPuts("    if ( error == 0 ) {\n", out);
    xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("    }\n", out);
    /* 未捕获错误会在打印完成后才释放，直接读取拥有字段即可，避免额外临时内存。 */
    xl6OutputPuts("    kind = error->kind;\n", out);
    xl6OutputPuts("    message = error->message;\n", out);
    xl6OutputPuts("    file = error->file;\n", out);
    xl6OutputPuts("    line = xl6_error_line(error);\n", out);
    xl6OutputPuts("    column = xl6_error_column(error);\n", out);
    xl6OutputPuts("    if ( kind == 0 || kind[0] == 0 ) {\n", out);
    xl6OutputPuts("        kind = \"Error\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( message == 0 ) {\n", out);
    xl6OutputPuts("        message = \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( file != 0 && file[0] != 0 && line > 0 && column > 0 ) {\n", out);
    xl6OutputPuts("        f", out);
    xl6OutputPuts("pri", out);
    xl6OutputPuts("ntf(st", out);
    xl6OutputPuts("derr, \"%s:%d:%d: uncaught %s: %s\\n\", file, line, column, kind, message);\n", out);
    xl6OutputPuts("    } else if ( line > 0 && column > 0 ) {\n", out);
    xl6OutputPuts("        f", out);
    xl6OutputPuts("pri", out);
    xl6OutputPuts("ntf(st", out);
    xl6OutputPuts("derr, \"%d:%d: uncaught %s: %s\\n\", line, column, kind, message);\n", out);
    xl6OutputPuts("    } else {\n", out);
    xl6OutputPuts("        f", out);
    xl6OutputPuts("pri", out);
    xl6OutputPuts("ntf(st", out);
    xl6OutputPuts("derr, \"uncaught %s: %s\\n\", kind, message);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);
}

static void xl6CgenEmitDefaultErrorReturn(XL6_CgenContext* ctx)
{
    int returnId;

    if ( ctx->currentReturns == NULL || ctx->currentReturns->childCount == 0 ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("return;\n", ctx->out);
        return;
    }

    returnId = ctx->tempId++;
    xl6CgenIndent(ctx);
    if ( ctx->currentReturns->childCount > 1 ) {
        if ( ctx->currentReturnFunc != NULL ) {
            xl6CgenFuncReturnStructName(ctx, ctx->currentReturnFunc);
        } else {
            xl6CgenFunctionReturnStructName(ctx, ctx->currentReturnParams, ctx->currentReturns);
        }
    } else {
        const XL6_AstNode* returnType = ctx->currentReturns->children[0];
        if ( xl6CgenIsTypedContainerType(ctx, returnType) ) {
            xl6OutputPuts(xl6CgenTypedContainerCType(ctx, returnType), ctx->out);
        } else {
            xl6OutputPuts(xl6CgenTypeName(ctx, returnType), ctx->out);
        }
    }
    xl6OutputPrintf(ctx->out, " __xl6_error_return_%d;\n", returnId);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
                    "memset(&__xl6_error_return_%d, 0, sizeof(__xl6_error_return_%d));\n",
                    returnId,
                    returnId);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "return __xl6_error_return_%d;\n", returnId);
}

static void xl6CgenEmitExceptionPropagate(XL6_CgenContext* ctx, const XL6_AstNode* at)
{
    XL6_CgenTryTarget* target;
    XL6_CgenFinallyTarget* finallyTarget;

    (void)at;
    if ( ctx == NULL || !ctx->needsErrorRuntime ) {
        return;
    }

    target = xl6CgenCurrentTryTarget(ctx);
    finallyTarget = xl6CgenCurrentFinallyTarget(ctx);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( xl6_error_has() ) {\n", ctx->out);
    ctx->indent += 1;
    if ( finallyTarget != NULL && finallyTarget->inFinally ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (__xl6_pending_kind_%d == 1) {\n", finallyTarget->id);
        ctx->indent += 1;
        xl6CgenDiscardPendingFinallyReturn(ctx, finallyTarget->id);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_pending_kind_%d = 0;\n", finallyTarget->id);
    }
    if ( xl6CgenEmitCurrentErrorScopeJump(ctx) ) {
        ;
    } else if ( target != NULL ) {
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
    const XL6_AstNode* kindNode;
    const XL6_AstNode* expr;
    XL6_CgenTaskSpawnArgTempScope taskScope;
    int hasTaskScope;

    kindNode = node != NULL && node->childCount == 2 ? node->children[0] : NULL;
    expr = node != NULL && node->childCount > 0 ? node->children[node->childCount == 2 ? 1 : 0] : NULL;
    hasTaskScope = expr != NULL && xl6CgenTaskSpawnArgTempScopeBegin(ctx, expr, &taskScope);
    xl6CgenIndent(ctx);
    if ( expr == NULL ) {
        if ( ctx->currentCatchErrorName != NULL ) {
            xl6OutputPrintf(ctx->out, "xl6_error_reraise(%s);\n", ctx->currentCatchErrorName);
        } else {
            xl6OutputPuts("xl6_error_raise(\"Error\", \"rethrow without active catch\", \"\", 0, 0);\n", ctx->out);
        }
    } else {
        xl6OutputPrintf(ctx->out, "xl6_error_raise(\"%s\", ", kindNode != NULL && kindNode->text != NULL ? kindNode->text : "Error");
        xl6CgenExpr(ctx, expr);
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d);\n", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
    }
    if ( hasTaskScope ) {
        xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
    }
    xl6CgenEmitExceptionPropagate(ctx, node);
}

static void xl6CgenAssertStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* cond;
    const XL6_AstNode* message;
    char conditionName[64];
    int hasConditionTemp;
    int hasConditionTaskScope;
    int hasMessageTaskScope;
    XL6_CgenTaskSpawnArgTempScope conditionTaskScope;
    XL6_CgenTaskSpawnArgTempScope messageTaskScope;

    cond = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    message = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    conditionName[0] = '\0';
    hasConditionTemp = cond != NULL &&
                       xl6CgenTryConditionTemp(ctx, cond, "__xl6_assert_cond_", conditionName, sizeof(conditionName));
    hasConditionTaskScope = 0;
    hasMessageTaskScope = 0;
    if ( cond != NULL && !hasConditionTemp ) {
        hasConditionTaskScope = xl6CgenTaskSpawnArgTempScopeBegin(ctx, cond, &conditionTaskScope);
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( !(", ctx->out);
    if ( hasConditionTemp ) {
        xl6OutputPuts(conditionName, ctx->out);
    } else {
        xl6CgenConditionExpr(ctx, cond);
    }
    xl6OutputPuts(") ) {\n", ctx->out);
    if ( hasConditionTaskScope ) {
        xl6CgenTaskSpawnArgTempScopeEnd(ctx, &conditionTaskScope);
    }
    ctx->indent += 1;
    if ( message != NULL ) {
        hasMessageTaskScope = xl6CgenTaskSpawnArgTempScopeBegin(ctx, message, &messageTaskScope);
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts("xl6_error_raise(\"AssertionError\", ", ctx->out);
    if ( message != NULL ) {
        xl6CgenExpr(ctx, message);
    } else {
        xl6OutputPuts("\"assert failed\"", ctx->out);
    }
    xl6OutputPrintf(ctx->out, ", \"\", %d, %d);\n", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
    if ( hasMessageTaskScope ) {
        xl6CgenTaskSpawnArgTempScopeEnd(ctx, &messageTaskScope);
    }
    xl6CgenEmitExceptionPropagate(ctx, node);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static int xl6CgenTryHasCatch(const XL6_AstNode* node)
{
    const XL6_AstNode* catchList = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    return catchList != NULL && catchList->kind == XL6_AST_LIST && catchList->childCount > 0;
}

static int xl6CgenTryHasFinally(const XL6_AstNode* node)
{
    const XL6_AstNode* finallyBlock = node != NULL && node->childCount > 2 ? node->children[2] : NULL;
    return finallyBlock != NULL && finallyBlock->childCount > 0;
}

static int xl6CgenFunctionHasReturnValue(XL6_CgenContext* ctx)
{
    return ctx != NULL && ctx->currentReturns != NULL && ctx->currentReturns->childCount > 0;
}

static void xl6CgenPendingReturnType(XL6_CgenContext* ctx)
{
    if ( ctx->currentReturns != NULL && ctx->currentReturns->childCount > 1 ) {
        if ( ctx->currentReturnFunc != NULL ) {
            xl6CgenFuncReturnStructName(ctx, ctx->currentReturnFunc);
        } else {
            xl6CgenFunctionReturnStructName(ctx, ctx->currentReturnParams, ctx->currentReturns);
        }
    } else {
        xl6OutputPuts(xl6CgenTypeName(ctx, ctx->currentReturnType), ctx->out);
    }
}

static void xl6CgenDeclarePendingFinallyControl(XL6_CgenContext* ctx, int targetId)
{
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "int __xl6_pending_kind_%d = 0;\n", targetId);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "int __xl6_pending_target_%d = 0;\n", targetId);
    if ( xl6CgenFunctionHasReturnValue(ctx) ) {
        xl6CgenIndent(ctx);
        xl6CgenPendingReturnType(ctx);
        xl6OutputPrintf(ctx->out, " __xl6_pending_return_%d;\n", targetId);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "memset(&__xl6_pending_return_%d, 0, sizeof(__xl6_pending_return_%d));\n", targetId, targetId);
    }
}

static void xl6CgenDiscardPendingFinallyReturn(XL6_CgenContext* ctx, int targetId)
{
    char name[96];

    if ( !xl6CgenFunctionHasReturnValue(ctx) ) {
        return;
    }
    snprintf(name, sizeof(name), "__xl6_pending_return_%d", targetId);
    if ( ctx->currentReturns->childCount > 1 ) {
        xl6CgenDiscardTemporaryMultiResultByTypes(ctx, ctx->currentReturns, name);
    } else {
        xl6CgenDiscardTemporaryResultByType(ctx, ctx->currentReturnType, name);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "memset(&%s, 0, sizeof(%s));\n", name, name);
}

static int xl6CgenEmitPendingFinallyReturn(
    XL6_CgenContext* ctx,
    const char* returnTempName,
    const char* movedCleanupName)
{
    XL6_CgenFinallyTarget* target;

    target = xl6CgenCurrentFinallyTarget(ctx);
    if ( target == NULL ) {
        return 0;
    }
    xl6CgenEmitCleanupsSinceExcept(
        ctx,
        target->cleanupMark,
        movedCleanupName != NULL && movedCleanupName[0] != '\0' ? movedCleanupName : NULL);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_pending_kind_%d == 1) {\n", target->id);
    ctx->indent += 1;
    xl6CgenDiscardPendingFinallyReturn(ctx, target->id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(
        ctx->out,
        "__xl6_pending_return_%d = %s;\n",
        target->id,
        returnTempName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_pending_kind_%d = 1;\n", target->id);
    xl6CgenIndent(ctx);
    if ( target->inFinally ) {
        xl6OutputPrintf(ctx->out, "goto __xl6_try_finally_dispatch_%d;\n", target->id);
    } else {
        xl6OutputPrintf(ctx->out, "goto __xl6_try_finally_%d;\n", target->id);
    }
    return 1;
}

static int xl6CgenEmitPendingFinallyVoidReturn(XL6_CgenContext* ctx)
{
    XL6_CgenFinallyTarget* target = xl6CgenCurrentFinallyTarget(ctx);

    if ( target == NULL ) {
        return 0;
    }
    xl6CgenEmitCleanupsSince(ctx, target->cleanupMark);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_pending_kind_%d == 1) {\n", target->id);
    ctx->indent += 1;
    xl6CgenDiscardPendingFinallyReturn(ctx, target->id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_pending_kind_%d = 1;\n", target->id);
    xl6CgenIndent(ctx);
    if ( target->inFinally ) {
        xl6OutputPrintf(ctx->out, "goto __xl6_try_finally_dispatch_%d;\n", target->id);
    } else {
        xl6OutputPrintf(ctx->out, "goto __xl6_try_finally_%d;\n", target->id);
    }
    return 1;
}

static void xl6CgenGotoPendingFinallyTarget(XL6_CgenContext* ctx, const XL6_CgenFinallyTarget* target)
{
    if ( target->inFinally ) {
        xl6OutputPrintf(ctx->out, "goto __xl6_try_finally_dispatch_%d;\n", target->id);
    } else {
        xl6OutputPrintf(ctx->out, "goto __xl6_try_finally_%d;\n", target->id);
    }
}

static void xl6CgenEmitPendingFinallyReturnDispatch(
    XL6_CgenContext* ctx,
    const XL6_CgenFinallyTarget* target)
{
    XL6_CgenFinallyTarget* parent = xl6CgenCurrentFinallyTarget(ctx);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_pending_kind_%d == 1) {\n", target->id);
    ctx->indent += 1;
    if ( parent != NULL ) {
        if ( xl6CgenFunctionHasReturnValue(ctx) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (__xl6_pending_kind_%d == 1) {\n", parent->id);
            ctx->indent += 1;
            xl6CgenDiscardPendingFinallyReturn(ctx, parent->id);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "__xl6_pending_return_%d = __xl6_pending_return_%d;\n",
                parent->id,
                target->id);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "memset(&__xl6_pending_return_%d, 0, sizeof(__xl6_pending_return_%d));\n",
                target->id,
                target->id);
        }
        xl6CgenEmitCleanupsSince(ctx, parent->cleanupMark);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_pending_kind_%d = 1;\n", parent->id);
        xl6CgenIndent(ctx);
        xl6CgenGotoPendingFinallyTarget(ctx, parent);
    } else {
        if ( ctx->currentAsyncWorker ) {
            xl6CgenAsyncPendingResultCompletion(ctx, target->id);
        } else if ( !xl6CgenFunctionHasReturnValue(ctx) ) {
            xl6CgenEmitCleanupsSince(ctx, 0);
            xl6CgenIndent(ctx);
            xl6OutputPuts("return;\n", ctx->out);
        } else if ( ctx->currentReturns->childCount > 1 ) {
            xl6CgenEmitCleanupsSince(ctx, 0);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "return __xl6_pending_return_%d;\n", target->id);
        } else if ( xl6CgenIsStringTypeNode(ctx, ctx->currentReturnType) ) {
            xl6CgenEmitCleanupsSince(ctx, 0);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "return xrtTempStringTake((char*)__xl6_pending_return_%d, 0u);\n", target->id);
        } else {
            xl6CgenEmitCleanupsSince(ctx, 0);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "return __xl6_pending_return_%d;\n", target->id);
        }
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenEmitPendingFinallyLoopDispatch(
    XL6_CgenContext* ctx,
    const XL6_CgenFinallyTarget* target,
    int kind)
{
    XL6_CgenFinallyTarget* parent = xl6CgenCurrentFinallyTarget(ctx);
    size_t i;

    for ( i = 0; i < target->loopTargetCount && i < ctx->loopTargetCount; ++i ) {
        XL6_CgenLoopTarget* loop = &ctx->loopTargets[i];
        int labelId = kind == 3 ? loop->continueId : loop->breakId;

        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "if (__xl6_pending_kind_%d == %d && __xl6_pending_target_%d == %d) {\n",
            target->id,
            kind,
            target->id,
            labelId);
        ctx->indent += 1;
        if ( parent != NULL && i < parent->loopTargetCount ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (__xl6_pending_kind_%d == 1) {\n", parent->id);
            ctx->indent += 1;
            xl6CgenDiscardPendingFinallyReturn(ctx, parent->id);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenEmitCleanupsSince(ctx, parent->cleanupMark);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "__xl6_pending_kind_%d = %d;\n", parent->id, kind);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "__xl6_pending_target_%d = %d;\n", parent->id, labelId);
            xl6CgenIndent(ctx);
            xl6CgenGotoPendingFinallyTarget(ctx, parent);
        } else {
            xl6CgenEmitCleanupsSince(
                ctx,
                kind == 3 ? loop->continueCleanupMark : loop->breakCleanupMark);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "goto __xl6_%s_%d;\n", kind == 3 ? "continue" : "break", labelId);
        }
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    }
}

static void xl6CgenTryStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* tryBlock;
    const XL6_AstNode* catchList;
    const XL6_AstNode* finallyBlock;
    char caughtName[64];
    char catchLabel[64];
    char finallyLabel[64];
    char endLabel[64];
    int id;
    int hasCatch;
    int hasFinally;
    int hasFinallyTarget;
    XL6_CgenFinallyTarget finallyTarget;
    size_t tryCleanupMark;

    tryBlock = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    catchList = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    finallyBlock = node != NULL && node->childCount > 2 ? node->children[2] : NULL;
    hasCatch = xl6CgenTryHasCatch(node);
    hasFinally = xl6CgenTryHasFinally(node);
    id = ctx->tempId++;

    snprintf(catchLabel, sizeof(catchLabel), "__xl6_try_catch_%d", id);
    snprintf(finallyLabel, sizeof(finallyLabel), "__xl6_try_finally_%d", id);
    snprintf(endLabel, sizeof(endLabel), "__xl6_try_end_%d", id);
    snprintf(caughtName, sizeof(caughtName), "__xl6_caught_error_%d", id);

    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;

    tryCleanupMark = ctx->cleanupCount;
    hasFinallyTarget = 0;
    memset(&finallyTarget, 0, sizeof(finallyTarget));
    if ( hasFinally ) {
        xl6CgenDeclarePendingFinallyControl(ctx, id);
        hasFinallyTarget = xl6CgenPushFinallyTarget(ctx, tryCleanupMark, id);
    }
    xl6CgenPushTryTarget(ctx, tryCleanupMark, id, hasCatch ? 0 : 1);
    xl6CgenBlock(ctx, tryBlock);
    xl6CgenPopTryTarget(ctx);
    xl6CgenEmitGotoIfError(ctx, hasCatch ? catchLabel : finallyLabel);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "goto %s;\n", hasFinally ? finallyLabel : endLabel);

    if ( hasCatch ) {
        size_t catchIndex;

        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s:\n", catchLabel);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "XL6_Error* %s = xl6_error_take();\n", caughtName);

        for ( catchIndex = 0; catchList != NULL && catchIndex < catchList->childCount; ++catchIndex ) {
            const XL6_AstNode* catchNode = catchList->children[catchIndex];
            const XL6_AstNode* catchKind = NULL;
            const XL6_AstNode* catchName = NULL;
            const XL6_AstNode* catchBlock = NULL;
            const XL6_AstNode* errorType;
            const char* prevCatchErrorName;
            char catchCName[512];
            size_t catchCleanupMark;
            size_t catchVarMark;

            if ( catchNode == NULL || catchNode->kind != XL6_AST_CATCH ) {
                continue;
            }
            if ( catchNode->childCount == 3 ) {
                catchKind = catchNode->children[0];
                catchName = catchNode->children[1];
                catchBlock = catchNode->children[2];
            } else {
                catchName = catchNode->childCount > 0 ? catchNode->children[0] : NULL;
                catchBlock = catchNode->childCount > 1 ? catchNode->children[1] : NULL;
            }
            xl6CgenIndent(ctx);
            if ( catchKind != NULL && catchKind->text != NULL ) {
                xl6OutputPrintf(ctx->out, "if (%s != 0 && xl6_error_is(%s, \"%s\")) {\n", caughtName, caughtName, catchKind->text);
            } else {
                xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", caughtName);
            }
            ctx->indent += 1;
            xl6CgenCIdentText(catchCName, sizeof(catchCName), catchName != NULL ? catchName->text : "__xl6_error");
            catchVarMark = ctx->varCount;
            catchCleanupMark = ctx->cleanupCount;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "XL6_Error* %s = %s;\n", catchCName, caughtName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = 0;\n", caughtName);
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
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        }

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", caughtName);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xl6_error_set_active(%s);\n", caughtName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", caughtName);
        if ( hasFinally ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "goto %s;\n", finallyLabel);
        } else {
            xl6CgenEmitExceptionPropagate(ctx, node);
        }
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "goto %s;\n", hasFinally ? finallyLabel : endLabel);
    }

    if ( hasFinally ) {
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s:\n", finallyLabel);
        ctx->indent += 1;
        if ( hasFinallyTarget && xl6CgenCurrentFinallyTarget(ctx) != NULL ) {
            xl6CgenCurrentFinallyTarget(ctx)->inFinally = 1;
        }
        xl6CgenBlock(ctx, finallyBlock);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (xl6_error_has() && __xl6_pending_kind_%d == 1) {\n", id);
        ctx->indent += 1;
        xl6CgenDiscardPendingFinallyReturn(ctx, id);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenEmitExceptionPropagate(ctx, node);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_try_finally_dispatch_%d:\n", id);
        ctx->indent += 1;
        if ( hasFinallyTarget && xl6CgenCurrentFinallyTarget(ctx) != NULL ) {
            finallyTarget = *xl6CgenCurrentFinallyTarget(ctx);
            xl6CgenPopFinallyTarget(ctx);
            hasFinallyTarget = 0;
            xl6CgenEmitPendingFinallyReturnDispatch(ctx, &finallyTarget);
            xl6CgenEmitPendingFinallyLoopDispatch(ctx, &finallyTarget, 2);
            xl6CgenEmitPendingFinallyLoopDispatch(ctx, &finallyTarget, 3);
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "goto %s;\n", endLabel);
    }

    if ( hasFinallyTarget ) {
        xl6CgenPopFinallyTarget(ctx);
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
