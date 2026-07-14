static XL6_AstNode* xl6CgenCloneSingleGenericArgType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* baseName)
{
    const XL6_AstNode* concrete;
    const XL6_AstNode* args;

    concrete = xl6CgenSubstituteGenericType(ctx, typeNode);
    concrete = xl6CgenResolveAliasType(ctx, concrete);
    if ( concrete == NULL || !xl6CTextEquals(concrete->text, baseName) ) {
        return NULL;
    }

    args = concrete->childCount > 0 && concrete->children[0] != NULL &&
           concrete->children[0]->kind == XL6_AST_LIST ? concrete->children[0] : NULL;
    if ( args == NULL || args->childCount == 0 ) {
        return xl6CgenNewSimpleType("null", concrete);
    }
    return xl6CgenCloneCanonicalTypeNode(ctx, args->children[0]);
}

static XL6_AstNode* xl6CgenCloneAwaitResultType(XL6_CgenContext* ctx, const XL6_AstNode* awaitNode)
{
    XL6_AstNode* futureType;
    XL6_AstNode* result;

    if ( awaitNode == NULL || awaitNode->childCount == 0 ) {
        return NULL;
    }

    futureType = xl6CgenCloneExprTypeForInference(ctx, awaitNode->children[0]);
    result = xl6CgenCloneSingleGenericArgType(ctx, futureType, "future");
    if ( result == NULL ) {
        result = xl6CgenCloneSingleGenericArgType(ctx, futureType, "task");
    }
    xl6AstFree(futureType);
    return result;
}
