static void xl6CgenForTypedContainerIterable(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* iterableType)
{
    const XL6_AstNode* targets;
    const XL6_AstNode* iterable;
    const XL6_AstNode* concreteType;
    const char* typeName;
    int id;
    int ownsIter;
    int loweredIter;
    int hasTaskScope;
    char iterName[64];
    size_t iterCleanupMark;
    XL6_CgenTaskSpawnArgTempScope taskScope;

    if ( node->childCount < 3 ) {
        xl6CgenError(ctx, node, "malformed typed container for-in statement");
        return;
    }

    targets = node->children[0];
    iterable = node->children[1];
    if ( targets == NULL || targets->childCount == 0 || targets->childCount > 2 ) {
        xl6CgenError(ctx, targets, "for in 只支持一个或两个迭代变量");
        return;
    }

    id = ctx->tempId++;
    snprintf(iterName, sizeof(iterName), "__xl6_typed_iter_%d", id);
    concreteType = xl6CgenTypedContainerConcreteType(ctx, iterableType);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    iterCleanupMark = ctx->cleanupCount;
    hasTaskScope = iterable != NULL && xl6CgenTaskSpawnArgTempScopeBegin(ctx, iterable, &taskScope);
    loweredIter = iterable != NULL && xl6CgenDeclCanEmitCallArgTemp(ctx, iterableType, iterable);
    if ( loweredIter ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = 0;\n", xl6CgenTypedContainerCType(ctx, iterableType), iterName);
        if ( !xl6CgenDeclEmitCallArgTemp(
                 ctx,
                 iterableType,
                 xl6CgenTypedContainerCType(ctx, iterableType),
                 iterName,
                 iterable) ) {
            xl6CgenError(ctx, iterable, "typed for-in iterable lowering failed");
        }
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, iterableType), iterName);
        xl6CgenExpr(ctx, iterable);
        xl6OutputPuts(";\n", ctx->out);
    }
    if ( hasTaskScope ) {
        xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
    }
    ownsIter = loweredIter || !xl6CgenExprUsesTypedContainerStorage(ctx, iterable);
    if ( ownsIter ) {
        xl6CgenAddTypedContainerCleanup(ctx, iterName, iterableType);
    }

    if ( xl6CTextEquals(typeName, "array") ) {
        xl6CgenForTypedArrayLoop(ctx, node, targets, iterName, iterableType, iterCleanupMark, id, id);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6CgenForTypedListLoop(ctx, node, targets, iterName, iterableType, iterCleanupMark, id, id);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6CgenForTypedSetLoop(ctx, node, targets, iterName, iterableType, iterCleanupMark, id, id);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6CgenForTypedDictLoop(ctx, node, targets, iterName, iterableType, iterCleanupMark, id, id);
    } else {
        xl6CgenError(ctx, iterable, "typed container for-in requires array, list, set or dict");
    }

    xl6CgenEmitCleanupsSince(ctx, iterCleanupMark);
    xl6CgenFreeCleanupsSince(ctx, iterCleanupMark);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_break_%d:\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts(";\n", ctx->out);
}
