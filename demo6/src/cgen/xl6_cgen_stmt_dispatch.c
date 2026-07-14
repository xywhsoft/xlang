static void xl6CgenForIn(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* iterableType;

    if ( node->childCount < 3 ) {
        xl6CgenError(ctx, node, "malformed for-in statement");
        return;
    }
    if ( xl6CgenIsRangeCall(node->children[1]) ) {
        xl6CgenForRange(ctx, node);
        return;
    }

    iterableType = xl6CgenCloneExprTypeForInference(ctx, node->children[1]);
    if ( !xl6CgenIsForIterableType(ctx, iterableType) &&
         !xl6CgenIsClassProtocolIterableType(ctx, iterableType) ) {
        xl6CgenError(ctx, node->children[1], "for in right side must be range, string, array, list, set, dict, generator, var or class iterable protocol");
        xl6AstFree(iterableType);
        return;
    }

    if ( xl6CgenIsGeneratorType(iterableType) ) {
        xl6CgenForGeneratorIterable(ctx, node, iterableType);
    } else if ( iterableType != NULL && xl6CTextEquals(iterableType->text, "string") ) {
        xl6CgenForStringIterable(ctx, node, iterableType);
    } else if ( xl6CgenIsClassProtocolIterableType(ctx, iterableType) ) {
        xl6CgenForClassProtocolIterable(ctx, node, iterableType);
    } else if ( xl6CgenExprUsesTypedContainerStorage(ctx, node->children[1]) ||
                xl6CgenExprIsTypedContainerReturnCall(ctx, node->children[1]) ) {
        xl6CgenForTypedContainerIterable(ctx, node, iterableType);
    } else {
        xl6CgenForXValueIterable(ctx, node, iterableType);
    }
    xl6AstFree(iterableType);
}

static int xl6CgenReturnTryMultiCallInto(XL6_CgenContext* ctx, const XL6_AstNode* expr, int returnId)
{
    const XL6_AstNode* func;
    const XL6_AstNode* functionType;
    const XL6_AstNode* classMethod;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* receiverType;
    XL6_AstNode* ownedFunctionType;
    XL6_AstNode* ownedFuncTypeArgs;
    const XL6_AstNode* returns;
    int multiId;
    size_t i;
    char memberTempName[64];

    if ( ctx == NULL || expr == NULL || ctx->currentReturns == NULL || ctx->currentReturns->childCount <= 1 ) {
        return 0;
    }

    ownedFunctionType = NULL;
    ownedFuncTypeArgs = NULL;
    functionType = xl6CgenResolveMultiReturnFunctionType(ctx, expr, &ownedFunctionType);
    func = functionType == NULL ? xl6CgenResolveMultiReturnCall(ctx, expr, &ownedFuncTypeArgs) : NULL;
    ownerClass = NULL;
    receiverType = NULL;
    classMethod = (functionType == NULL && func == NULL) ?
        xl6CgenResolveMultiReturnClassMethod(ctx, expr, &ownerClass, &receiverType) :
        NULL;
    returns = functionType != NULL ? xl6CgenFunctionReturnTypes(functionType) :
              (func != NULL ? func->children[2] :
              (classMethod != NULL && classMethod->childCount > 2 ? classMethod->children[2] : NULL));
    if ( returns == NULL || returns->childCount != ctx->currentReturns->childCount ) {
        xl6AstFree(ownedFunctionType);
        xl6AstFree(ownedFuncTypeArgs);
        return 0;
    }

    multiId = ctx->tempId++;
    memberTempName[0] = '\0';
    xl6CgenEmitMultiReturnMemberTemp(
        ctx,
        xl6CgenMultiReturnMemberCallee(expr, functionType),
        multiId,
        memberTempName,
        sizeof(memberTempName));

    xl6CgenIndent(ctx);
    xl6CgenMultiReturnStructName(ctx, func, functionType, classMethod, ownerClass, receiverType, ownedFuncTypeArgs);
    xl6OutputPrintf(ctx->out, " __xl6_multi_%d = ", multiId);
    xl6CgenEmitMultiReturnCall(ctx, expr, func, functionType, classMethod, ownedFuncTypeArgs, memberTempName[0] != '\0' ? memberTempName : NULL);
    xl6OutputPuts(";\n", ctx->out);

    xl6CgenIndent(ctx);
    if ( ctx->currentReturnFunc != NULL ) {
        xl6CgenFuncReturnStructName(ctx, ctx->currentReturnFunc);
    } else {
        xl6CgenFunctionReturnStructName(ctx, ctx->currentReturnParams, ctx->currentReturns);
    }
    xl6OutputPrintf(ctx->out, " __xl6_return_%d;\n", returnId);
    for ( i = 0; i < ctx->currentReturns->childCount; ++i ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_return_%d.value%u = __xl6_multi_%d.value%u;\n",
                        returnId, (unsigned)i, multiId, (unsigned)i);
    }
    xl6AstFree(ownedFunctionType);
    xl6AstFree(ownedFuncTypeArgs);
    return 1;
}

static int xl6CgenReturnTrySingleValueStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* returnType,
    const char* returnName,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* concreteReturnType;
    const char* cType;

    if ( ctx == NULL || returnType == NULL || returnName == NULL || expr == NULL ) {
        return 0;
    }

    concreteReturnType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, returnType));
    if ( concreteReturnType != NULL && xl6CTextEquals(concreteReturnType->text, "string") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "const char* %s = xrtCopyStr(", returnName);
        xl6CgenStringSourceExpr(ctx, concreteReturnType, expr);
        xl6OutputPuts(", 0u);\n", ctx->out);
        return 1;
    }

    if ( xl6CgenIsXValueType(returnType) ) {
        if ( xl6CgenDeclTryXValueArrayLiteralInit(ctx, returnType, returnName, expr) ||
             xl6CgenDeclTryXValueNullCoalesceInit(ctx, returnType, returnName, expr) ||
             xl6CgenDeclTryXValueBinaryInit(ctx, returnType, returnName, expr) ||
             xl6CgenDeclTryXValueConditionalInit(ctx, returnType, returnName, expr) ) {
            return 1;
        }
    }

    cType = xl6CgenTypeName(ctx, returnType);
    if ( xl6CgenDeclTryStaticNullCoalesceInit(ctx, returnType, cType, returnName, expr) ) {
        return 1;
    }

    if ( xl6CgenDeclCanEmitCallArgTemp(ctx, returnType, expr) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = 0;\n", cType, returnName);
        if ( !xl6CgenDeclEmitCallArgTemp(ctx, returnType, cType, returnName, expr) ) {
            xl6CgenError(ctx, expr, "return expression lowering failed");
        }
        return 1;
    }

    return 0;
}

static void xl6CgenPrepareMultiStringReturns(XL6_CgenContext* ctx, int returnId)
{
    size_t i;

    if ( ctx == NULL || ctx->currentReturns == NULL || ctx->currentReturns->childCount <= 1 ) {
        return;
    }
    for ( i = 0; i < ctx->currentReturns->childCount; ++i ) {
        if ( !xl6CgenIsStringTypeNode(ctx, ctx->currentReturns->children[i]) ) {
            continue;
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out,
                        "__xl6_return_%d.value%u = xrtCopyStr(__xl6_return_%d.value%u, 0u);\n",
                        returnId, (unsigned)i, returnId, (unsigned)i);
    }
}

static void xl6CgenPromoteMultiStringReturns(XL6_CgenContext* ctx, int returnId)
{
    size_t i;

    if ( ctx == NULL || ctx->currentReturns == NULL || ctx->currentReturns->childCount <= 1 ) {
        return;
    }
    for ( i = 0; i < ctx->currentReturns->childCount; ++i ) {
        if ( !xl6CgenIsStringTypeNode(ctx, ctx->currentReturns->children[i]) ) {
            continue;
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out,
                        "__xl6_return_%d.value%u = xrtTempStringTake((char*)__xl6_return_%d.value%u, 0u);\n",
                        returnId, (unsigned)i, returnId, (unsigned)i);
    }
}

static int xl6CgenReturnCanMultiXValueFieldStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* returnType,
    const XL6_AstNode* expr)
{
    if ( ctx == NULL || returnType == NULL || expr == NULL ||
         !xl6CgenIsXValueType(returnType) ) {
        return 0;
    }

    if ( xl6CgenDeclArrayLiteralSource(expr) != NULL &&
         xl6CgenDeclCanUseXValueArrayLiteralInit(ctx, returnType) ) {
        return 1;
    }

    if ( xl6CgenDeclIsNullCoalesceExpr(expr) ) {
        XL6_AstNode* leftType;
        int ok;

        if ( expr->childCount < 1 || expr->children[0] == NULL ||
             expr->children[0]->kind == XL6_AST_NULL ) {
            return 0;
        }
        leftType = xl6CgenCloneExprTypeForInference(ctx, expr->children[0]);
        ok = xl6CgenIsXValueType(leftType);
        xl6AstFree(leftType);
        return ok;
    }

    return (expr->kind == XL6_AST_BINARY && xl6CgenBinaryArithmeticIsXValue(ctx, expr)) ||
           (expr->kind == XL6_AST_CONDITIONAL && xl6CgenConditionalIsXValue(ctx, expr));
}

