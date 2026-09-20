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
        xl6OutputPuts("xrtValueNull()", ctx->out);
        xl6AstFree(rawTypeNode);
        return 1;
    }

    if ( returnCall ) {
        xl6CgenError(ctx, expr, "typed container return boxing requires statement-level lowering");
        xl6OutputPuts("xrtValueNull()", ctx->out);
        xl6AstFree(rawTypeNode);
        return 1;
    }

    if ( !xl6CTextEquals(typeName, "array") &&
         !xl6CTextEquals(typeName, "list") &&
         !xl6CTextEquals(typeName, "set") &&
         !xl6CTextEquals(typeName, "dict") ) {
        xl6CgenError(ctx, expr, "typed container boxing requires array/list/set/dict");
        xl6OutputPuts("xrtValueNull()", ctx->out);
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

/*
    JSON/XSON 需要遍历容器内容，不能接收仅保留对象身份的 typed-container handle。
    普通 var 实参仍走上面的引用装箱，避免无条件复制容器。
*/
static int xl6CgenTypedContainerMaterializedXValueFromExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    const char* format
)
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
        xl6CgenError(ctx, expr, "typed container materialization requires a concrete item type");
        xl6OutputPuts("xrtValueNull()", ctx->out);
        xl6AstFree(rawTypeNode);
        return 1;
    }
    if ( returnCall ) {
        xl6CgenError(ctx, expr, "typed container return materialization requires statement-level lowering");
        xl6OutputPuts("xrtValueNull()", ctx->out);
        xl6AstFree(rawTypeNode);
        return 1;
    }
    if ( !xl6CTextEquals(typeName, "array") &&
         !xl6CTextEquals(typeName, "list") &&
         !xl6CTextEquals(typeName, "set") &&
         !xl6CTextEquals(typeName, "dict") ) {
        xl6CgenError(ctx, expr, "typed container materialization requires array/list/set/dict");
        xl6OutputPuts("xrtValueNull()", ctx->out);
        xl6AstFree(rawTypeNode);
        return 1;
    }

    xl6CgenTypedContainerExprToXValueExpr(ctx, typeNode, expr, format);
    xl6AstFree(rawTypeNode);
    return 1;
}

static int xl6CgenPersistenceValueArgNeedsMaterialization(
    const XL6_AstNode* func,
    const XL6_AstNode* param,
    const char** format
)
{
    const char* name;

    if ( func == NULL || param == NULL || !xl6CTextEquals(param->text, "value") ) {
        return 0;
    }
    name = xl6CgenCallableName(func);
    if ( format != NULL ) { *format = NULL; }
    return xl6CgenPersistenceStringifyName(name, format) ||
           xl6CgenPersistenceWriteName(name, format);
}

static void xl6CgenKnownFixedParamArgExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* func,
    const XL6_AstNode* param,
    const XL6_AstNode* expr
)
{
    const char* format = NULL;

    if ( func != NULL && func->kind == XL6_AST_EXTERN_FUNC &&
         !xl6CgenIsInternalStdRuntimeExtern(func) &&
         param != NULL && param->childCount > 0 &&
         xl6CgenIsFunctionType(param->children[0]) &&
         xl6CgenEmitCAbiCallbackArgument(ctx, param->children[0], expr) ) {
        return;
    }
    if ( xl6CgenPersistenceValueArgNeedsMaterialization(func, param, &format) &&
         xl6CgenTypedContainerMaterializedXValueFromExpr(ctx, expr, format) ) {
        return;
    }
    xl6CgenFixedParamArgExpr(ctx, param, expr);
}
