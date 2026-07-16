static void xl6CgenNullCoalesceExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    int isXValue;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;

    if ( left != NULL && left->kind == XL6_AST_NULL ) {
        xl6CgenExpr(ctx, right);
        return;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, left);
    rightType = xl6CgenCloneExprTypeForInference(ctx, right);
    isXValue = xl6CgenIsXValueType(leftType) || xl6CgenIsXValueType(rightType);

    if ( isXValue ) {
        if ( left == NULL || !xl6CgenIsXValueType(leftType) ) {
            xl6CgenError(ctx, node, "null coalesce with var result requires the left operand to be var or null");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        } else {
            xl6CgenError(ctx, node, "dynamic var null coalesce requires statement-level lowering");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        }
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return;
    }

    if ( leftType == NULL || rightType == NULL || !xl6CgenTypeNodeEqual(leftType, rightType) ) {
        xl6CgenError(ctx, node, "当前 C 生成 ?? 两侧必须是同一静态类型，或左侧为 null/var");
        xl6OutputPuts("0", ctx->out);
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return;
    }

    xl6CgenError(ctx, node, "static null coalesce requires statement-level lowering");
    xl6OutputPuts("0", ctx->out);

    xl6AstFree(leftType);
    xl6AstFree(rightType);
}