static int xl6CgenReturnTryMultiXValueFieldStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* returnType,
    const char* returnField,
    const XL6_AstNode* expr)
{
    char tempName[64];

    if ( ctx == NULL || returnType == NULL || returnField == NULL || expr == NULL ||
         !xl6CgenIsXValueType(returnType) ) {
        return 0;
    }

    /* class 对外是托管引用，多返回值字段必须构造并装箱，不能写入底层 C 记录。 */
    if ( xl6CgenClassHandleTargetType(ctx, returnType) != NULL &&
         xl6CgenTryClassCtorBoxXValueStmt(ctx, returnType, expr, returnField) ) {
        return 1;
    }
    if ( !xl6CgenReturnCanMultiXValueFieldStmt(ctx, returnType, expr) ) {
        return 0;
    }

    snprintf(tempName, sizeof(tempName), "__xl6_return_value_%d", ctx->tempId++);
    xl6CgenIndent(ctx);
    if ( !(xl6CgenDeclTryXValueArrayLiteralInit(ctx, returnType, tempName, expr) ||
           xl6CgenDeclTryXValueNullCoalesceInit(ctx, returnType, tempName, expr) ||
           xl6CgenDeclTryXValueBinaryInit(ctx, returnType, tempName, expr) ||
           xl6CgenDeclTryXValueConditionalInit(ctx, returnType, tempName, expr)) ) {
        return 0;
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = %s;\n", returnField, tempName);
    return 1;
}

static int xl6CgenReturnTryMultiTypedContainerFieldStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* returnType,
    const char* returnField,
    const XL6_AstNode* expr)
{
    if ( ctx == NULL || returnType == NULL || returnField == NULL || expr == NULL ||
         !xl6CgenIsTypedContainerType(ctx, returnType) ) {
        return 0;
    }

    /* 返回结构体取得独立所有权，函数局部值仍由原作用域正常释放。 */
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", returnField);
    xl6CgenTypedContainerValueFromExpr(ctx, returnType, expr);
    xl6OutputPuts(";\n", ctx->out);
    if ( xl6CgenTypedContainerInitLiteral(ctx, returnType, expr) != NULL ) {
        xl6CgenTypedContainerFillFromLiteral(ctx, returnField, returnType, expr);
    }
    return 1;
}

static int xl6CgenReturnCanMultiScalarFieldStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* returnType,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    int ok;

    if ( ctx == NULL || returnType == NULL || expr == NULL ||
         !xl6CgenDeclCallArgStaticScalarType(returnType) ) {
        return 0;
    }
    if ( xl6CgenDeclCanEmitCallArgTemp(ctx, returnType, expr) ) {
        return 1;
    }
    if ( !xl6CgenDeclIsNullCoalesceExpr(expr) ||
         !xl6CgenDeclCanLowerStaticNullCoalesceType(ctx, returnType) ) {
        return 0;
    }

    left = expr->children[0];
    right = expr->children[1];
    if ( left == NULL || right == NULL || left->kind == XL6_AST_NULL ) {
        return 0;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, left);
    rightType = xl6CgenCloneExprTypeForInference(ctx, right);
    ok = leftType != NULL && rightType != NULL && xl6CgenTypeNodeEqual(leftType, rightType);
    xl6AstFree(leftType);
    xl6AstFree(rightType);
    return ok;
}

static int xl6CgenReturnTryMultiScalarFieldStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* returnType,
    const char* returnField,
    const XL6_AstNode* expr)
{
    char tempName[64];

    if ( ctx == NULL || returnType == NULL || returnField == NULL || expr == NULL ||
         !xl6CgenReturnCanMultiScalarFieldStmt(ctx, returnType, expr) ) {
        return 0;
    }

    snprintf(tempName, sizeof(tempName), "__xl6_return_field_%d", ctx->tempId++);
    if ( !xl6CgenReturnTrySingleValueStmt(ctx, returnType, tempName, expr) ) {
        return 0;
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = %s;\n", returnField, tempName);
    return 1;
}

static void xl6CgenDiscardExpressionStatementTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* tempName)
{
    if ( ctx == NULL || typeNode == NULL || tempName == NULL || tempName[0] == '\0' ) {
        return;
    }

    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xvoUnref(%s); %s = 0; }\n", tempName, tempName, tempName);
        return;
    }

    xl6CgenDiscardTemporaryResultByType(ctx, typeNode, tempName);
}

