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

static const XL6_AstNode* xl6CgenAsyncSingleResultType(const XL6_AstNode* node)
{
    const XL6_AstNode* returns;

    returns = node != NULL && node->childCount > 2 ? node->children[2] : NULL;
    return returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
}

static void xl6CgenAwaitClassReleaseHelperSymbol(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    xl6OutputPuts("xl6_future_await_class_", ctx->out);
    xl6CgenFuncSymbol(ctx, funcNode);
    xl6OutputPuts("_release", ctx->out);
}

static void xl6CgenAsyncClassAwaitReleaseHelper(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* resultType;
    const char* cType;
    int prevIndent;

    resultType = xl6CgenAsyncSingleResultType(node);
    if ( !xl6CgenIsClassType(ctx, resultType) ) {
        return;
    }

    cType = xl6CgenTypeName(ctx, resultType);
    xl6OutputPuts("static ", ctx->out);
    xl6OutputPuts(cType, ctx->out);
    xl6OutputPutc(' ', ctx->out);
    xl6CgenAwaitClassReleaseHelperSymbol(ctx, node);
    xl6OutputPuts("(xfuture* pFuture, const char* file, int line, int column)\n{\n", ctx->out);
    xl6OutputPuts("    ", ctx->out);
    xl6OutputPuts(cType, ctx->out);
    xl6OutputPuts(" result;\n", ctx->out);
    xl6OutputPuts("    void* value;\n", ctx->out);
    xl6OutputPuts("    memset(&result, 0, sizeof(result));\n", ctx->out);
    xl6OutputPuts("    value = xl6_future_await_value(pFuture, file, line, column, 1, sizeof(result));\n", ctx->out);
    xl6OutputPuts("    if ( value != 0 ) {\n", ctx->out);
    xl6OutputPuts("        result = *(", ctx->out);
    xl6OutputPuts(cType, ctx->out);
    xl6OutputPuts("*)value;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    if ( !xl6_error_has() ) {\n", ctx->out);
    prevIndent = ctx->indent;
    ctx->indent = 2;
    xl6CgenRetainClassFunctionFieldsForType(ctx, resultType, "result");
    ctx->indent = prevIndent;
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    if ( xl6_error_has() && value != 0 ) {\n", ctx->out);
    xl6OutputPuts("        xrtFree(value);\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    xl6_future_release_if_needed(pFuture);\n", ctx->out);
    xl6OutputPuts("    return result;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
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

static int xl6CgenAwaitOperandIsOwnedTemporary(const XL6_AstNode* node)
{
    const XL6_AstNode* operand;

    if ( node == NULL || node->childCount == 0 ) {
        return 0;
    }
    operand = node->children[0];

    /*
        直接 async 调用会返回新的 future 引用，如果不绑定到变量，就没有
        scope cleanup 负责释放；具名变量、成员、索引等路径按借用处理，
        继续允许同一个 future 被多次 await。
    */
    return operand != NULL && operand->kind == XL6_AST_CALL;
}

static void xl6CgenAwaitOperandExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    if ( node != NULL && node->childCount > 0 ) {
        xl6CgenExpr(ctx, node->children[0]);
    } else {
        xl6OutputPuts("0", ctx->out);
    }
}

static XL6_AstNode* xl6CgenAwaitOwnedTemporaryAsyncFunc(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* operand;
    const XL6_AstNode* callee;
    XL6_AstNode* func;

    if ( ctx == NULL || node == NULL || node->childCount == 0 ) {
        return NULL;
    }
    operand = node->children[0];
    if ( operand == NULL || operand->kind != XL6_AST_CALL || operand->childCount == 0 ) {
        return NULL;
    }
    callee = operand->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return NULL;
    }

    func = xl6CgenResolveCallFunc(ctx, operand, callee->text);
    return func != NULL && func->kind == XL6_AST_ASYNC_FUNC ? func : NULL;
}

static const char* xl6CgenAwaitReleaseHelperName(XL6_CgenContext* ctx, const XL6_AstNode* resultType, int requireValue)
{
    const char* cType;

    if ( !requireValue ) {
        return "xl6_future_await_void_release";
    }
    if ( xl6CgenIsFunctionType(resultType) ) {
        return "xl6_future_await_function_release";
    }
    if ( resultType == NULL || xl6CgenIsXValueType(resultType) || xl6CgenIsClassType(ctx, resultType) ) {
        return NULL;
    }

    cType = xl6CgenTypeName(ctx, resultType);
    if ( xl6CTextEquals(cType, "int") ) {
        return "xl6_future_await_i32_release";
    }
    if ( xl6CTextEquals(cType, "long long") ) {
        return "xl6_future_await_i64_release";
    }
    if ( xl6CTextEquals(cType, "unsigned long long") ) {
        return "xl6_future_await_u64_release";
    }
    if ( xl6CTextEquals(cType, "float") ) {
        return "xl6_future_await_f32_release";
    }
    if ( xl6CTextEquals(cType, "double") ) {
        return "xl6_future_await_f64_release";
    }
    if ( xl6CTextEquals(cType, "const char*") ) {
        return "xl6_future_await_cstr_release";
    }
    if ( xl6CTextEquals(cType, "void*") ) {
        return "xl6_future_await_ptr_release";
    }
    return NULL;
}

static int xl6CgenAwaitOwnedTemporaryHelperExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* resultType,
    int requireValue)
{
    const char* helperName;

    if ( requireValue && xl6CgenIsClassType(ctx, resultType) ) {
        XL6_AstNode* asyncFunc = xl6CgenAwaitOwnedTemporaryAsyncFunc(ctx, node);
        if ( asyncFunc != NULL && xl6CgenIsClassType(ctx, xl6CgenAsyncSingleResultType(asyncFunc)) ) {
            xl6CgenAwaitClassReleaseHelperSymbol(ctx, asyncFunc);
            xl6OutputPutc('(', ctx->out);
            xl6CgenAwaitOperandExpr(ctx, node);
            xl6OutputPrintf(ctx->out, ", \"\", %d, %d)", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
            return 1;
        }
    }

    helperName = xl6CgenAwaitReleaseHelperName(ctx, resultType, requireValue);
    if ( helperName == NULL ) {
        return 0;
    }

    xl6OutputPuts(helperName, ctx->out);
    xl6OutputPutc('(', ctx->out);
    xl6CgenAwaitOperandExpr(ctx, node);
    xl6OutputPrintf(ctx->out, ", \"\", %d, %d)", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
    return 1;
}

static void xl6CgenAwaitOwnedTemporaryExpr(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* resultType, int requireValue)
{
    int id = ctx->tempId++;
    char futureName[64];
    char valueName[64];

    snprintf(futureName, sizeof(futureName), "__xl6_await_future_%d", id);
    snprintf(valueName, sizeof(valueName), "__xl6_await_value_%d", id);

    xl6OutputPuts("({ xfuture* ", ctx->out);
    xl6OutputPuts(futureName, ctx->out);
    xl6OutputPuts(" = ", ctx->out);
    xl6CgenAwaitOperandExpr(ctx, node);
    xl6OutputPuts(";\n", ctx->out);

    if ( !requireValue ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("xl6_future_await_value(", ctx->out);
        xl6OutputPuts(futureName, ctx->out);
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d, 0, 0);\n", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPuts(xl6CgenTypeName(ctx, resultType), ctx->out);
        xl6OutputPutc(' ', ctx->out);
        xl6OutputPuts(valueName, ctx->out);
        xl6OutputPuts(" = (*(", ctx->out);
        xl6OutputPuts(xl6CgenTypeName(ctx, resultType), ctx->out);
        xl6OutputPuts("*)xl6_future_await_value(", ctx->out);
        xl6OutputPuts(futureName, ctx->out);
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d, 1, sizeof(", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
        xl6OutputPuts(xl6CgenTypeName(ctx, resultType), ctx->out);
        xl6OutputPuts(")));\n", ctx->out);
        if ( xl6CgenIsFunctionType(resultType) ) {
            xl6CgenRetainFunctionValue(ctx, valueName);
        } else {
            xl6CgenRetainClassFunctionFieldsForType(ctx, resultType, valueName);
        }
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("if (", ctx->out);
    xl6OutputPuts(futureName, ctx->out);
    xl6OutputPuts(" != 0) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("xFutureRelease(", ctx->out);
    xl6OutputPuts(futureName, ctx->out);
    xl6OutputPuts(");\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);

    xl6CgenIndent(ctx);
    if ( requireValue ) {
        xl6OutputPuts(valueName, ctx->out);
    } else {
        xl6OutputPuts("0", ctx->out);
    }
    xl6OutputPuts("; })", ctx->out);
}

static void xl6CgenAwaitExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* resultType;
    int requireValue;

    resultType = xl6CgenCloneAwaitResultType(ctx, node);
    requireValue = resultType != NULL && !xl6CTextEquals(resultType->text, "null");

    if ( xl6CgenAwaitOperandIsOwnedTemporary(node) ) {
        if ( !xl6CgenAwaitOwnedTemporaryHelperExpr(ctx, node, resultType, requireValue) ) {
            xl6CgenAwaitOwnedTemporaryExpr(ctx, node, resultType, requireValue);
        }
        xl6AstFree(resultType);
        return;
    }

    if ( !requireValue ) {
        xl6OutputPuts("(xl6_future_await_value(", ctx->out);
        xl6CgenAwaitOperandExpr(ctx, node);
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d, 0, 0), 0)", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
        xl6AstFree(resultType);
        return;
    }

    xl6OutputPuts("(*(", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, resultType), ctx->out);
    xl6OutputPuts("*)xl6_future_await_value(", ctx->out);
    xl6CgenAwaitOperandExpr(ctx, node);
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
        xl6OutputPuts("return xl6_async_require_future(xTaskRunThread(", ctx->out);
        xl6CgenAsyncWorkerSymbol(ctx, node);
        xl6OutputPrintf(ctx->out, ", 0, 0), \"\", %d, %d);\n", node->line, node->column);
    }
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}
