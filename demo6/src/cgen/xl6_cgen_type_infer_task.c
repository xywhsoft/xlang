static const XL6_AstNode* xl6CgenTaskCallArgExpr(const XL6_AstNode* call)
{
    const XL6_AstNode* args;
    const XL6_AstNode* arg;

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    arg = args != NULL && args->childCount == 1 ? args->children[0] : NULL;
    return arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenTaskCallArgExprAt(const XL6_AstNode* call, size_t index)
{
    const XL6_AstNode* args;
    const XL6_AstNode* arg;

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    arg = args != NULL && index < args->childCount ? args->children[index] : NULL;
    return arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
}

static int xl6CgenIsTaskNamespaceMemberCall(const XL6_AstNode* call, const char** outName)
{
    const XL6_AstNode* member;
    const XL6_AstNode* receiver;

    if ( outName != NULL ) {
        *outName = NULL;
    }
    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return 0;
    }
    member = call->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME || !xl6CTextEquals(receiver->text, "task") ) {
        return 0;
    }
    if ( !xl6CTextEquals(member->text, "spawn") &&
         !xl6CTextEquals(member->text, "cancel") &&
         !xl6CTextEquals(member->text, "all") &&
         !xl6CTextEquals(member->text, "race") ) {
        return 0;
    }
    if ( outName != NULL ) {
        *outName = member->text;
    }
    return 1;
}

static XL6_AstNode* xl6CgenCloneFutureValueTypeFromType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    XL6_AstNode* result;

    result = xl6CgenCloneSingleGenericArgType(ctx, typeNode, "future");
    if ( result == NULL ) {
        result = xl6CgenCloneSingleGenericArgType(ctx, typeNode, "task");
    }
    return result;
}

static XL6_AstNode* xl6CgenCloneTaskArrayItemFutureValueType(XL6_CgenContext* ctx, const XL6_AstNode* argExpr)
{
    XL6_AstNode* argType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;
    XL6_AstNode* result;
    size_t i;

    if ( argExpr == NULL ) {
        return NULL;
    }

    if ( argExpr->kind == XL6_AST_ARRAY_LITERAL ) {
        if ( argExpr->childCount == 0 ) {
            return NULL;
        }
        result = NULL;
        for ( i = 0; i < argExpr->childCount; ++i ) {
            XL6_AstNode* itemType = xl6CgenCloneExprTypeForInference(ctx, argExpr->children[i]);
            XL6_AstNode* valueType = xl6CgenCloneFutureValueTypeFromType(ctx, itemType);
            xl6AstFree(itemType);
            if ( valueType == NULL ) {
                xl6AstFree(result);
                return NULL;
            }
            if ( result == NULL ) {
                result = valueType;
            } else if ( xl6CgenTypeNodeEqual(result, valueType) ) {
                xl6AstFree(valueType);
            } else {
                xl6AstFree(result);
                xl6AstFree(valueType);
                return NULL;
            }
        }
        return result;
    }

    argType = xl6CgenCloneExprTypeForInference(ctx, argExpr);
    concreteType = xl6CgenSubstituteGenericType(ctx, argType);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    if ( concreteType == NULL || !xl6CTextEquals(concreteType->text, "array") ) {
        xl6AstFree(argType);
        return NULL;
    }
    args = xl6CgenTypeNodeGenericArgs(concreteType);
    if ( args == NULL || args->childCount != 1 ) {
        xl6AstFree(argType);
        return NULL;
    }
    result = xl6CgenCloneFutureValueTypeFromType(ctx, args->children[0]);
    xl6AstFree(argType);
    return result;
}

static XL6_AstNode* xl6CgenCloneTaskCallType(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const char* name;
    const XL6_AstNode* argExpr;
    const XL6_AstNode* returns;
    XL6_AstNode* functionType;
    XL6_AstNode* valueType;
    XL6_AstNode* arrayType;

    if ( !xl6CgenIsTaskNamespaceMemberCall(call, &name) ) {
        return NULL;
    }
    if ( xl6CTextEquals(name, "cancel") ) {
        return xl6CgenNewSimpleType("bool", call);
    }
    if ( xl6CTextEquals(name, "spawn") ) {
        argExpr = xl6CgenTaskCallArgExprAt(call, 0);
        functionType = xl6CgenCloneExprTypeForInference(ctx, argExpr);
        returns = xl6CgenFunctionReturnTypes(functionType);
        if ( !xl6CgenIsFunctionType(functionType) || returns == NULL || returns->childCount != 1 ) {
            xl6AstFree(functionType);
            return NULL;
        }
        valueType = xl6CgenCloneTypeNode(returns->children[0]);
        xl6AstFree(functionType);
        return xl6CgenNewGenericTypeNode("future", valueType, call);
    }
    argExpr = xl6CgenTaskCallArgExpr(call);
    valueType = xl6CgenCloneTaskArrayItemFutureValueType(ctx, argExpr);
    if ( valueType == NULL ) {
        return NULL;
    }
    if ( xl6CTextEquals(name, "race") ) {
        return xl6CgenNewGenericTypeNode("future", valueType, call);
    }
    arrayType = xl6CgenNewGenericTypeNode("array", valueType, call);
    return xl6CgenNewGenericTypeNode("future", arrayType, call);
}

