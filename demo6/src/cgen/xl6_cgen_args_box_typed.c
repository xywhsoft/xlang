/* 静态泛型容器装箱。 */
static int xl6CgenTypedContainerXValueFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* rawTypeNode;
    const XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const XL6_AstNode* itemRecordType;
    XL6_AstNode* itemClass;
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
    itemRecordType = xl6CgenClassHandleTargetType(ctx, itemType);
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

    xl6OutputPuts("xl6_xrt_bind_type(", ctx->out);
    itemClass = itemRecordType != NULL && itemRecordType->text != NULL ?
                xl6CgenFindClass(ctx, itemRecordType->text) :
                (itemType != NULL && itemType->text != NULL ? xl6CgenFindClass(ctx, itemType->text) : NULL);
    if ( xl6CTextEquals(typeName, "array") &&
         itemClass != NULL &&
         xl6CgenClassHasValueDerive(itemClass) ) {
        xl6OutputPuts("xl6_xrt_typed_array_to_xvalue_map(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts(", (xl6_xrt_typed_item_box_fn)", ctx->out);
        xl6CgenClassAutoToValueFormatSymbol(ctx->out, itemClass, NULL);
        xl6OutputPuts(itemRecordType != NULL ? ", true" : ", false", ctx->out);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6_xrt_typed_array_to_xvalue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6_xrt_typed_list_to_xvalue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6_xrt_typed_set_to_xvalue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6_xrt_typed_dict_to_xvalue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else {
        xl6CgenError(ctx, expr, "typed container boxing requires array/list/set/dict");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        xl6AstFree(rawTypeNode);
        return 1;
    }
    xl6OutputPuts(", ", ctx->out);
    xl6CgenEmitRuntimeTypeDescRef(ctx, typeNode);
    xl6OutputPutc(')', ctx->out);

    xl6AstFree(rawTypeNode);
    return 1;
}
