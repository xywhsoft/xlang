static int xl6CgenFunctionTypeSupportsErasedCall(XL6_CgenContext* ctx, const XL6_AstNode* functionType)
{
    const XL6_AstNode* paramTypes;
    const XL6_AstNode* returns;
    size_t i;

    paramTypes = xl6CgenFunctionParamTypes(functionType);
    returns = xl6CgenFunctionReturnTypes(functionType);
    if ( paramTypes == NULL || returns == NULL || returns->childCount > 1 ) {
        return 0;
    }
    if ( returns->childCount == 1 &&
         !xl6CgenTypeSupportsVarInvokeValue(ctx, returns->children[0]) ) {
        return 0;
    }
    for ( i = 0; i < paramTypes->childCount; ++i ) {
        if ( !xl6CgenTypeSupportsVarInvokeValue(ctx, paramTypes->children[i]) ) {
            return 0;
        }
    }
    return 1;
}

static XL6_AstNode* xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* returns;
    XL6_AstNode* functionType;

    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return NULL;
    }
    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER ) {
        return NULL;
    }
    functionType = xl6CgenCloneMemberFieldTypeForInference(ctx, callee);
    returns = xl6CgenFunctionReturnTypes(functionType);
    if ( !xl6CgenIsFunctionType(functionType) ||
         returns == NULL || returns->childCount != 1 ||
         xl6CgenFunctionTypeSupportsErasedCall(ctx, functionType) ) {
        xl6AstFree(functionType);
        return NULL;
    }
    return functionType;
}

static void xl6CgenEmitMemberFunctionStatementTemp(XL6_CgenContext* ctx, const XL6_AstNode* expr, const char* tempName)
{
    const XL6_AstNode* callee;

    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 || tempName == NULL ) {
        return;
    }
    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER ) {
        return;
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "XL6_Function %s = ", tempName);
    xl6CgenExpr(ctx, callee);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenMemberFunctionStatementInvokeExpr(XL6_CgenContext* ctx,
                                                     const XL6_AstNode* expr,
                                                     const XL6_AstNode* functionType,
                                                     const char* tempName)
{
    const XL6_AstNode* args = expr != NULL && expr->childCount > 1 ? expr->children[1] : NULL;
    xl6CgenFunctionInvoke(ctx, tempName, functionType, args);
}

static int xl6CgenMemberIsClassMethodCallTarget(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    XL6_AstNode* ownedReceiverType;
    const XL6_AstNode* classNode;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    const XL6_AstNode* method;

    if ( ctx == NULL ||
         call == NULL ||
         member == NULL ||
         member->kind != XL6_AST_MEMBER ||
         member->text == NULL ||
         member->childCount == 0 ) {
        return 0;
    }

    receiver = member->children[0];
    receiverType = NULL;
    ownedReceiverType = NULL;
    classNode = NULL;

    if ( receiver != NULL &&
         receiver->kind == XL6_AST_NAME &&
         xl6CTextEquals(receiver->text, "self") &&
         ctx->currentClass != NULL ) {
        classNode = ctx->currentClass;
    } else if ( receiver != NULL && receiver->kind == XL6_AST_MEMBER ) {
        ownedReceiverType = xl6CgenCloneMemberFieldTypeForInference(ctx, receiver);
        receiverType = ownedReceiverType;
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    } else if ( receiver != NULL && receiver->kind == XL6_AST_NAME ) {
        ownedReceiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
        receiverType = ownedReceiverType;
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    }

    if ( classNode == NULL ) {
        xl6AstFree(ownedReceiverType);
        return 0;
    }

    /*
     * 普通 class 方法优先于 function 字段和 __field__ 动态字段。
     * 否则带 __field__ 的类中，self.method() 会先被推断成 var 字段调用，
     * 真实方法永远没有机会进入 class method lowering。
     */
    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( receiverType != NULL && receiverType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = receiverType->children[0];
    }
    method = xl6CgenResolveClassFunc(ctx, classNode, call, member->text, NULL);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    xl6AstFree(ownedReceiverType);
    return method != NULL;
}

