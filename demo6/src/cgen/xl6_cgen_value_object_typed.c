static void xl6CgenXValueFromExprForTarget(XL6_CgenContext* ctx, const XL6_AstNode* targetType, const XL6_AstNode* expr)
{
    if ( xl6CgenIsRuntimeContainerTypeDesc(ctx, targetType) ) {
        xl6OutputPuts("xl6_xrt_bind_type(", ctx->out);
        xl6CgenXValueFromExpr(ctx, expr);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitRuntimeTypeDescRef(ctx, targetType);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    xl6CgenXValueFromExpr(ctx, expr);
}

static void xl6CgenDefaultXValueForTarget(XL6_CgenContext* ctx, const XL6_AstNode* targetType)
{
    if ( xl6CgenIsRuntimeContainerTypeDesc(ctx, targetType) ) {
        xl6OutputPuts("xl6_xrt_bind_type(", ctx->out);
        xl6CgenDefaultValue(ctx, targetType);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitRuntimeTypeDescRef(ctx, targetType);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    xl6CgenDefaultValue(ctx, targetType);
}

static int xl6CgenAwaitExprReturnsOwnedTemporaryCtx(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static const XL6_AstNode* xl6CgenTypedContainerInitLiteral(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr);
static int xl6CgenTypedContainerCanCopyFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr);
static int xl6CgenTypedContainerCanMoveFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr);

static int xl6CgenDeclShouldUseTypedContainer(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr, int declaredAuto)
{
    if ( !xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        return 0;
    }
    if ( !declaredAuto ) {
        return 1;
    }
    if ( initExpr == NULL ) {
        return 0;
    }
    return xl6CgenTypedContainerInitLiteral(ctx, typeNode, initExpr) != NULL ||
           xl6CgenTypedContainerCanCopyFromExpr(ctx, typeNode, initExpr) ||
           xl6CgenTypedContainerCanMoveFromExpr(ctx, typeNode, initExpr);
}

static void xl6CgenTypedContainerCreateExprMode(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* mode)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xrtTypedArrayCreate(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xrtTypedListCreate(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xrtTypedSetCreate(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xrtTypedDictCreate(", ctx->out);
    } else {
        xl6OutputPuts("(void*)(", ctx->out);
    }
    xl6CgenEmitXrtTypeDescRef(ctx, itemType);
    xl6OutputPuts(", ", ctx->out);
    xl6OutputPuts(mode != NULL ? mode : "XRT_OBJMODE_LOCAL", ctx->out);
    xl6OutputPuts(")", ctx->out);
}

static void xl6CgenTypedContainerCreateExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    xl6CgenTypedContainerCreateExprMode(ctx, typeNode, "XRT_OBJMODE_LOCAL");
}

static const XL6_AstNode* xl6CgenTypedContainerInitLiteral(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( initExpr == NULL || typeName == NULL ) {
        return NULL;
    }

    if ( initExpr->kind == XL6_AST_TYPED_LITERAL ) {
        if ( xl6CTextEquals(initExpr->text, typeName) ) {
            return initExpr->childCount > 0 ? initExpr->children[0] : NULL;
        }
        return NULL;
    }
    if ( xl6CTextEquals(typeName, "array") && initExpr->kind == XL6_AST_ARRAY_LITERAL ) {
        return initExpr;
    }
    if ( xl6CTextEquals(typeName, "list") && initExpr->kind == XL6_AST_LIST_LITERAL ) {
        return initExpr;
    }
    if ( xl6CTextEquals(typeName, "set") && initExpr->kind == XL6_AST_SET_LITERAL ) {
        return initExpr;
    }
    if ( xl6CTextEquals(typeName, "dict") && initExpr->kind == XL6_AST_DICT_LITERAL ) {
        return initExpr;
    }
    return NULL;
}

static int xl6CgenFuncSingleReturnUsesTypedContainerStorage(XL6_CgenContext* ctx, const XL6_AstNode* funcNode);

static int xl6CgenFunctionTypeSingleReturnUsesTypedContainerStorage(XL6_CgenContext* ctx, const XL6_AstNode* functionType)
{
    const XL6_AstNode* returns;

    if ( ctx == NULL || !xl6CgenIsFunctionType(functionType) ) {
        return 0;
    }

    returns = xl6CgenFunctionReturnTypes(functionType);
    return returns != NULL &&
           returns->childCount == 1 &&
           xl6CgenIsTypedContainerType(ctx, returns->children[0]);
}

static int xl6CgenTypedContainerExprTypeMatches(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr)
{
    XL6_AstNode* targetType;
    XL6_AstNode* exprType;
    int result;

    targetType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
    exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
    result = targetType != NULL &&
             exprType != NULL &&
             xl6CgenIsTypedContainerType(ctx, exprType) &&
             xl6CgenTypeNodeEqual(targetType, exprType);
    xl6AstFree(targetType);
    xl6AstFree(exprType);
    return result;
}

static int xl6CgenExprIsTypedContainerReturnCall(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    XL6_AstNode* func;
    XL6_AstNode* inferredType;
    int result;

    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }

    callee = expr->children[0];
    if ( callee == NULL ) {
        return 0;
    }

    if ( callee->kind == XL6_AST_NAME ) {
        XL6_AstNode* inferredArgs = NULL;
        func = xl6CgenResolveCallFuncInternal(ctx, expr, callee->text, &inferredArgs, 0);
        xl6AstFree(inferredArgs);
        return xl6CgenFuncSingleReturnUsesTypedContainerStorage(ctx, func);
    }

    if ( callee->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* functionType = xl6CgenCloneMemberFieldTypeForInference(ctx, callee);
        result = xl6CgenFunctionTypeSingleReturnUsesTypedContainerStorage(ctx, functionType);

        xl6AstFree(functionType);
        if ( result ) {
            return 1;
        }
    }

    /*
        这是一个无副作用兜底：成员函数和泛型实例化函数在部分路径下
        不一定能从 callee 本身还原完整函数签名，但 call 表达式的整体
        类型推断已经能给出 array<T>/list<T>/set<T>/dict<T>。只要它是
        call 且结果是静态 typed container，就按“返回 storage”处理。
    */
    if ( callee->kind == XL6_AST_MEMBER ) {
        inferredType = xl6CgenCloneExprTypeForInference(ctx, expr);
        result = xl6CgenIsTypedContainerType(ctx, inferredType);
        xl6AstFree(inferredType);
        return result;
    }
    return 0;
}

static int xl6CgenTypedContainerCanCopyFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr)
{
    if ( initExpr == NULL || xl6CgenTypedContainerInitLiteral(ctx, typeNode, initExpr) != NULL ) {
        return 0;
    }
    if ( initExpr->kind == XL6_AST_AWAIT && !xl6CgenAwaitExprReturnsOwnedTemporaryCtx(ctx, initExpr) ) {
        return xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
    }
    if ( !xl6CgenExprUsesTypedContainerStorage(ctx, initExpr) ) {
        return 0;
    }
    return xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
}

static int xl6CgenTypedContainerCanMoveFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr)
{
    XL6_AstNode* binaryResultType;
    int canMoveBinary;

    if ( initExpr == NULL || xl6CgenTypedContainerInitLiteral(ctx, typeNode, initExpr) != NULL ) {
        return 0;
    }
    binaryResultType = NULL;
    canMoveBinary = initExpr->kind == XL6_AST_BINARY &&
                    xl6CgenTypedContainerBinaryInfo(ctx, initExpr, &binaryResultType, NULL) &&
                    xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
    xl6AstFree(binaryResultType);
    if ( canMoveBinary ) {
        return 1;
    }
    if ( initExpr->kind == XL6_AST_AWAIT ) {
        if ( !xl6CgenAwaitExprReturnsOwnedTemporaryCtx(ctx, initExpr) ) {
            return 0;
        }
    } else if ( !xl6CgenExprIsTypedContainerReturnCall(ctx, initExpr) ) {
        return 0;
    }
    return xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
}

static void xl6CgenTypedContainerCopyExprMode(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr, const char* mode)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6_xrt_typed_array_copy_mode(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6_xrt_typed_list_copy_mode(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6_xrt_typed_set_copy_mode(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6_xrt_typed_dict_copy_mode(", ctx->out);
    } else {
        xl6CgenError(ctx, expr, "typed container copy requires array/list/set/dict");
        xl6OutputPuts("0", ctx->out);
        return;
    }
    xl6CgenEmitXrtTypeDescRef(ctx, itemType);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPuts(", ", ctx->out);
    xl6OutputPuts(mode != NULL ? mode : "XRT_OBJMODE_LOCAL", ctx->out);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenTypedContainerCopyExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr)
{
    xl6CgenTypedContainerCopyExprMode(ctx, typeNode, expr, "XRT_OBJMODE_LOCAL");
}
