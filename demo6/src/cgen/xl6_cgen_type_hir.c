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
    /*
        表达式替换只改变 C 端的求值位置，不改变 xlang 语义类型。
        重载解析必须始终优先采用 typed HIR，否则 class 构造临时量会退化成 var，
        进而选错协议重载或错误地落入动态运算路径。
    */
    if ( xl6HirExprType(expr) != NULL ) {
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
