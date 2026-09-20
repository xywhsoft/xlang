/*
    typed HIR 是源码表达式进入后端时的唯一类型入口。
    后端合成节点没有语义阶段结果，才允许回退到局部推断与缓存。
*/
static int xl6CgenExprHasStaticValueType(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_HirNode* hirNode;

    if ( expr == NULL ) {
        return 0;
    }
    hirNode = xl6CgenHirNode(ctx, expr);

    /* 后端合成节点没有 HIR 映射，仍由局部 lowering 决定类型。 */
    return hirNode == NULL || hirNode->type != XL6_HIR_TYPE_INVALID;
}

static XL6_AstNode* xl6CgenCloneExprTypeForInference(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_HirNode* hirNode;
    XL6_AstNode* hirType;
    XL6_AstNode* result;

    if ( expr == NULL ) {
        return NULL;
    }
    hirNode = xl6CgenHirNode(ctx, expr);
    hirType = NULL;
    if ( hirNode != NULL ) {
        if ( hirNode->type == XL6_HIR_TYPE_INVALID ) {
            char message[320];

            snprintf(
                message,
                sizeof(message),
                "typed HIR source expression '%s' text='%s' has no static type; symbol=%llu, targetSymbol=%llu",
                xl6AstKindName(expr->kind),
                expr->text != NULL ? expr->text : "",
                (unsigned long long)hirNode->symbol,
                (unsigned long long)hirNode->targetSymbol
            );
            xl6CgenError(ctx, expr, message);
            return NULL;
        }
        hirType = xl6HirTypeCloneAst(&ctx->hir->types, hirNode->type);
        if ( hirType == NULL ) {
            xl6CgenError(ctx, expr, "typed HIR source expression type cannot be restored");
            return NULL;
        }
    }
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
                xl6AstFree(hirType);
                return result;
            }
        }
        /*
            循环目标等后端合成局部量拥有比源码 HIR 更精确的存储类型。
            例如 HIR 中是 var，协议 __next__ 已经把目标物化为 int。
        */
        /*
         * typed HIR 保存当前求值点的语义类型，其中包含 nullable 流收窄。
         * liveType 是 C 存储类型，只能在 HIR 没有给出具体类型时作为后备。
         */
        if ( hirType != NULL && hirType->text != NULL &&
             !xl6CTextEquals(hirType->text, "auto") &&
             !xl6CTextEquals(hirType->text, "var") ) {
            result = xl6CgenCloneCanonicalTypeNode(ctx, hirType);
            if ( result != NULL ) {
                xl6AstFree(hirType);
                return result;
            }
        }
        if ( liveType != NULL && liveType->text != NULL && !xl6CTextEquals(liveType->text, "auto") &&
             !(xl6CTextEquals(expr->text, "self") &&
               hirType != NULL && xl6CTextEquals(hirType->text, "__class_handle")) ) {
            result = xl6CgenCloneCanonicalTypeNode(ctx, liveType);
            if ( result != NULL ) {
                xl6AstFree(hirType);
                return result;
            }
        }
    }
    /*
        HIR 中的源节点只能使用 HIR 类型；这里失败说明前后端契约已损坏，
        不能用 AST 推断掩盖问题。后端合成节点没有 HIR 映射，才进入下方兜底。
    */
    if ( hirNode != NULL ) {
        result = ctx != NULL ?
                 xl6CgenCloneCanonicalTypeNode(ctx, hirType) :
                 xl6CgenCloneTypeNode(hirType);
        xl6AstFree(hirType);
        if ( result != NULL ) {
            return result;
        }
        xl6CgenError(ctx, expr, "typed HIR source expression type cannot be canonicalized");
        return NULL;
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
    例如 var 保存 class 时仍是 xvalue*，协议未命中后应走动态字段桥接。
*/
static XL6_AstNode* xl6CgenCloneExprStorageType(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_HirTypeId storageType;
    const XL6_AstNode* declaredType;

    if ( ctx != NULL && ctx->hir != NULL && expr != NULL ) {
        storageType = xl6HirSourceStorageType(ctx->hir, expr);
        if ( storageType != XL6_HIR_TYPE_INVALID ) {
            XL6_AstNode* typeNode = xl6HirTypeCloneAst(&ctx->hir->types, storageType);

            if ( typeNode != NULL ) {
                return typeNode;
            }
        }
    }

    if ( ctx != NULL && expr != NULL && expr->kind == XL6_AST_NAME ) {
        declaredType = xl6CgenFindVarType(ctx, expr->text);
        if ( declaredType != NULL && declaredType->text != NULL ) {
            return xl6CgenCloneCanonicalTypeNode(ctx, declaredType);
        }
    }
    return xl6CgenCloneExprTypeForInference(ctx, expr);
}

/*
    动态值在控制流收窄后，语义类型可以是 int/bool 等具体类型，
    但它在 ABI 中仍然以 xvalue* 保存。所有解箱判断必须读取 storageType，
    避免前端、needs 扫描和最终 lowering 对同一个表达式得出不同结论。
*/
static int xl6CgenExprUsesXValueStorage(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* storageType;
    int result;

    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }
    storageType = xl6CgenCloneExprStorageType(ctx, expr);
    result = xl6CgenIsXValueType(storageType) ||
             xl6CgenBinaryArithmeticIsXValue(ctx, expr) ||
             xl6CgenConditionalIsXValue(ctx, expr);
    xl6AstFree(storageType);
    return result;
}

/*
    这是静态标量与 xvalue* 之间唯一的上下文桥接判定。
    声明、赋值、实参、返回值和 needs 扫描必须共用它，不能各自维护类型名单。
*/
static int xl6CgenExprNeedsXValueScalarBridge(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* concreteTarget;
    XL6_AstNode* exprType;
    const XL6_AstNode* concreteExpr;
    const char* targetName;
    int result;

    if ( ctx == NULL || targetType == NULL || expr == NULL ) {
        return 0;
    }
    concreteTarget = xl6CgenResolveAliasType(
        ctx,
        xl6CgenSubstituteGenericType(ctx, targetType));
    targetName = concreteTarget != NULL ? concreteTarget->text : NULL;
    if ( !xl6CgenIsIntegerTypeName(targetName) &&
         !xl6CgenIsFloatTypeName(targetName) &&
         !xl6CgenIsBoolTypeName(targetName) &&
         !xl6CTextEquals(targetName, "string") &&
         !xl6CTextEquals(targetName, "time") &&
         !xl6CTextEquals(targetName, "point") ) {
        return 0;
    }
    /*
        storageType 为 var 不代表这里还要再次解箱。
        `if value is string` 分支内，NAME lowering 已按语义类型生成
        xl6_runtime_value_as_string；外层若再套 xvalue* 转换会形成双重解箱。
    */
    exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
    concreteExpr = xl6CgenResolveAliasType(ctx, exprType);
    result = xl6CgenIsXValueType(concreteExpr) ||
             xl6CgenBinaryArithmeticIsXValue(ctx, expr) ||
             xl6CgenConditionalIsXValue(ctx, expr);
    xl6AstFree(exprType);
    return result;
}
