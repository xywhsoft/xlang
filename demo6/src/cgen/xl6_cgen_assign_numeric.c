static int xl6CgenExprTypeIsFloat(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    int result;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    result = typeNode != NULL && xl6CgenIsFloatTypeName(typeNode->text);
    xl6AstFree(typeNode);
    return result;
}

static void xl6CgenXValueAsI64Expr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6CgenXValueScalarGetExpr(ctx, "long long", "xvoGetInt", expr);
    } else if ( typeNode != NULL && xl6CgenIsFloatTypeName(typeNode->text) ) {
        xl6OutputPuts("(long long)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else {
        xl6CgenExpr(ctx, expr);
    }
    xl6AstFree(typeNode);
}

static void xl6CgenXValueAsF64Expr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6CgenXValueScalarGetExpr(ctx, "double", "xvoGetFloat", expr);
    } else {
        xl6OutputPuts("(double)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    }
    xl6AstFree(typeNode);
}