static int xl6CgenTryDiscardExpressionStatement(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* exprType;
    char tempName[64];
    int id;

    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }

    exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( exprType == NULL ) {
        return 0;
    }
    if ( xl6CTextEquals(exprType->text, "null") ) {
        xl6AstFree(exprType);
        return 0;
    }

    id = ctx->tempId++;
    snprintf(tempName, sizeof(tempName), "__xl6_expr_stmt_%d", id);

    if ( xl6CgenReturnTrySingleValueStmt(ctx, exprType, tempName, expr) ) {
        xl6CgenDiscardExpressionStatementTemp(ctx, exprType, tempName);
        xl6AstFree(exprType);
        return 1;
    }

    if ( xl6CgenIsXValueType(exprType) ) {
        /* 表达式语句会丢弃结果，也必须走语句级 lowering，避免回退到 GNU 语句表达式。 */
        xl6CgenDeclEmitXValueExprInit(ctx, exprType, tempName, expr);
        xl6CgenDiscardExpressionStatementTemp(ctx, exprType, tempName);
        xl6AstFree(exprType);
        return 1;
    }

    xl6AstFree(exprType);
    return 0;
}

static int xl6CgenTryPrintIntrinsicStatement(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    int id;
    int newline;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }
    callee = expr->children[0];
    if ( callee == NULL ||
         callee->kind != XL6_AST_NAME ||
         !xl6CgenIsPrintIntrinsicName(callee->text) ) {
        return 0;
    }

    args = expr->childCount > 1 ? expr->children[1] : NULL;
    id = ctx->tempId++;
    newline = xl6CgenPrintIntrinsicNewline(callee->text);

    /* print/println 的参数由语句自身持有，调用运行时后立即释放。 */
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_print_args_%d = ", id);
    xl6CgenArrayVarargExpr(ctx, args, 0);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xl6_xrt_print(__xl6_print_args_%d, %d);\n", id, newline);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
                    "if (__xl6_print_args_%d != 0) { xvoUnref(__xl6_print_args_%d); __xl6_print_args_%d = 0; }\n",
                    id,
                    id,
                    id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static void xl6CgenExprStatementBody(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_CgenClassExprTempScope classScope;
    int hasClassScope;

    if ( expr != NULL && xl6CgenTryPrintIntrinsicStatement(ctx, expr) ) {
        return;
    }
    if ( expr != NULL && xl6CgenTryKnownCallStatementWithClassArgTemps(ctx, expr) ) {
        return;
    }
    if ( expr != NULL && xl6CgenTryKnownCallStatementWithScalarArgTemps(ctx, expr) ) {
        return;
    }
    hasClassScope = expr != NULL && xl6CgenClassExprTempScopeBegin(ctx, expr, &classScope);
    if ( expr != NULL && xl6CgenTryMemberFunctionStatementInvoke(ctx, expr) ) {
        if ( hasClassScope ) {
            xl6CgenClassExprTempScopeEnd(ctx, &classScope);
        }
        return;
    }
    if ( expr != NULL && xl6CgenTryDiscardExpressionStatement(ctx, expr) ) {
        if ( hasClassScope ) {
            xl6CgenClassExprTempScopeEnd(ctx, &classScope);
        }
        return;
    }

    xl6CgenIndent(ctx);
    if ( expr != NULL ) {
        xl6CgenExpr(ctx, expr);
    }
    xl6OutputPuts(";\n", ctx->out);
    if ( hasClassScope ) {
        xl6CgenClassExprTempScopeEnd(ctx, &classScope);
    }
}

static int xl6CgenTryAssignWithTaskSpawnArgTemps(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_CgenTaskSpawnArgTempScope scope;
    XL6_CgenCallArgTempScope callScope;
    const XL6_AstNode* right;
    int hasCallScope;

    if ( ctx == NULL || node == NULL || node->kind != XL6_AST_ASSIGN || node->childCount < 2 ) {
        return 0;
    }

    if ( !xl6CgenTaskSpawnArgTempScopeBegin(ctx, node, &scope) ) {
        return 0;
    }

    right = node->children[1];
    hasCallScope = right != NULL && right->kind != XL6_AST_CALL &&
                   xl6CgenCallArgTempScopeBegin(ctx, right, &callScope);
    xl6CgenAssignStmt(ctx, node);
    if ( hasCallScope ) {
        xl6CgenCallArgTempScopeEnd(ctx, &callScope);
    }
    xl6CgenTaskSpawnArgTempScopeEnd(ctx, &scope);
    return 1;
}

