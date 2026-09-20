static void xl6CgenAsyncReturnStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* resultType = ctx->currentReturnType;
    int id = ctx->tempId++;
    const char* cType;
    char cTypeBuffer[256];
    char returnName[64];
    XL6_CgenClassExprTempScope returnArgScope;
    int hasReturnArgScope;

    if ( node == NULL || node->childCount == 0 || resultType == NULL ) {
        if ( xl6CgenEmitPendingFinallyVoidReturn(ctx) ) {
            return;
        }
        xl6CgenEmitCleanupsSince(ctx, 0);
        xl6CgenIndent(ctx);
        xl6OutputPuts("xl6_async_result_ok(pOut);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("return 0;\n", ctx->out);
        return;
    }

    snprintf(returnName, sizeof(returnName), "__xl6_async_return_%d", id);
    cType = xl6CgenIsTypedContainerType(ctx, resultType) ?
            xl6CgenTypedContainerCType(ctx, resultType) :
            xl6CgenTypeName(ctx, resultType);
    snprintf(cTypeBuffer, sizeof(cTypeBuffer), "%s", cType != NULL ? cType : "int");
    cType = cTypeBuffer;
    hasReturnArgScope = xl6CgenClassExprTempScopeBegin(ctx, node->children[0], &returnArgScope);

    {
        XL6_AstNode* returnClass = xl6CgenClassNodeForType(ctx, resultType);
        XL6_AstNode* ctorClass = NULL;
        XL6_AstNode* genericCtorType = NULL;
        int isClassCtor = xl6CgenCallIsClassCtor(ctx, node->children[0], &ctorClass);
        int emittedCtor;
        int emittedHandleCtor;

        if ( !isClassCtor ) {
            genericCtorType = xl6CgenGenericClassCtorType(ctx, node->children[0], &ctorClass);
            isClassCtor = genericCtorType != NULL;
        }
        emittedCtor = returnClass != NULL &&
                      isClassCtor &&
                      ctorClass != NULL &&
                      xl6CTextEquals(returnClass->text, ctorClass->text);
        emittedHandleCtor = emittedCtor &&
                            xl6CgenClassHandleTargetType(ctx, resultType) != NULL;
        xl6CgenIndent(ctx);
        xl6OutputPuts(cType, ctx->out);
        if ( emittedHandleCtor ) {
            xl6OutputPrintf(ctx->out, " %s = 0;\n", returnName);
            if ( !xl6CgenTryClassCtorBoxXValueStmt(ctx, resultType, node->children[0], returnName) ) {
                xl6CgenError(ctx, node->children[0], "async class handle constructor lowering failed");
            }
        } else if ( emittedCtor ) {
            xl6OutputPrintf(ctx->out, " %s = {0};\n", returnName);
            if ( !xl6CgenEmitClassCtorInto(ctx, node->children[0], returnName) ) {
                xl6CgenError(ctx, node->children[0], "async class constructor lowering failed");
            }
        } else {
            xl6OutputPrintf(ctx->out, " %s = ", returnName);
        }
        xl6AstFree(genericCtorType);
        if ( emittedCtor ) {
            ;
        } else if ( xl6CgenIsTypedContainerType(ctx, resultType) ) {
            xl6CgenTypedContainerValueFromExpr(ctx, resultType, node->children[0]);
        } else if ( xl6CgenIsTaggedEnumType(ctx, resultType) ) {
            xl6CgenTaggedEnumValueExpr(ctx, resultType, node->children[0]);
        } else if ( xl6CgenIsWeakType(resultType) ) {
            xl6CgenWeakValueExpr(ctx, node->children[0]);
        } else if ( xl6CgenIsErrorType(ctx, resultType) ) {
            xl6CgenErrorOwnedExpr(ctx, node->children[0]);
        } else if ( xl6CgenIsXValueType(resultType) ) {
            xl6CgenXValueFromExprForTarget(ctx, resultType, node->children[0]);
        } else if ( xl6CgenIsFunctionType(resultType) ) {
            xl6CgenFunctionValueExpr(ctx, node->children[0], resultType);
        } else if ( xl6CgenNullableHasManagedInner(ctx, resultType) ) {
            xl6CgenNullableTaggedValueExpr(ctx, resultType, node->children[0]);
        } else if ( xl6CgenIsOwnedStringTypeNode(ctx, resultType) ) {
            xl6OutputPuts(
                xl6CgenIsNullableStringTypeNode(ctx, resultType) ?
                    "xl6_nullable_string_copy(" :
                    "xl6_xrt_string_dup(",
                ctx->out);
            xl6CgenStringSourceExpr(ctx, resultType, node->children[0]);
            xl6OutputPuts(
                xl6CgenIsNullableStringTypeNode(ctx, resultType) ?
                    ")" :
                    ", 0u)",
                ctx->out);
        } else {
            xl6CgenExpr(ctx, node->children[0]);
        }
        if ( !emittedCtor ) {
            xl6OutputPuts(";\n", ctx->out);
        }
    }
    if ( xl6CgenIsTypedContainerType(ctx, resultType) &&
         xl6CgenTypedContainerInitLiteral(ctx, resultType, node->children[0]) != NULL ) {
        xl6CgenTypedContainerFillFromLiteral(ctx, returnName, resultType, node->children[0]);
    }
    if ( xl6CgenIsFunctionType(resultType) && xl6CgenExprIsFunctionBorrow(ctx, node->children[0]) ) {
        xl6CgenRetainFunctionValue(ctx, returnName);
    }
    if ( xl6CgenIsSpecialCollectionType(ctx, resultType) &&
         !xl6CgenSpecialCollectionExprIsOwned(ctx, node->children[0]) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", returnName);
        xl6CgenSpecialCollectionRetainCExpr(ctx, resultType, returnName);
        xl6OutputPuts(";\n", ctx->out);
    }
    xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, resultType, returnName, node->children[0]);
    if ( hasReturnArgScope ) {
        xl6CgenClassExprTempScopeEnd(ctx, &returnArgScope);
    }

    if ( xl6CgenEmitPendingFinallyReturn(ctx, returnName, NULL) ) {
        return;
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts(cType, ctx->out);
    xl6OutputPrintf(ctx->out, "* __xl6_async_box_%d = (", id);
    xl6OutputPuts(cType, ctx->out);
    xl6OutputPuts("*)xrtMalloc(sizeof(", ctx->out);
    xl6OutputPuts(cType, ctx->out);
    xl6OutputPuts("));\n", ctx->out);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if ( __xl6_async_box_%d == 0 ) {\n", id);
    ctx->indent += 1;
    xl6CgenDiscardTemporaryResultByType(ctx, resultType, returnName);
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
    xl6OutputPuts("pOut->Error = 0;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("pOut->iFlags = XL6_FUTURE_OWN_VALUE;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("pOut->pfnFreeValue = ", ctx->out);
    xl6CgenAsyncResultFreeValue(ctx, ctx->currentReturnFunc, resultType);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
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

static const XL6_AstNode* xl6CgenAwaitOperand(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 0 ? node->children[0] : NULL;
}

static int xl6CgenAwaitOperandIsConditional(const XL6_AstNode* node)
{
    const XL6_AstNode* operand = xl6CgenAwaitOperand(node);

    return operand != NULL && operand->kind == XL6_AST_CONDITIONAL && operand->childCount > 2;
}

static void xl6CgenAwaitOperandExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    if ( node != NULL && node->childCount > 0 ) {
        xl6CgenExpr(ctx, node->children[0]);
    } else {
        xl6OutputPuts("0", ctx->out);
    }
}

static int xl6CgenAwaitFutureExprIsOwnedDirect(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;
    int owned;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL ) {
        return 0;
    }

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    owned = xl6CgenIsFutureLikeType(concreteType);
    xl6AstFree(typeNode);
    return owned;
}

static void xl6CgenAwaitOwnedFutureExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr != NULL && expr->kind == XL6_AST_CONDITIONAL && expr->childCount > 2 ) {
        xl6OutputPutc('(', ctx->out);
        xl6CgenConditionExpr(ctx, expr->children[0]);
        xl6OutputPuts(" ? ", ctx->out);
        xl6CgenAwaitOwnedFutureExpr(ctx, expr->children[1]);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenAwaitOwnedFutureExpr(ctx, expr->children[2]);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    if ( xl6CgenAwaitFutureExprIsOwnedDirect(ctx, expr) ) {
        xl6CgenExpr(ctx, expr);
        return;
    }

    xl6OutputPuts("xl6_future_add_ref_if_needed(", ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenAwaitOwnedOperandExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    xl6CgenAwaitOwnedFutureExpr(ctx, xl6CgenAwaitOperand(node));
}

static void xl6CgenAwaitStringBranchExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    int line,
    int column)
{
    if ( expr != NULL && expr->kind == XL6_AST_CONDITIONAL && expr->childCount > 2 ) {
        xl6OutputPutc('(', ctx->out);
        xl6CgenConditionExpr(ctx, expr->children[0]);
        xl6OutputPuts(" ? ", ctx->out);
        xl6CgenAwaitStringBranchExpr(ctx, expr->children[1], line, column);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenAwaitStringBranchExpr(ctx, expr->children[2], line, column);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    xl6OutputPuts(
        xl6CgenAwaitFutureExprIsOwnedDirect(ctx, expr) ?
        "xl6_future_await_cstr_release(" :
        "xl6_future_await_cstr_copy(",
        ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPrintf(ctx->out, ", \"\", %d, %d)", line, column);
}

static void xl6CgenAwaitTaggedEnumBranchExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* resultType,
    const XL6_AstNode* expr,
    int line,
    int column)
{
    if ( expr != NULL && expr->kind == XL6_AST_CONDITIONAL &&
         expr->childCount > 2 ) {
        xl6OutputPutc('(', ctx->out);
        xl6CgenConditionExpr(ctx, expr->children[0]);
        xl6OutputPuts(" ? ", ctx->out);
        xl6CgenAwaitTaggedEnumBranchExpr(
            ctx,
            resultType,
            expr->children[1],
            line,
            column);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenAwaitTaggedEnumBranchExpr(
            ctx,
            resultType,
            expr->children[2],
            line,
            column);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    xl6OutputPuts("(*(", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, resultType), ctx->out);
    xl6OutputPuts("*)xl6_future_await_typed_value_copy(", ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenEmitXrtTypeDescRef(ctx, resultType);
    xl6OutputPrintf(
        ctx->out,
        ", %d, \"\", %d, %d))",
        xl6CgenAwaitFutureExprIsOwnedDirect(ctx, expr) ? 1 : 0,
        line,
        column);
}

static const char* xl6CgenAwaitTypedContainerCopyFuncName(const char* typeName)
{
    if ( xl6CTextEquals(typeName, "array") ) {
        return "xl6_xrt_typed_array_copy";
    }
    if ( xl6CTextEquals(typeName, "list") ) {
        return "xl6_xrt_typed_list_copy";
    }
    if ( xl6CTextEquals(typeName, "set") ) {
        return "xl6_xrt_typed_set_copy";
    }
    if ( xl6CTextEquals(typeName, "dict") ) {
        return "xl6_xrt_typed_dict_copy";
    }
    return NULL;
}

static void xl6CgenAwaitTypedContainerBorrowedBranchExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* resultType,
    const XL6_AstNode* expr,
    int line,
    int column)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;
    const char* copyFunc;
    const char* cType;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, resultType);
    itemType = xl6CgenTypedContainerItemType(ctx, resultType);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    copyFunc = xl6CgenAwaitTypedContainerCopyFuncName(typeName);
    cType = xl6CgenTypedContainerCType(ctx, resultType);

    if ( copyFunc == NULL || itemType == NULL ) {
        xl6CgenError(ctx, expr, "conditional await typed container requires array/list/set/dict");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    xl6OutputPuts(copyFunc, ctx->out);
    xl6OutputPutc('(', ctx->out);
    xl6CgenEmitXrtTypeDescRef(ctx, itemType);
    xl6OutputPuts(", (*(", ctx->out);
    xl6OutputPuts(cType, ctx->out);
    xl6OutputPuts("*)xl6_future_await_value(", ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPrintf(ctx->out, ", \"\", %d, %d, 1, sizeof(", line, column);
    xl6OutputPuts(cType, ctx->out);
    xl6OutputPuts("))))", ctx->out);
}

static void xl6CgenAwaitErrorBranchExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    int line,
    int column)
{
    if ( expr != NULL && expr->kind == XL6_AST_CONDITIONAL && expr->childCount > 2 ) {
        xl6OutputPutc('(', ctx->out);
        xl6CgenConditionExpr(ctx, expr->children[0]);
        xl6OutputPuts(" ? ", ctx->out);
        xl6CgenAwaitErrorBranchExpr(ctx, expr->children[1], line, column);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenAwaitErrorBranchExpr(ctx, expr->children[2], line, column);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    xl6OutputPuts("(XL6_Error*)", ctx->out);
    xl6OutputPuts(
        xl6CgenAwaitFutureExprIsOwnedDirect(ctx, expr) ?
            "xl6_future_await_handle_release(" :
            "xl6_future_await_handle_copy(",
        ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPrintf(ctx->out, ", xl6TypeError(), \"\", %d, %d)", line, column);
}

static void xl6CgenAwaitTypedContainerBranchExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* resultType,
    const XL6_AstNode* expr,
    int line,
    int column)
{
    const char* cType;

    if ( expr != NULL && expr->kind == XL6_AST_CONDITIONAL && expr->childCount > 2 ) {
        xl6OutputPutc('(', ctx->out);
        xl6CgenConditionExpr(ctx, expr->children[0]);
        xl6OutputPuts(" ? ", ctx->out);
        xl6CgenAwaitTypedContainerBranchExpr(ctx, resultType, expr->children[1], line, column);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenAwaitTypedContainerBranchExpr(ctx, resultType, expr->children[2], line, column);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    if ( xl6CgenAwaitFutureExprIsOwnedDirect(ctx, expr) ) {
        cType = xl6CgenTypedContainerCType(ctx, resultType);
        xl6OutputPutc('(', ctx->out);
        xl6OutputPuts(cType, ctx->out);
        xl6OutputPuts(")xl6_future_await_handle_release(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, resultType);
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d)", line, column);
        return;
    }

    xl6CgenAwaitTypedContainerBorrowedBranchExpr(ctx, resultType, expr, line, column);
}

static void xl6CgenAwaitSpecialCollectionBranchExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* resultType,
    const XL6_AstNode* expr,
    int line,
    int column)
{
    const char* cType;

    if ( expr != NULL && expr->kind == XL6_AST_CONDITIONAL && expr->childCount > 2 ) {
        xl6OutputPutc('(', ctx->out);
        xl6CgenConditionExpr(ctx, expr->children[0]);
        xl6OutputPuts(" ? ", ctx->out);
        xl6CgenAwaitSpecialCollectionBranchExpr(ctx, resultType, expr->children[1], line, column);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenAwaitSpecialCollectionBranchExpr(ctx, resultType, expr->children[2], line, column);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    cType = xl6CgenTypeName(ctx, resultType);
    xl6OutputPutc('(', ctx->out);
    xl6OutputPuts(cType, ctx->out);
    xl6OutputPutc(')', ctx->out);
    xl6OutputPuts(
        xl6CgenAwaitFutureExprIsOwnedDirect(ctx, expr)
            ? "xl6_future_await_handle_release("
            : "xl6_future_await_handle_copy(",
        ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenEmitXrtTypeDescRef(ctx, resultType);
    xl6OutputPrintf(ctx->out, ", \"\", %d, %d)", line, column);
}

static void xl6CgenAwaitClassBranchExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* resultType,
    const XL6_AstNode* expr,
    int line,
    int column)
{
    XL6_AstNode* helperFunc;

    if ( expr != NULL && expr->kind == XL6_AST_CONDITIONAL && expr->childCount > 2 ) {
        xl6OutputPutc('(', ctx->out);
        xl6CgenConditionExpr(ctx, expr->children[0]);
        xl6OutputPuts(" ? ", ctx->out);
        xl6CgenAwaitClassBranchExpr(ctx, resultType, expr->children[1], line, column);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenAwaitClassBranchExpr(ctx, resultType, expr->children[2], line, column);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    helperFunc = xl6CgenAwaitClassReleaseHelperFunc(ctx, resultType, expr);
    if ( helperFunc == NULL ) {
        xl6CgenError(ctx, expr, "conditional await class future requires a matching async class result helper");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    xl6CgenAwaitClassReleaseHelperSymbol(ctx, helperFunc);
    xl6OutputPutc('(', ctx->out);
    xl6CgenAwaitOwnedFutureExpr(ctx, expr);
    xl6OutputPrintf(ctx->out, ", \"\", %d, %d)", line, column);
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

static const char* xl6CgenAwaitReleaseHelperName(
    XL6_CgenContext* ctx,
    const XL6_AstNode* resultType,
    int requireValue,
    char* nullableName,
    size_t nullableNameCapacity)
{
    const char* cType;

    if ( !requireValue ) {
        return "xl6_future_await_void_release";
    }
    if ( xl6CgenIsWeakType(resultType) ) {
        return "xl6_future_await_weak_release";
    }
    if ( xl6CgenIsFunctionType(resultType) ) {
        return "xl6_future_await_function_release";
    }
    if ( xl6CgenNullableHasManagedInner(ctx, resultType) ) {
        xl6CgenNullableAwaitReleaseHelperName(
            nullableName,
            nullableNameCapacity,
            resultType);
        return nullableName;
    }
    if ( xl6CgenIsTypedContainerType(ctx, resultType) ) {
        return "xl6_future_await_handle_release";
    }
    if ( xl6CgenIsSpecialCollectionType(ctx, resultType) ) {
        return "xl6_future_await_handle_release";
    }
    if ( xl6CgenIsXValueType(resultType) ) {
        return "xl6_future_await_xvalue_release";
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

static int xl6CgenAwaitReleaseHelperCopiesResult(XL6_CgenContext* ctx, const XL6_AstNode* resultType, int requireValue)
{
    const char* cType;

    if ( !requireValue ) {
        return 1;
    }
    if ( xl6CgenIsWeakType(resultType) ||
         xl6CgenIsFunctionType(resultType) ||
         xl6CgenNullableHasManagedInner(ctx, resultType) ||
         xl6CgenIsXValueType(resultType) ) {
        return 1;
    }
    if ( resultType == NULL || xl6CgenIsTypedContainerType(ctx, resultType) ||
         xl6CgenIsSpecialCollectionType(ctx, resultType) || xl6CgenIsClassType(ctx, resultType) ) {
        return 0;
    }

    cType = xl6CgenTypeName(ctx, resultType);
    return xl6CTextEquals(cType, "int") ||
           xl6CTextEquals(cType, "long long") ||
           xl6CTextEquals(cType, "unsigned long long") ||
           xl6CTextEquals(cType, "float") ||
           xl6CTextEquals(cType, "double") ||
           xl6CTextEquals(cType, "void*");
}

static const char* xl6CgenAwaitResultCType(XL6_CgenContext* ctx, const XL6_AstNode* resultType)
{
    if ( xl6CgenIsTypedContainerType(ctx, resultType) ) {
        return xl6CgenTypedContainerCType(ctx, resultType);
    }
    return xl6CgenTypeName(ctx, resultType);
}

static int xl6CgenAwaitConditionalBranchResultExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* resultType,
    int requireValue)
{
    const XL6_AstNode* operand;
    int line;
    int column;

    if ( !xl6CgenAwaitOperandIsConditional(node) ) {
        return 0;
    }

    operand = xl6CgenAwaitOperand(node);
    line = node != NULL ? node->line : 0;
    column = node != NULL ? node->column : 0;

    if ( requireValue && xl6CTextEquals(xl6CgenAwaitResultCType(ctx, resultType), "const char*") ) {
        xl6CgenAwaitStringBranchExpr(ctx, operand, line, column);
        return 1;
    }

    if ( requireValue && xl6CgenIsTypedContainerType(ctx, resultType) ) {
        xl6CgenAwaitTypedContainerBranchExpr(ctx, resultType, operand, line, column);
        return 1;
    }

    if ( requireValue && xl6CgenIsSpecialCollectionType(ctx, resultType) ) {
        xl6CgenAwaitSpecialCollectionBranchExpr(ctx, resultType, operand, line, column);
        return 1;
    }

    if ( requireValue && xl6CgenIsErrorType(ctx, resultType) ) {
        xl6CgenAwaitErrorBranchExpr(ctx, operand, line, column);
        return 1;
    }

    if ( requireValue && xl6CgenIsClassType(ctx, resultType) ) {
        xl6CgenAwaitClassBranchExpr(ctx, resultType, operand, line, column);
        return 1;
    }

    return 0;
}

static int xl6CgenAwaitOwnedTemporaryHelperExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* resultType,
    int requireValue)
{
    const char* helperName;
    char nullableHelperName[64];

    if ( requireValue && xl6CgenIsErrorType(ctx, resultType) ) {
        xl6CgenAwaitErrorBranchExpr(
            ctx,
            xl6CgenAwaitOperand(node),
            node != NULL ? node->line : 0,
            node != NULL ? node->column : 0);
        return 1;
    }

    if ( requireValue && xl6CgenIsClassType(ctx, resultType) ) {
        xl6CgenAwaitClassBranchExpr(
            ctx,
            resultType,
            xl6CgenAwaitOperand(node),
            node != NULL ? node->line : 0,
            node != NULL ? node->column : 0);
        return 1;
    }

    if ( requireValue && xl6CgenIsTaggedEnumType(ctx, resultType) ) {
        xl6CgenAwaitTaggedEnumBranchExpr(
            ctx,
            resultType,
            xl6CgenAwaitOperand(node),
            node != NULL ? node->line : 0,
            node != NULL ? node->column : 0);
        return 1;
    }

    helperName = xl6CgenAwaitReleaseHelperName(
        ctx,
        resultType,
        requireValue,
        nullableHelperName,
        sizeof(nullableHelperName));
    if ( helperName == NULL ) {
        return 0;
    }

    xl6OutputPuts(helperName, ctx->out);
    xl6OutputPutc('(', ctx->out);
    if ( xl6CgenAwaitOperandIsConditional(node) ) {
        xl6CgenAwaitOwnedOperandExpr(ctx, node);
    } else {
        xl6CgenAwaitOperandExpr(ctx, node);
    }
    if ( xl6CgenIsTypedContainerType(ctx, resultType) ||
         xl6CgenIsSpecialCollectionType(ctx, resultType) ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, resultType);
    }
    xl6OutputPrintf(ctx->out, ", \"\", %d, %d)", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
    return 1;
}

static void xl6CgenAwaitOwnedTemporaryExpr(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* resultType, int requireValue)
{
    (void)resultType;
    (void)requireValue;
    xl6CgenError(ctx, node, "temporary await requires a typed release helper");
    xl6OutputPuts("0", ctx->out);
}

static void xl6CgenAwaitExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* resultType;
    int requireValue;

    resultType = xl6CgenCloneAwaitResultType(ctx, node);
    requireValue = resultType != NULL && !xl6CTextEquals(resultType->text, "null");

    if ( xl6CgenAwaitConditionalBranchResultExpr(ctx, node, resultType, requireValue) ) {
        xl6AstFree(resultType);
        return;
    }

    if ( requireValue && xl6CgenIsTaggedEnumType(ctx, resultType) ) {
        xl6CgenAwaitTaggedEnumBranchExpr(
            ctx,
            resultType,
            xl6CgenAwaitOperand(node),
            node != NULL ? node->line : 0,
            node != NULL ? node->column : 0);
        xl6AstFree(resultType);
        return;
    }

    if ( xl6CgenAwaitOperandIsOwnedTemporary(node) ||
         (xl6CgenAwaitOperandIsConditional(node) &&
          xl6CgenAwaitReleaseHelperCopiesResult(ctx, resultType, requireValue)) ) {
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

    if ( xl6CTextEquals(xl6CgenAwaitResultCType(ctx, resultType), "const char*") ) {
        xl6OutputPuts("xl6_future_await_cstr_copy(", ctx->out);
        xl6CgenAwaitOperandExpr(ctx, node);
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d)", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
        xl6AstFree(resultType);
        return;
    }

    if ( xl6CgenIsTypedContainerType(ctx, resultType) ) {
        xl6CgenAwaitTypedContainerBorrowedBranchExpr(
            ctx,
            resultType,
            xl6CgenAwaitOperand(node),
            node != NULL ? node->line : 0,
            node != NULL ? node->column : 0);
        xl6AstFree(resultType);
        return;
    }

    if ( xl6CgenIsSpecialCollectionType(ctx, resultType) ) {
        xl6OutputPutc('(', ctx->out);
        xl6OutputPuts(xl6CgenAwaitResultCType(ctx, resultType), ctx->out);
        xl6OutputPuts(")xl6_future_await_handle_copy(", ctx->out);
        xl6CgenAwaitOperandExpr(ctx, node);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, resultType);
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d)", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
        xl6AstFree(resultType);
        return;
    }

    if ( xl6CgenNullableHasManagedInner(ctx, resultType) ) {
        char helperName[64];
        xl6CgenNullableAwaitCopyHelperName(
            helperName,
            sizeof(helperName),
            resultType);
        xl6OutputPrintf(ctx->out, "%s(", helperName);
        xl6CgenAwaitOperandExpr(ctx, node);
        xl6OutputPrintf(
            ctx->out,
            ", \"\", %d, %d)",
            node != NULL ? node->line : 0,
            node != NULL ? node->column : 0);
        xl6AstFree(resultType);
        return;
    }

	if ( xl6CgenIsWeakType(resultType) ) {
		xl6OutputPuts("xl6_future_await_weak_copy(", ctx->out);
		xl6CgenAwaitOperandExpr(ctx, node);
		xl6OutputPrintf(
			ctx->out,
			", \"\", %d, %d)",
			node != NULL ? node->line : 0,
			node != NULL ? node->column : 0);
		xl6AstFree(resultType);
		return;
	}

    if ( xl6CgenIsErrorType(ctx, resultType) ) {
        xl6OutputPuts("(XL6_Error*)xl6_future_await_handle_copy(", ctx->out);
        xl6CgenAwaitOperandExpr(ctx, node);
        xl6OutputPrintf(
            ctx->out,
            ", xl6TypeError(), \"\", %d, %d)",
            node != NULL ? node->line : 0,
            node != NULL ? node->column : 0);
        xl6AstFree(resultType);
        return;
    }

    /*
     * Future 始终拥有完成值。非临时 future 可被重复 await，因此 var、class
     * 和协议句柄每次读取都必须取得独立强引用，不能移动 future 内部结果。
     */
    if ( xl6CgenIsXValueType(resultType) || xl6CgenIsClassType(ctx, resultType) ) {
        xl6OutputPuts("xl6_future_await_xvalue_copy(", ctx->out);
        xl6CgenAwaitOperandExpr(ctx, node);
        xl6OutputPrintf(
            ctx->out,
            ", \"\", %d, %d)",
            node != NULL ? node->line : 0,
            node != NULL ? node->column : 0);
        xl6AstFree(resultType);
        return;
    }

    xl6OutputPuts("(*(", ctx->out);
    xl6OutputPuts(xl6CgenAwaitResultCType(ctx, resultType), ctx->out);
    xl6OutputPuts("*)xl6_future_await_value(", ctx->out);
    xl6CgenAwaitOperandExpr(ctx, node);
    xl6OutputPrintf(ctx->out, ", \"\", %d, %d, 1, sizeof(", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
    xl6OutputPuts(xl6CgenAwaitResultCType(ctx, resultType), ctx->out);
    xl6OutputPuts(")))", ctx->out);
    xl6AstFree(resultType);
}

#include "xl6_cgen_async_func_emit.c"
