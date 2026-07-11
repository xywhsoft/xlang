static XL6_AstNode* xl6CgenCloneClassProtocolReturnTypeForCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiverType,
    const char* protocolName,
    const XL6_AstNode* call)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* classNode;
    const XL6_AstNode* method;
    const XL6_AstNode* returns;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    XL6_AstNode* result;

    concreteType = xl6CgenResolveAliasType(ctx, receiverType);
    if ( xl6CgenRefTargetType(ctx, concreteType) != NULL ) {
        concreteType = xl6CgenRefTargetType(ctx, concreteType);
    }
    if ( concreteType == NULL || concreteType->text == NULL || protocolName == NULL || call == NULL ) {
        return NULL;
    }

    classNode = xl6CgenFindClass(ctx, concreteType->text);
    if ( classNode == NULL ) {
        return NULL;
    }

    method = xl6CgenResolveClassFunc(ctx, classNode, call, protocolName, NULL);
    returns = method != NULL && method->childCount > 2 ? method->children[2] : NULL;
    if ( returns == NULL || returns->childCount != 1 ) {
        return NULL;
    }
    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( concreteType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = concreteType->children[0];
    }
    result = xl6CgenCloneCanonicalTypeNode(ctx, returns->children[0]);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    return result;
}

static XL6_AstNode* xl6CgenCloneClassProtocolReturnTypeForIndex(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const char* protocolName,
    const XL6_AstNode* valueExpr)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode* call;
    XL6_AstNode* result;
    size_t i;

    if ( node == NULL ||
         (node->kind != XL6_AST_INDEX && node->kind != XL6_AST_SAFE_INDEX) ||
         node->childCount < 2 ) {
        return NULL;
    }
    receiver = node->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    call = xl6CgenProtocolTempCallNew(node);
    if ( call == NULL ) {
        xl6AstFree(receiverType);
        return NULL;
    }
    for ( i = 0; i < xl6CgenIndexArgCount(node); ++i ) {
        if ( !xl6CgenProtocolTempCallAddArg(call, xl6CgenIndexArgAt(node, i)) ) {
            xl6CgenProtocolTempCallFree(call);
            xl6AstFree(receiverType);
            return NULL;
        }
    }
    if ( valueExpr != NULL && !xl6CgenProtocolTempCallAddArg(call, valueExpr) ) {
        xl6CgenProtocolTempCallFree(call);
        xl6AstFree(receiverType);
        return NULL;
    }

    result = xl6CgenCloneClassProtocolReturnTypeForCall(ctx, receiverType, protocolName, call);
    xl6CgenProtocolTempCallFree(call);
    xl6AstFree(receiverType);
    return result;
}
