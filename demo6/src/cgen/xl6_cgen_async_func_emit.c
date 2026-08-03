static void xl6CgenAsyncWorker(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* params = xl6CgenAsyncParams(node);
    const XL6_AstNode* body = xl6CgenAsyncBody(node);
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevReturnParams;
    const XL6_AstNode* prevReturns;
    const XL6_AstNode* prevReturnFunc;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* prevNativeClass;
    const char* prevErrorExitLabel;
    int prevAsyncWorker;
    int hasFrame;
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
    prevClass = ctx->currentClass;
    prevNativeClass = ctx->currentNativeClass;
    prevErrorExitLabel = ctx->currentErrorExitLabel;
    prevAsyncWorker = ctx->currentAsyncWorker;

    hasFrame = xl6CgenAsyncIsClassMethod(ctx) ||
               (params != NULL && params->childCount > 0);
    if ( hasFrame ) {
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
        if ( xl6CgenAsyncIsClassMethod(ctx) ) {
            xl6CgenIndent(ctx);
            xl6OutputPuts(
                "xvalue __xl6_self_handle = __xl6_args->__xl6_self;\n",
                ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPuts("__xl6_args->__xl6_self = 0;\n", ctx->out);
            xl6CgenIndent(ctx);
            if ( ctx->currentAsyncNativeClass != NULL ) {
                xl6CgenNativeBackedTypeName(
                    ctx->out,
                    ctx->currentAsyncNativeClass->text);
                xl6OutputPuts("* self = (", ctx->out);
                xl6CgenNativeBackedTypeName(
                    ctx->out,
                    ctx->currentAsyncNativeClass->text);
                xl6OutputPuts(
                    "*)xvoGetHandleSlot(__xl6_self_handle);\n",
                    ctx->out);
            } else {
                if ( ctx->currentAsyncClassType != NULL ) {
                    xl6OutputPuts(
                        xl6CgenTypeName(ctx, ctx->currentAsyncClassType),
                        ctx->out);
                } else {
                    xl6CgenClassTypeName(
                        ctx->out,
                        ctx->currentAsyncClass->text);
                }
                xl6OutputPuts("* self = (", ctx->out);
                if ( ctx->currentAsyncClassType != NULL ) {
                    xl6OutputPuts(
                        xl6CgenTypeName(ctx, ctx->currentAsyncClassType),
                        ctx->out);
                } else {
                    xl6CgenClassTypeName(
                        ctx->out,
                        ctx->currentAsyncClass->text);
                }
                if ( ctx->currentAsyncClass != NULL &&
                     xl6CgenClassIsPureNativeHandle(
                         ctx->currentAsyncClass) ) {
                    xl6OutputPuts(
                        "*)xvoGetHandleSlot(__xl6_self_handle);\n",
                        ctx->out);
                } else {
                    xl6OutputPuts(
                        "*)xvoGetRecordData(__xl6_self_handle);\n",
                        ctx->out);
                }
            }
            xl6CgenIndent(ctx);
            xl6OutputPuts(
                "if (self == 0) { "
                "xvoUnref(__xl6_self_handle); "
                "xrtFree(__xl6_args); "
                "return xl6_async_result_error("
                "pOut, \"missing async class receiver\"); }\n",
                ctx->out);
            /*
                异步接收者帧固定保存 xvalue owner，不受公开 class
                的具体表示影响。这里直接登记 xvalue 清理，避免把
                class 声明节点误判成非 xvalue 而泄漏 owner。
            */
            xl6CgenAddCleanupEntry(
                ctx,
                "__xl6_self_handle",
                XL6_CGEN_CLEANUP_XVALUE,
                ctx->currentAsyncClassType != NULL
                    ? ctx->currentAsyncClassType
                    : (ctx->currentAsyncClass != NULL
                        ? ctx->currentAsyncClass
                        : ctx->currentAsyncNativeClass),
                NULL);
        }
        for ( i = 0; params != NULL && i < params->childCount; ++i ) {
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
    ctx->currentClass = ctx->currentAsyncClass;
    ctx->currentNativeClass = ctx->currentAsyncNativeClass;
    if ( ctx->currentAsyncClass != NULL ) {
        xl6CgenAddVarType(
            ctx,
            "self",
            ctx->currentAsyncClassType != NULL
                ? ctx->currentAsyncClassType
                : ctx->currentAsyncClass);
    } else if ( ctx->currentAsyncNativeClass != NULL ) {
        xl6CgenAddVarType(ctx, "self", ctx->currentAsyncNativeClass);
    }
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
    ctx->currentClass = prevClass;
    ctx->currentNativeClass = prevNativeClass;
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
    int hasFrame;
    size_t i;

    xl6CgenAsyncArgStructDecl(ctx, node);
    xl6CgenAsyncArgsFreeFunc(ctx, node);
    xl6CgenAsyncResultFreeFunc(ctx, node);
    xl6CgenAsyncClassAwaitReleaseHelper(ctx, node);
    xl6CgenAsyncWorker(ctx, node);

    xl6CgenAsyncFuncProto(ctx, node);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    hasFrame = xl6CgenAsyncIsClassMethod(ctx) ||
               (params != NULL && params->childCount > 0);
    if ( hasFrame ) {
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
        if ( xl6CgenAsyncIsClassMethod(ctx) ) {
            xl6CgenIndent(ctx);
            if ( ctx->currentAsyncNativeClass != NULL ||
                 (ctx->currentAsyncClass != NULL &&
                  xl6CgenClassIsPureNativeHandle(
                      ctx->currentAsyncClass)) ) {
                xl6OutputPuts(
                    "__xl6_args->__xl6_self = "
                    "xvoGetHandleOwner((const ptr*)self);\n",
                    ctx->out);
            } else {
                xl6OutputPuts(
                    "__xl6_args->__xl6_self = xvoGetRecordOwner(self);\n",
                    ctx->out);
            }
            xl6CgenIndent(ctx);
            xl6OutputPuts(
                "if (__xl6_args->__xl6_self != 0) { "
                "xvoAddRef(__xl6_args->__xl6_self); }\n",
                ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "if (__xl6_args->__xl6_self == 0) { "
                "xrtFree(__xl6_args); "
                "xl6_error_raise(\"AsyncError\", "
                "\"async class receiver has no owner\", \"\", "
                "%d, %d); return 0; }\n",
                node->line,
                node->column);
        }
        for ( i = 0; params != NULL && i < params->childCount; ++i ) {
            xl6CgenAsyncCopyArgToFrame(ctx, params->children[i]);
        }
        xl6CgenIndent(ctx);
        xl6OutputPuts("(void)xl6_error_active_get();\n", ctx->out);
        xl6CgenIndent(ctx);
    xl6OutputPuts("__xl6_future = xl6_task_run_thread_tracked(", ctx->out);
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
    xl6OutputPuts("return xl6_async_require_future(xl6_task_run_thread_tracked(", ctx->out);
        xl6CgenAsyncWorkerSymbol(ctx, node);
        xl6OutputPrintf(ctx->out, ", 0, 0), \"\", %d, %d);\n", node->line, node->column);
    }
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenAsyncClassFunc(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* classType,
    const XL6_AstNode* node)
{
    const XL6_AstNode* prevClass = ctx->currentAsyncClass;
    const XL6_AstNode* prevClassType = ctx->currentAsyncClassType;
    const XL6_AstNode* prevNativeClass = ctx->currentAsyncNativeClass;
    const XL6_AstNode* prevParams = ctx->currentGenericParams;
    const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
    int prevStatic = ctx->currentAsyncStatic;

    ctx->currentAsyncClass = classNode;
    ctx->currentAsyncClassType = classType;
    ctx->currentAsyncNativeClass = NULL;
    ctx->currentAsyncStatic = xl6CgenIsStaticMember(node);
    if ( classType != NULL ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = classType->childCount > 0
            ? classType->children[0]
            : NULL;
    }
    xl6CgenAsyncFunc(ctx, node);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    ctx->currentAsyncClass = prevClass;
    ctx->currentAsyncClassType = prevClassType;
    ctx->currentAsyncNativeClass = prevNativeClass;
    ctx->currentAsyncStatic = prevStatic;
}

static void xl6CgenAsyncNativeClassFunc(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* node)
{
    const XL6_AstNode* prevClass = ctx->currentAsyncClass;
    const XL6_AstNode* prevClassType = ctx->currentAsyncClassType;
    const XL6_AstNode* prevNativeClass = ctx->currentAsyncNativeClass;
    int prevStatic = ctx->currentAsyncStatic;

    ctx->currentAsyncClass = NULL;
    ctx->currentAsyncClassType = NULL;
    ctx->currentAsyncNativeClass = classNode;
    ctx->currentAsyncStatic = xl6CgenIsStaticMember(node);
    xl6CgenAsyncFunc(ctx, node);
    ctx->currentAsyncClass = prevClass;
    ctx->currentAsyncClassType = prevClassType;
    ctx->currentAsyncNativeClass = prevNativeClass;
    ctx->currentAsyncStatic = prevStatic;
}
