/* 静态泛型容器装箱。 */
static int xl6CgenTypedContainerXValueFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* rawTypeNode;
    const XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;
    int usesStorage;
    int returnCall;

    usesStorage = xl6CgenExprUsesTypedContainerStorage(ctx, expr);
    returnCall = !usesStorage && xl6CgenExprIsTypedContainerReturnCall(ctx, expr);
    if ( !usesStorage && !returnCall ) {
        return 0;
    }

    rawTypeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    typeNode = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, rawTypeNode));
    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( typeName == NULL || itemType == NULL ) {
        xl6CgenError(ctx, expr, "typed container boxing requires a concrete item type");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        xl6AstFree(rawTypeNode);
        return 1;
    }

    if ( returnCall ) {
        xl6CgenError(ctx, expr, "typed container return boxing requires statement-level lowering");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        xl6AstFree(rawTypeNode);
        return 1;
    }

    if ( !xl6CTextEquals(typeName, "array") &&
         !xl6CTextEquals(typeName, "list") &&
         !xl6CTextEquals(typeName, "set") &&
         !xl6CTextEquals(typeName, "dict") ) {
        xl6CgenError(ctx, expr, "typed container boxing requires array/list/set/dict");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        xl6AstFree(rawTypeNode);
        return 1;
    }

    xl6OutputPuts("xl6_xrt_typed_container_box_ref(", ctx->out);
    xl6CgenEmitRuntimeTypeDescRef(ctx, typeNode);
    xl6OutputPuts(", (void*)(", ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPuts("))", ctx->out);

    xl6AstFree(rawTypeNode);
    return 1;
}
