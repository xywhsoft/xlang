/*
    typed HIR 是源码表达式进入后端时的唯一类型入口。
    后端合成节点没有语义阶段结果，才允许回退到局部推断与缓存。
*/
static XL6_AstNode* xl6CgenCloneExprTypeForInference(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* result;

    if ( expr == NULL ) {
        return NULL;
    }
    if ( xl6HirExprType(expr) != NULL && (ctx == NULL || ctx->exprReplacements == NULL) ) {
        result = ctx != NULL ?
                 xl6CgenCloneCanonicalTypeNode(ctx, xl6HirExprType(expr)) :
                 xl6CgenCloneTypeNode(xl6HirExprType(expr));
        if ( result != NULL ) {
            return result;
        }
    }
    if ( ctx != NULL && ctx->exprReplacements == NULL ) {
        result = xl6CgenTypeCacheGet(ctx, expr);
        if ( result != NULL ) {
            return result;
        }
    }

    result = xl6CgenCloneExprTypeForInferenceRaw(ctx, expr);
    if ( result != NULL && ctx != NULL && ctx->exprReplacements == NULL ) {
        xl6CgenTypeCachePut(ctx, expr, result);
    }
    return result;
}
