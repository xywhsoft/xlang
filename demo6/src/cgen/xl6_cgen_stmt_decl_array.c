static int xl6CgenDeclTryXValueArrayLiteralInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);

static const XL6_AstNode* xl6CgenDeclArrayLiteralSource(const XL6_AstNode* initExpr)
{
    if ( initExpr == NULL ) {
        return NULL;
    }
    if ( initExpr->kind == XL6_AST_ARRAY_LITERAL ) {
        return initExpr;
    }
    if ( initExpr->kind == XL6_AST_TYPED_LITERAL &&
         xl6CTextEquals(initExpr->text, "array") &&
         initExpr->childCount > 0 ) {
        return initExpr->children[0];
    }
    return NULL;
}

static int xl6CgenDeclCanUseXValueArrayLiteralInit(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* name;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    name = concreteType != NULL ? concreteType->text : NULL;
    return xl6CTextEquals(name, "var") || xl6CTextEquals(name, "array");
}

static int xl6CgenDeclTryXValueArrayLiteralItemInit(
    XL6_CgenContext* ctx,
    const char* itemName,
    const XL6_AstNode* itemExpr)
{
    XL6_AstNode valueType;

    memset(&valueType, 0, sizeof(valueType));
    valueType.kind = XL6_AST_TYPE;
    valueType.text = "var";

    if ( xl6CgenDeclTryXValueArrayLiteralInit(ctx, &valueType, itemName, itemExpr) ||
         xl6CgenDeclTryXValueNullCoalesceInit(ctx, &valueType, itemName, itemExpr) ||
         xl6CgenDeclTryXValueBinaryInit(ctx, &valueType, itemName, itemExpr) ||
         xl6CgenDeclTryXValueConditionalInit(ctx, &valueType, itemName, itemExpr) ) {
        return 1;
    }

    xl6OutputPrintf(ctx->out, "xvalue* %s = ", itemName);
    xl6CgenXValueFromExprForTarget(ctx, &valueType, itemExpr);
    xl6OutputPuts(";\n", ctx->out);
    return 1;
}

static int xl6CgenDeclTryXValueArrayLiteralInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* literal;
    size_t i;

    literal = xl6CgenDeclArrayLiteralSource(initExpr);
    if ( ctx == NULL || typeNode == NULL || cName == NULL ||
         literal == NULL ||
         !xl6CgenDeclCanUseXValueArrayLiteralInit(ctx, typeNode) ) {
        return 0;
    }

    xl6OutputPrintf(ctx->out, "xvalue* %s = xrtValueArray();\n", cName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s == 0) { %s = xrtValueNull(); }\n", cName, cName);
    if ( xl6CgenIsRuntimeContainerTypeDesc(ctx, typeNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xl6_xrt_bind_type(%s, ", cName, cName);
        xl6CgenEmitRuntimeTypeDescRef(ctx, typeNode);
        xl6OutputPuts(");\n", ctx->out);
    }

    for ( i = 0; i < literal->childCount; ++i ) {
        char itemName[64];

        snprintf(itemName, sizeof(itemName), "__xl6_array_item_%d", ctx->tempId++);
        xl6CgenIndent(ctx);
        xl6CgenDeclTryXValueArrayLiteralItemInit(ctx, itemName, literal->children[i]);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (!xl6_xrt_value_array_append(%s, %s, true) && %s != 0) {\n",
                        cName,
                        itemName,
                        itemName);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xrtValueRelease(%s);\n", itemName);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    }
    return 1;
}
