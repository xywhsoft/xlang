static int xl6CgenDeclTryXValueExprInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    return xl6CgenDeclTryXValueArrayLiteralInit(ctx, typeNode, cName, initExpr) ||
           xl6CgenDeclTryXValueNullCoalesceInit(ctx, typeNode, cName, initExpr) ||
           xl6CgenDeclTryXValueBinaryInit(ctx, typeNode, cName, initExpr) ||
           xl6CgenDeclTryXValueConditionalInit(ctx, typeNode, cName, initExpr);
}

static void xl6CgenDeclEmitXValueExprInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    if ( xl6CgenDeclTryXValueExprInit(ctx, typeNode, cName, initExpr) ) {
        return;
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue %s = ", cName);
    xl6CgenXValueFromExprForTarget(ctx, typeNode, initExpr);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenDeclEmitXValueExprAssign(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    char tempName[64];

    if ( ctx == NULL || cName == NULL ) {
        return;
    }

    snprintf(tempName, sizeof(tempName), "__xl6_nested_value_%d", ctx->tempId++);
    if ( xl6CgenDeclTryXValueExprInit(ctx, typeNode, tempName, initExpr) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = %s;\n", cName, tempName);
        return;
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", cName);
    xl6CgenXValueFromExprForTarget(ctx, typeNode, initExpr);
    xl6OutputPuts(";\n", ctx->out);
}