static void xl6CgenFunctionInvoke(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* functionType, const XL6_AstNode* args)
{
    const XL6_AstNode* paramTypes;
    const XL6_AstNode* returns;
    unsigned long long signatureId;
    size_t fixedCount;
    size_t positionalCount;
    int hasArrayVararg;
    int hasDictVararg;
    int returnCount;
    char cName[512];

    paramTypes = xl6CgenFunctionParamTypes(functionType);
    returns = xl6CgenFunctionReturnTypes(functionType);
    if ( paramTypes == NULL || returns == NULL ) {
        xl6CgenBareFunctionInvoke(ctx, name, args);
        return;
    }
    hasArrayVararg = paramTypes->childCount > 0 &&
                     xl6CgenFunctionTypeParamIsArrayVararg(paramTypes->children[paramTypes->childCount - 1]);
    hasDictVararg = paramTypes->childCount > 0 &&
                    xl6CgenFunctionTypeParamIsDictVararg(paramTypes->children[paramTypes->childCount - 1]);
    fixedCount = xl6CgenFunctionTypeFixedParamCount(paramTypes);
    positionalCount = xl6CgenFunctionValuePositionalArgCount(args);
    if ( positionalCount < fixedCount ||
         (!hasArrayVararg && positionalCount > fixedCount) ||
         (!hasDictVararg && xl6CgenFunctionValueDictVarargCount(args) > 0) ) {
        xl6CgenError(ctx, functionType, "function 调用实参数量和签名不匹配");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    returnCount = (int)returns->childCount;
    signatureId = xl6CgenFunctionSignatureId(ctx, paramTypes, returns);
    xl6CgenFunctionValueCName(ctx, name, cName, sizeof(cName));

    xl6OutputPrintf(ctx->out, "((%s.signature == 0x%016llXULL) ? ", cName, signatureId);
    if ( returnCount > 1 ) {
        xl6OutputPrintf(ctx->out, "((%s.invoke != 0) ? ", cName);
        xl6CgenFunctionInvokeDirectCall(ctx, cName, paramTypes, returns, args, fixedCount, hasArrayVararg, hasDictVararg);
        xl6OutputPuts(" : (xl6_function_fatal(3), (", ctx->out);
        xl6CgenFunctionReturnStructName(ctx, paramTypes, returns);
        xl6OutputPuts("){0})) : (xl6_function_fatal(3), (", ctx->out);
        xl6CgenFunctionReturnStructName(ctx, paramTypes, returns);
        xl6OutputPuts("){0}))", ctx->out);
    } else if ( returnCount == 1 ) {
        xl6OutputPrintf(ctx->out, "((%s.invoke != 0) ? ", cName);
        xl6CgenFunctionInvokeDirectCall(ctx, cName, paramTypes, returns, args, fixedCount, hasArrayVararg, hasDictVararg);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenFunctionInvokeFallbackExpr(ctx, returns->children[0], cName, args);
        xl6OutputPuts(") : (xl6_function_fatal(3), ", ctx->out);
        xl6CgenFunctionDefaultReturnExpr(ctx, returns->children[0]);
        xl6OutputPuts("))", ctx->out);
    } else {
        xl6OutputPrintf(ctx->out, "((%s.invoke != 0) ? ", cName);
        xl6CgenFunctionInvokeDirectCall(ctx, cName, paramTypes, returns, args, fixedCount, hasArrayVararg, hasDictVararg);
        xl6OutputPuts(" : xl6_xvalue_drop(", ctx->out);
        xl6CgenFunctionInvokeFallbackCall(ctx, cName, args);
        xl6OutputPuts(")) : xl6_function_fatal(3))", ctx->out);
    }
}

static int xl6CgenMemberFunctionInvoke(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    XL6_AstNode* functionType;
    const XL6_AstNode* args;

    if ( xl6CgenMemberIsClassMethodCallTarget(ctx, call, member) ) {
        return 0;
    }

    functionType = xl6CgenCloneMemberFieldTypeForInference(ctx, member);
    if ( functionType == NULL ) {
        return 0;
    }

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    if ( xl6CgenIsFunctionType(functionType) ) {
        const XL6_AstNode* returns = xl6CgenFunctionReturnTypes(functionType);
        int returnCount = returns != NULL ? (int)returns->childCount : 0;

        if ( xl6CgenFunctionTypeSupportsErasedCall(ctx, functionType) ) {
            if ( returnCount == 1 ) {
                xl6CgenFunctionInvokeFallbackExprForExpr(ctx, returns->children[0], member, functionType, args, 0);
            } else {
                xl6OutputPuts("xl6_xvalue_drop(", ctx->out);
                xl6CgenFunctionInvokeFallbackCallExpr(ctx, member, functionType, args, 0);
                xl6OutputPutc(')', ctx->out);
            }
            xl6AstFree(functionType);
            return 1;
        }

        if ( xl6CgenFunctionExprIsRepeatableLvalue(member) ) {
            xl6CgenFunctionInvokeExpr(ctx, member, functionType, args);
        } else {
            xl6CgenError(ctx, member, "member function expression receiver must be a stable lvalue");
            if ( returnCount == 1 ) {
                xl6CgenFunctionDefaultReturnExpr(ctx, returns != NULL ? returns->children[0] : NULL);
            } else if ( returnCount > 1 ) {
                xl6OutputPutc('(', ctx->out);
                xl6CgenFunctionReturnStructName(ctx, xl6CgenFunctionParamTypes(functionType), returns);
                xl6OutputPuts("){0}", ctx->out);
            } else {
                xl6OutputPuts("xl6_function_fatal(3)", ctx->out);
            }
        }
        xl6AstFree(functionType);
        return 1;
    }
    if ( xl6CTextEquals(functionType->text, "var") ) {
        xl6CgenVarFunctionInvokeExpr(ctx, member, args);
        xl6AstFree(functionType);
        return 1;
    }

    xl6AstFree(functionType);
    return 0;
}

static void xl6CgenAnonMakeCall(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_CgenAnonFunc* info;
    size_t i;

    info = xl6CgenFindAnonFuncInfo(ctx, node);
    if ( info == NULL ) {
        xl6CgenError(ctx, node, "anonymous function metadata was not collected");
        xl6OutputPuts("(XL6_Function){0}", ctx->out);
        return;
    }

    xl6OutputPrintf(ctx->out, "xl6_anon_%d_make(", info->id);
    for ( i = 0; i < info->captureCount; ++i ) {
        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenFunctionValueExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* targetType);
static void xl6CgenConditionExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr);

static int xl6CgenFunctionValueConditionalNeedsOwnedBranches(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    int thenBorrow;
    int elseBorrow;

    if ( expr == NULL || expr->kind != XL6_AST_CONDITIONAL || expr->childCount < 3 ) {
        return 0;
    }
    thenBorrow = xl6CgenExprIsFunctionBorrow(ctx, expr->children[1]);
    elseBorrow = xl6CgenExprIsFunctionBorrow(ctx, expr->children[2]);
    if ( thenBorrow != elseBorrow ) {
        return 1;
    }
    return xl6CgenFunctionValueConditionalNeedsOwnedBranches(ctx, expr->children[1]) ||
           xl6CgenFunctionValueConditionalNeedsOwnedBranches(ctx, expr->children[2]);
}

static void xl6CgenFunctionValueConditionalBranch(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* targetType)
{
    if ( xl6CgenExprIsFunctionBorrow(ctx, expr) ) {
        xl6OutputPuts("xl6_function_copy_value(", ctx->out);
        xl6CgenFunctionValueExpr(ctx, expr, targetType);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    xl6CgenFunctionValueExpr(ctx, expr, targetType);
}

static void xl6CgenFunctionValueConditionalExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* targetType)
{
    xl6OutputPutc('(', ctx->out);
    xl6CgenConditionExpr(ctx, expr->children[0]);
    xl6OutputPuts(" ? ", ctx->out);
    xl6CgenFunctionValueConditionalBranch(ctx, expr->children[1], targetType);
    xl6OutputPuts(" : ", ctx->out);
    xl6CgenFunctionValueConditionalBranch(ctx, expr->children[2], targetType);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenFunctionValueExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* targetType)
{
    if ( expr != NULL && expr->kind == XL6_AST_NAME &&
         xl6CgenFindVarType(ctx, expr->text) == NULL &&
         xl6CgenIsFunctionType(targetType) ) {
        XL6_AstNode* func = xl6CgenFindFunctionValueFunc(ctx, expr->text, targetType, expr);
        if ( func != NULL ) {
            xl6CgenFunctionValueMakeCall(ctx, func, targetType);
            return;
        }
    }
    if ( xl6CgenIsFunctionType(targetType) &&
         xl6CgenFunctionValueConditionalNeedsOwnedBranches(ctx, expr) ) {
        xl6CgenFunctionValueConditionalExpr(ctx, expr, targetType);
        return;
    }
    xl6CgenExpr(ctx, expr);
}
