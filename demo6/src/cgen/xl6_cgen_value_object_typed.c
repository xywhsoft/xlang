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

static int xl6CgenDeclShouldUseTypedContainer(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr, int declaredAuto)
{
    if ( !xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        return 0;
    }
    if ( !declaredAuto ) {
        return 1;
    }
    return initExpr != NULL && initExpr->kind == XL6_AST_TYPED_LITERAL;
}

static void xl6CgenTypedContainerCreateExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
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
    xl6OutputPuts(", XRT_OBJMODE_LOCAL)", ctx->out);
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

    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }

    callee = expr->children[0];
    if ( callee == NULL ) {
        return 0;
    }

    if ( callee->kind == XL6_AST_NAME ) {
        func = xl6CgenResolveCallFunc(ctx, expr, callee->text);
        return xl6CgenFuncSingleReturnUsesTypedContainerStorage(ctx, func);
    }

    if ( callee->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* functionType = xl6CgenCloneMemberFieldTypeForInference(ctx, callee);
        int result = xl6CgenFunctionTypeSingleReturnUsesTypedContainerStorage(ctx, functionType);

        xl6AstFree(functionType);
        return result;
    }

    return 0;
}

static int xl6CgenTypedContainerCanCopyFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr)
{
    if ( initExpr == NULL || xl6CgenTypedContainerInitLiteral(ctx, typeNode, initExpr) != NULL ) {
        return 0;
    }
    if ( !xl6CgenExprUsesTypedContainerStorage(ctx, initExpr) ) {
        return 0;
    }
    return xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
}

static int xl6CgenTypedContainerCanMoveFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr)
{
    if ( initExpr == NULL || xl6CgenTypedContainerInitLiteral(ctx, typeNode, initExpr) != NULL ) {
        return 0;
    }
    if ( initExpr->kind != XL6_AST_AWAIT && !xl6CgenExprIsTypedContainerReturnCall(ctx, initExpr) ) {
        return 0;
    }
    return xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
}

static void xl6CgenTypedContainerCopyExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6_xrt_typed_array_copy(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6_xrt_typed_list_copy(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6_xrt_typed_set_copy(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6_xrt_typed_dict_copy(", ctx->out);
    } else {
        xl6CgenError(ctx, expr, "typed container copy requires array/list/set/dict");
        xl6OutputPuts("0", ctx->out);
        return;
    }
    xl6CgenEmitXrtTypeDescRef(ctx, itemType);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPutc(')', ctx->out);
}
