/*
    typed HIR 是源码表达式进入后端时的唯一类型入口。
    后端合成节点没有语义阶段结果，才允许回退到局部推断与缓存。
*/
static XL6_AstNode* xl6CgenCloneExprTypeForInference(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* hirType;
    XL6_AstNode* result;

    if ( expr == NULL ) {
        return NULL;
    }
    hirType = xl6HirExprType(expr);
    if ( ctx != NULL && expr->kind == XL6_AST_NAME ) {
        const XL6_AstNode* narrowType = xl6CgenFindVarNarrowType(ctx, expr->text);
        const XL6_AstNode* liveType = xl6CgenFindVarType(ctx, expr->text);

        /*
         * HIR 保存声明类型 var；局部构造初始化得到的 class 句柄收窄属于
         * 当前控制流事实，必须优先于声明类型，否则协议和虚调用会退回动态路径。
         */
        if ( narrowType != NULL ) {
            result = xl6CgenCloneCanonicalTypeNode(ctx, narrowType);
            if ( result != NULL ) {
                return result;
            }
        }
        /*
            循环目标等后端合成局部量拥有比源码 HIR 更精确的存储类型。
            例如 HIR 中是 var，协议 __next__ 已经把目标物化为 int。
        */
        if ( liveType != NULL && liveType->text != NULL && !xl6CTextEquals(liveType->text, "auto") &&
             !(xl6CTextEquals(expr->text, "self") &&
               hirType != NULL && xl6CTextEquals(hirType->text, "__class_handle")) ) {
            result = xl6CgenCloneCanonicalTypeNode(ctx, liveType);
            if ( result != NULL ) {
                return result;
            }
        }
    }
    /*
        表达式替换只改变 C 端的求值位置，不改变 xlang 语义类型。
        重载解析必须始终优先采用 typed HIR，否则 class 构造临时量会退化成 var，
        进而选错协议重载或错误地落入动态运算路径。
    */
    if ( ctx != NULL && hirType != NULL && hirType->text != NULL &&
         xl6CTextEquals(hirType->text, "var") ) {
        /*
            var 是宽类型，不应覆盖代码生成阶段已经成立的局部类型事实。
            仅当局部推断得到非 var 类型时采用收窄结果；真正动态的表达式仍使用 HIR。
        */
        result = xl6CgenCloneExprTypeForInferenceRaw(ctx, expr);
        if ( result != NULL && result->text != NULL && !xl6CTextEquals(result->text, "var") ) {
            return result;
        }
        xl6AstFree(result);
    }
    if ( hirType != NULL ) {
        result = ctx != NULL ?
                 xl6CgenCloneCanonicalTypeNode(ctx, hirType) :
                 xl6CgenCloneTypeNode(hirType);
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

/*
    控制流收窄服务于协议和重载解析；容器索引最终必须服从变量的真实 C 存储。
    例如 var 保存 class 时仍是 xvalue，协议未命中后应走动态字段桥接。
*/
static XL6_AstNode* xl6CgenCloneExprStorageType(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* declaredType;

    if ( ctx != NULL && expr != NULL && expr->kind == XL6_AST_NAME ) {
        declaredType = xl6CgenFindVarType(ctx, expr->text);
        if ( declaredType != NULL && declaredType->text != NULL ) {
            return xl6CgenCloneCanonicalTypeNode(ctx, declaredType);
        }
    }
    return xl6CgenCloneExprTypeForInference(ctx, expr);
}
