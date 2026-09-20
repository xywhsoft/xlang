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
    int usesXValue;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    usesXValue = xl6CgenIsXValueType(typeNode) || xl6CgenExprIsXValueBorrow(ctx, expr);
    if ( usesXValue ) {
        xl6CgenXValueScalarGetExpr(ctx, "long long", "xl6_xrt_value_int", expr);
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
    int usesXValue;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    usesXValue = xl6CgenIsXValueType(typeNode) || xl6CgenExprIsXValueBorrow(ctx, expr);
    if ( usesXValue ) {
        xl6CgenXValueScalarGetExpr(ctx, "double", "xl6_xrt_value_float", expr);
    } else {
        xl6OutputPuts("(double)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    }
    xl6AstFree(typeNode);
}

static int xl6CgenIsNumericCompoundOp(const char* op)
{
    return xl6CTextEquals(op, "+=") || xl6CTextEquals(op, "-=") ||
           xl6CTextEquals(op, "*=") || xl6CTextEquals(op, "/=") ||
           xl6CTextEquals(op, "%=") || xl6CTextEquals(op, "&=") ||
           xl6CTextEquals(op, "|=") || xl6CTextEquals(op, "^=") ||
           xl6CTextEquals(op, "<<=") || xl6CTextEquals(op, ">>=");
}

static int xl6CgenStaticNumericCompoundXValueStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* left,
    const char* op,
    const XL6_AstNode* right)
{
    const XL6_AstNode* targetType;
    XL6_AstNode* rightType;
    int targetInteger;
    int targetFloat;
    int rightUsesXValue;

    if ( ctx == NULL || left == NULL || left->kind != XL6_AST_NAME ||
         right == NULL || !xl6CgenIsNumericCompoundOp(op) ) {
        return 0;
    }
    targetType = xl6CgenFindVarType(ctx, left->text);
    rightType = xl6CgenCloneExprTypeForInference(ctx, right);
    targetInteger = targetType != NULL && xl6CgenIsIntegerTypeName(targetType->text);
    targetFloat = targetType != NULL && xl6CgenIsFloatTypeName(targetType->text);
    rightUsesXValue = xl6CgenIsXValueType(rightType) || xl6CgenExprIsXValueBorrow(ctx, right);
    if ( (!targetInteger && !targetFloat) || !rightUsesXValue ) {
        xl6AstFree(rightType);
        return 0;
    }

    xl6CgenIndent(ctx);
    xl6CgenExpr(ctx, left);
    xl6OutputPrintf(ctx->out, " %s ", op);
    if ( targetFloat ) {
        xl6CgenXValueAsF64Expr(ctx, right);
    } else {
        xl6CgenXValueAsI64Expr(ctx, right);
    }
    xl6OutputPuts(";\n", ctx->out);
    xl6AstFree(rightType);
    return 1;
}