static int xl6CgenTryAssignWithNestedCallArgTemps(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_CgenCallArgTempScope scope;
    const XL6_AstNode* right;

    if ( ctx == NULL || node == NULL || node->kind != XL6_AST_ASSIGN || node->childCount < 2 ) {
        return 0;
    }
    right = node->children[1];
    if ( right == NULL || right->kind == XL6_AST_CALL ||
         !xl6CgenCallArgTempScopeBegin(ctx, right, &scope) ) {
        return 0;
    }

    xl6CgenAssignStmt(ctx, node);
    xl6CgenCallArgTempScopeEnd(ctx, &scope);
    return 1;
}

static int xl6CgenTryMultiDeclWithTaskSpawnArgTemps(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* right;
    XL6_CgenTaskSpawnArgTempScope scope;

    if ( ctx == NULL || node == NULL || node->kind != XL6_AST_MULTI_VAR_DECL || node->childCount < 2 ) {
        return 0;
    }

    right = node->children[1];
    if ( !xl6CgenTaskSpawnArgTempScopeBegin(ctx, right, &scope) ) {
        return 0;
    }

    xl6CgenMultiDecl(ctx, node);
    xl6CgenTaskSpawnArgTempScopeEnd(ctx, &scope);
    return 1;
}

static int xl6CgenTryMultiAssignWithTaskSpawnArgTemps(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_CgenTaskSpawnArgTempScope scope;

    if ( ctx == NULL || node == NULL || node->kind != XL6_AST_MULTI_ASSIGN || node->childCount < 2 ) {
        return 0;
    }

    if ( !xl6CgenTaskSpawnArgTempScopeBegin(ctx, node, &scope) ) {
        return 0;
    }

    xl6CgenMultiAssign(ctx, node);
    xl6CgenTaskSpawnArgTempScopeEnd(ctx, &scope);
    return 1;
}

static void xl6CgenStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return;
    }

    xl6CgenDebugProbe(ctx, node);

    switch ( node->kind ) {
    case XL6_AST_VAR_DECL:
    case XL6_AST_CONST_DECL:
        xl6CgenDecl(ctx, node);
        if ( xl6CgenNodeMayRaise(node) ) {
            xl6CgenEmitExceptionPropagate(ctx, node);
        }
        break;
    case XL6_AST_MULTI_VAR_DECL:
        if ( !xl6CgenTryMultiDeclWithTaskSpawnArgTemps(ctx, node) ) {
            xl6CgenMultiDecl(ctx, node);
        }
        if ( xl6CgenNodeMayRaise(node) ) {
            xl6CgenEmitExceptionPropagate(ctx, node);
        }
        break;
    case XL6_AST_ASSIGN: {
        const XL6_AstNode* right;
        const XL6_AstNode* scopeExpr;
        XL6_CgenClassExprTempScope classScope;
        int hasClassScope;

        right = node->childCount > 1 ? node->children[1] : NULL;
        /*
            普通赋值自身接管根 class 构造，只预降级其内部临时值。
            复合赋值把右值作为协议参数，必须连右值根构造一起物化。
        */
        scopeExpr = xl6CTextEquals(node->text, "=") ? right : node;
        hasClassScope = scopeExpr != NULL &&
                        xl6CgenClassExprTempScopeBegin(ctx, scopeExpr, &classScope);
        if ( !xl6CgenTryAssignWithTaskSpawnArgTemps(ctx, node) &&
             !xl6CgenTryAssignWithNestedCallArgTemps(ctx, node) ) {
            xl6CgenAssignStmt(ctx, node);
        }
        if ( hasClassScope ) {
            xl6CgenClassExprTempScopeEnd(ctx, &classScope);
        }
        if ( xl6CgenNodeMayRaise(node) ) {
            xl6CgenEmitExceptionPropagate(ctx, node);
        }
        break;
    }
    case XL6_AST_MULTI_ASSIGN:
        if ( !xl6CgenTryMultiAssignWithTaskSpawnArgTemps(ctx, node) ) {
            xl6CgenMultiAssign(ctx, node);
        }
        if ( xl6CgenNodeMayRaise(node) ) {
            xl6CgenEmitExceptionPropagate(ctx, node);
        }
        break;
    case XL6_AST_RETURN: {
        XL6_CgenTaskSpawnArgTempScope taskScope;
        int hasTaskScope;

        hasTaskScope = node->childCount > 0 &&
                       xl6CgenTaskSpawnArgTempScopeBegin(ctx, node, &taskScope);
        if ( ctx->currentAsyncWorker ) {
            xl6CgenAsyncReturnStmt(ctx, node);
            if ( hasTaskScope ) {
                xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
            }
            break;
        }
        if ( node->childCount > 0 ) {
            int id = ctx->tempId++;
            char returnName[64];
            char movedCleanupName[512];
            XL6_CgenClassExprTempScope returnArgScope;
            int hasReturnArgScope;

            movedCleanupName[0] = '\0';
            hasReturnArgScope = node->childCount == 1 &&
                                xl6CgenClassExprTempScopeBegin(ctx, node->children[0], &returnArgScope);

            if ( ctx->currentReturns != NULL && ctx->currentReturns->childCount > 1 &&
                 node->childCount == 1 &&
                 xl6CgenReturnTryMultiCallInto(ctx, node->children[0], id) ) {
                ;
            } else if ( ctx->currentReturns != NULL && ctx->currentReturns->childCount > 1 ) {
                size_t i;

                xl6CgenIndent(ctx);
                if ( ctx->currentReturnFunc != NULL ) {
                    xl6CgenFuncReturnStructName(ctx, ctx->currentReturnFunc);
                } else {
                    xl6CgenFunctionReturnStructName(ctx, ctx->currentReturnParams, ctx->currentReturns);
                }
                xl6OutputPrintf(ctx->out, " __xl6_return_%d;\n", id);
                for ( i = 0; i < node->childCount; ++i ) {
                    const XL6_AstNode* retType = ctx->currentReturns->children[i];
                    XL6_CgenClassExprTempScope fieldArgScope;
                    int hasFieldArgScope;
                    char returnValuePath[96];

                    snprintf(returnValuePath, sizeof(returnValuePath), "__xl6_return_%d.value%u", id, (unsigned)i);
                    hasFieldArgScope = xl6CgenClassExprTempScopeBegin(ctx, node->children[i], &fieldArgScope);
                    if ( !xl6CgenReturnTryMultiTypedContainerFieldStmt(ctx, retType, returnValuePath, node->children[i]) &&
                         !xl6CgenReturnTryMultiXValueFieldStmt(ctx, retType, returnValuePath, node->children[i]) &&
                         !xl6CgenReturnTryMultiScalarFieldStmt(ctx, retType, returnValuePath, node->children[i]) ) {
                        XL6_AstNode* returnClass = xl6CgenClassNodeForType(ctx, retType);
                        XL6_AstNode* ctorClass = NULL;
                        XL6_AstNode* genericCtorType = NULL;
                        int isClassCtor = xl6CgenCallIsClassCtor(ctx, node->children[i], &ctorClass);
                        int emittedCtor;

                        if ( !isClassCtor ) {
                            genericCtorType = xl6CgenGenericClassCtorType(ctx, node->children[i], &ctorClass);
                            isClassCtor = genericCtorType != NULL;
                        }
                        emittedCtor = xl6CgenClassHandleTargetType(ctx, retType) == NULL &&
                                      returnClass != NULL &&
                                      isClassCtor &&
                                      ctorClass != NULL &&
                                      xl6CTextEquals(returnClass->text, ctorClass->text) &&
                                      xl6CgenEmitClassCtorInto(ctx, node->children[i], returnValuePath);
                        xl6AstFree(genericCtorType);

                        if ( emittedCtor ) {
                            ;
                        } else if ( xl6CgenIsXValueType(retType) ) {
                            xl6CgenIndent(ctx);
                            xl6OutputPrintf(ctx->out, "%s = ", returnValuePath);
                            xl6CgenXValueFromExprForTarget(ctx, retType, node->children[i]);
                            xl6OutputPuts(";\n", ctx->out);
                        } else if ( xl6CgenIsFunctionType(retType) ) {
                            xl6CgenIndent(ctx);
                            xl6OutputPrintf(ctx->out, "%s = ", returnValuePath);
                            xl6CgenFunctionValueExpr(ctx, node->children[i], retType);
                            xl6OutputPuts(";\n", ctx->out);
                        } else {
                            xl6CgenIndent(ctx);
                            xl6OutputPrintf(ctx->out, "%s = ", returnValuePath);
                            xl6CgenExpr(ctx, node->children[i]);
                            xl6OutputPuts(";\n", ctx->out);
                        }
                    }
                    if ( xl6CgenIsFunctionType(retType) && xl6CgenExprIsFunctionBorrow(ctx, node->children[i]) ) {
                        xl6CgenIndent(ctx);
                        xl6OutputPrintf(ctx->out, "if (__xl6_return_%d.value%u.retain != 0 && __xl6_return_%d.value%u.env != 0) {\n",
                                        id, (unsigned)i, id, (unsigned)i);
                        ctx->indent += 1;
                        xl6CgenIndent(ctx);
                        xl6OutputPrintf(ctx->out, "__xl6_return_%d.value%u.retain(__xl6_return_%d.value%u.env);\n",
                                        id, (unsigned)i, id, (unsigned)i);
                        ctx->indent -= 1;
                        xl6CgenIndent(ctx);
                        xl6OutputPuts("}\n", ctx->out);
                    }
                    if ( xl6CgenIsSpecialCollectionType(ctx, retType) &&
                         !xl6CgenSpecialCollectionExprIsOwned(ctx, node->children[i]) ) {
                        xl6CgenIndent(ctx);
                        xl6OutputPrintf(ctx->out, "%s = ", returnValuePath);
                        xl6CgenSpecialCollectionRetainCExpr(ctx, retType, returnValuePath);
                        xl6OutputPuts(";\n", ctx->out);
                    }
                    xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, retType, returnValuePath, node->children[i]);
                    if ( hasFieldArgScope ) {
                        xl6CgenClassExprTempScopeEnd(ctx, &fieldArgScope);
                    }
                }
            } else if ( ctx->currentReturnType != NULL &&
                        xl6CgenFuncSingleReturnUsesTypedContainerStorage(ctx, ctx->currentReturnFunc) ) {
                xl6CgenTypedContainerReturnStmt(ctx, ctx->currentReturnType, node->children[0], id);
            } else {
                snprintf(returnName, sizeof(returnName), "__xl6_return_%d", id);
                if ( ctx->currentReturnType == NULL ||
                     !xl6CgenReturnTrySingleValueStmt(ctx, ctx->currentReturnType, returnName, node->children[0]) ) {
                    XL6_AstNode* returnClass = ctx->currentReturnType != NULL ? xl6CgenClassNodeForType(ctx, ctx->currentReturnType) : NULL;
                    XL6_AstNode* ctorClass = NULL;
                    int emittedCtor = 0;

                    xl6CgenIndent(ctx);
                    xl6OutputPuts(ctx->currentReturnType != NULL ? xl6CgenTypeName(ctx, ctx->currentReturnType) : xl6CgenInferExprType(ctx, node->children[0]), ctx->out);
                    if ( returnClass != NULL &&
                         xl6CgenCallIsClassCtor(ctx, node->children[0], &ctorClass) &&
                         xl6CTextEquals(returnClass->text, ctorClass->text) ) {
                        xl6OutputPrintf(ctx->out, " __xl6_return_%d = {0};\n", id);
                        emittedCtor = xl6CgenEmitClassCtorInto(ctx, node->children[0], returnName);
                    } else {
                        xl6OutputPrintf(ctx->out, " __xl6_return_%d = ", id);
                    }
                    if ( emittedCtor ) {
                        ;
                    } else if ( xl6CgenIsXValueType(ctx->currentReturnType) ) {
                        xl6CgenXValueFromExprForTarget(ctx, ctx->currentReturnType, node->children[0]);
                    } else if ( xl6CgenIsFunctionType(ctx->currentReturnType) ) {
                        xl6CgenFunctionValueExpr(ctx, node->children[0], ctx->currentReturnType);
                    } else {
                        xl6CgenExpr(ctx, node->children[0]);
                    }
                    if ( !emittedCtor ) {
                        xl6OutputPuts(";\n", ctx->out);
                    }
                }
                if ( xl6CgenIsFunctionType(ctx->currentReturnType) && xl6CgenExprIsFunctionBorrow(ctx, node->children[0]) ) {
                    snprintf(returnName, sizeof(returnName), "__xl6_return_%d", id);
                    xl6CgenRetainFunctionValue(ctx, returnName);
                }
                if ( xl6CgenIsSpecialCollectionType(ctx, ctx->currentReturnType) &&
                     !xl6CgenSpecialCollectionExprIsOwned(ctx, node->children[0]) ) {
                    snprintf(returnName, sizeof(returnName), "__xl6_return_%d", id);
                    xl6CgenIndent(ctx);
                    xl6OutputPrintf(ctx->out, "%s = ", returnName);
                    xl6CgenSpecialCollectionRetainCExpr(ctx, ctx->currentReturnType, returnName);
                    xl6OutputPuts(";\n", ctx->out);
                }
                snprintf(returnName, sizeof(returnName), "__xl6_return_%d", id);
                xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, ctx->currentReturnType, returnName, node->children[0]);
                xl6CgenClearMovedNativeClassReturnSource(ctx, node->children[0]);
                if ( node->children[0] != NULL &&
                     node->children[0]->kind == XL6_AST_NAME &&
                     xl6CgenClassCleanupIsTracked(ctx, node->children[0]->text) ) {
                    xl6CgenCIdentText(movedCleanupName, sizeof(movedCleanupName), node->children[0]->text);
                }
            }
            if ( hasReturnArgScope ) {
                xl6CgenClassExprTempScopeEnd(ctx, &returnArgScope);
            }
            xl6CgenPrepareMultiStringReturns(ctx, id);
            snprintf(returnName, sizeof(returnName), "__xl6_return_%d", id);
            if ( ctx->currentReturns != NULL && ctx->currentReturns->childCount > 1 &&
                 xl6CgenCurrentFinallyTarget(ctx) != NULL ) {
                xl6CgenPromoteMultiStringReturns(ctx, id);
            }
            if ( xl6CgenEmitPendingFinallyReturn(ctx, returnName, movedCleanupName) ) {
                ;
            } else {
                xl6CgenEmitCleanupsSinceExcept(
                    ctx,
                    0,
                    movedCleanupName[0] != '\0' ? movedCleanupName : NULL);
                xl6CgenIndent(ctx);
                if ( ctx->currentReturns != NULL && ctx->currentReturns->childCount > 1 ) {
                    xl6CgenPromoteMultiStringReturns(ctx, id);
                    xl6CgenIndent(ctx);
                    xl6OutputPrintf(ctx->out, "return __xl6_return_%d;\n", id);
                } else if ( xl6CgenIsStringTypeNode(ctx, ctx->currentReturnType) ) {
                    xl6OutputPrintf(ctx->out, "return xrtTempStringTake((char*)__xl6_return_%d, 0u);\n", id);
                } else {
                    xl6OutputPrintf(ctx->out, "return __xl6_return_%d;\n", id);
                }
            }
        } else {
            if ( !xl6CgenEmitPendingFinallyVoidReturn(ctx) ) {
                xl6CgenEmitCleanupsSince(ctx, 0);
                xl6CgenIndent(ctx);
                xl6OutputPuts("return;\n", ctx->out);
            }
        }
        if ( hasTaskScope ) {
            xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
        }
        break;
    }
    case XL6_AST_EXPR_STMT: {
        const XL6_AstNode* expr;
        XL6_CgenTaskSpawnArgTempScope taskScope;
        int hasTaskScope;

        expr = node->childCount > 0 ? node->children[0] : NULL;
        hasTaskScope = expr != NULL &&
                       xl6CgenTaskSpawnArgTempScopeBegin(ctx, expr, &taskScope);
        xl6CgenExprStatementBody(ctx, expr);
        if ( hasTaskScope ) {
            xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
        }
        if ( xl6CgenNodeMayRaise(node) ) {
            xl6CgenEmitExceptionPropagate(ctx, node);
        }
        break;
    }
    case XL6_AST_IF:
        xl6CgenIf(ctx, node);
        if ( xl6CgenNodeMayRaise(node) ) {
            xl6CgenEmitExceptionPropagate(ctx, node);
        }
        break;
    case XL6_AST_WHILE:
        xl6CgenWhile(ctx, node);
        if ( xl6CgenNodeMayRaise(node) ) {
            xl6CgenEmitExceptionPropagate(ctx, node);
        }
        break;
    case XL6_AST_DO_WHILE:
        xl6CgenDoWhile(ctx, node);
        if ( xl6CgenNodeMayRaise(node) ) {
            xl6CgenEmitExceptionPropagate(ctx, node);
        }
        break;
    case XL6_AST_FOR:
        xl6CgenForIn(ctx, node);
        if ( xl6CgenNodeMayRaise(node) ) {
            xl6CgenEmitExceptionPropagate(ctx, node);
        }
        break;
    case XL6_AST_THROW:
        xl6CgenThrowStmt(ctx, node);
        break;
    case XL6_AST_ASSERT:
        xl6CgenAssertStmt(ctx, node);
        break;
    case XL6_AST_TRY:
        xl6CgenTryStmt(ctx, node);
        break;
    case XL6_AST_BREAK:
        xl6CgenLoopJump(ctx, node, 0);
        break;
    case XL6_AST_CONTINUE:
        xl6CgenLoopJump(ctx, node, 1);
        break;
    case XL6_AST_YIELD:
        xl6CgenYieldStmt(ctx, node);
        if ( xl6CgenNodeMayRaise(node) ) {
            xl6CgenEmitExceptionPropagate(ctx, node);
        }
        break;

    default:
    {
        char message[160];

        snprintf(message, sizeof(message), "C generator does not support statement node: %s", xl6AstKindName(node->kind));
        xl6CgenError(ctx, node, message);
        break;
    }
    }
}
