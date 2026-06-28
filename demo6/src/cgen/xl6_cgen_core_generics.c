static const XL6_AstNode* xl6CgenTypeSymbolArgs(const XL6_AstNode* typeNode)
{
    if ( typeNode == NULL || typeNode->childCount == 0 ||
         typeNode->children[0] == NULL || typeNode->children[0]->kind != XL6_AST_LIST ) {
        return NULL;
    }
    return typeNode->children[0];
}

static int xl6CgenTypeSymbolEqual(XL6_CgenContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b)
{
    const XL6_AstNode* aa;
    const XL6_AstNode* ba;
    size_t i;

    a = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, a));
    b = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, b));
    if ( a == NULL || b == NULL ) {
        return a == b;
    }
    if ( a->text == NULL || b->text == NULL || !xl6CTextEquals(a->text, b->text) ) {
        return 0;
    }

    aa = xl6CgenTypeSymbolArgs(a);
    ba = xl6CgenTypeSymbolArgs(b);
    if ( aa == NULL || ba == NULL ) {
        return aa == ba;
    }
    if ( aa->childCount != ba->childCount ) {
        return 0;
    }
    for ( i = 0; i < aa->childCount; ++i ) {
        if ( !xl6CgenTypeSymbolEqual(ctx, aa->children[i], ba->children[i]) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6CgenTypeArgsEqual(XL6_CgenContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b)
{
    size_t i;

    if ( xl6CgenTypeNodeEqual(a, b) ) {
        return 1;
    }
    if ( a == NULL || b == NULL ) {
        return a == b;
    }
    if ( a->kind == XL6_AST_LIST && b->kind == XL6_AST_LIST ) {
        if ( a->childCount != b->childCount ) {
            return 0;
        }
        for ( i = 0; i < a->childCount; ++i ) {
            if ( !xl6CgenTypeSymbolEqual(ctx, a->children[i], b->children[i]) ) {
                return 0;
            }
        }
        return 1;
    }
    return xl6CgenTypeSymbolEqual(ctx, a, b);
}

static int xl6CgenHasGenericInstance(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* typeArgs)
{
    size_t i;

    if ( ctx == NULL || func == NULL || typeArgs == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->genericInstanceCount; ++i ) {
        if ( ctx->genericInstances[i].func == func && xl6CgenTypeArgsEqual(ctx, ctx->genericInstances[i].typeArgs, typeArgs) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenAddGenericInstanceEx(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* typeArgs, int ownsTypeArgs)
{
    XL6_CgenGenericInstance* newItems;
    size_t newCapacity;
    XL6_AstNode* canonicalArgs;

    if ( ctx == NULL || func == NULL || typeArgs == NULL ) {
        if ( ownsTypeArgs ) {
            xl6AstFree((XL6_AstNode*)typeArgs);
        }
        return 0;
    }
    canonicalArgs = xl6CgenCloneCanonicalTypeNode(ctx, typeArgs);
    if ( ownsTypeArgs ) {
        xl6AstFree((XL6_AstNode*)typeArgs);
    }
    if ( canonicalArgs == NULL ) {
        xl6CgenError(ctx, func, "泛型实例类型实参规范化失败");
        return 0;
    }
    if ( xl6CgenHasGenericInstance(ctx, func, canonicalArgs) ) {
        xl6AstFree(canonicalArgs);
        return 0;
    }
    if ( ctx->genericInstanceCount == ctx->genericInstanceCapacity ) {
        newCapacity = ctx->genericInstanceCapacity == 0 ? 16 : ctx->genericInstanceCapacity * 2;
        newItems = (XL6_CgenGenericInstance*)xrtRealloc(ctx->genericInstances, newCapacity * sizeof(XL6_CgenGenericInstance));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, func, "C 生成泛型实例表内存不足");
            xl6AstFree(canonicalArgs);
            return 0;
        }
        ctx->genericInstances = newItems;
        ctx->genericInstanceCapacity = newCapacity;
    }
    ctx->genericInstances[ctx->genericInstanceCount].func = func;
    ctx->genericInstances[ctx->genericInstanceCount].typeArgs = canonicalArgs;
    ctx->genericInstances[ctx->genericInstanceCount].ownsTypeArgs = 1;
    ctx->genericInstanceCount += 1;
    return 1;
}

static void xl6CgenAddGenericInstance(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* typeArgs)
{
    (void)xl6CgenAddGenericInstanceEx(ctx, func, typeArgs, 0);
}

static void xl6CgenAddOwnedGenericInstance(XL6_CgenContext* ctx, const XL6_AstNode* func, XL6_AstNode* typeArgs)
{
    (void)xl6CgenAddGenericInstanceEx(ctx, func, typeArgs, 1);
}

static void xl6CgenFreeGenericInstances(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->genericInstanceCount; ++i ) {
        if ( ctx->genericInstances[i].ownsTypeArgs ) {
            xl6AstFree((XL6_AstNode*)ctx->genericInstances[i].typeArgs);
        }
    }
    xrtFree(ctx->genericInstances);
    ctx->genericInstances = NULL;
    ctx->genericInstanceCount = 0;
    ctx->genericInstanceCapacity = 0;
}

static int xl6CgenHasGenericClassInstance(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode)
{
    size_t i;

    if ( ctx == NULL || classNode == NULL || typeNode == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->genericClassInstanceCount; ++i ) {
        if ( ctx->genericClassInstances[i].classNode == classNode &&
             xl6CgenTypeNodeEqual(ctx->genericClassInstances[i].typeNode, typeNode) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenAddGenericClassInstance(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode)
{
    XL6_CgenGenericClassInstance* newItems;
    size_t newCapacity;
    XL6_AstNode* canonicalType;

    if ( ctx == NULL || classNode == NULL || typeNode == NULL ) {
        return;
    }
    canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
    if ( canonicalType == NULL ) {
        xl6CgenError(ctx, classNode, "泛型 class 实例类型规范化失败");
        return;
    }
    if ( xl6CgenHasGenericClassInstance(ctx, classNode, canonicalType) ) {
        xl6AstFree(canonicalType);
        return;
    }
    if ( ctx->genericClassInstanceCount == ctx->genericClassInstanceCapacity ) {
        newCapacity = ctx->genericClassInstanceCapacity == 0 ? 8 : ctx->genericClassInstanceCapacity * 2;
        newItems = (XL6_CgenGenericClassInstance*)xrtRealloc(ctx->genericClassInstances, newCapacity * sizeof(XL6_CgenGenericClassInstance));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, classNode, "泛型 class 实例表内存不足");
            xl6AstFree(canonicalType);
            return;
        }
        ctx->genericClassInstances = newItems;
        ctx->genericClassInstanceCapacity = newCapacity;
    }
    ctx->genericClassInstances[ctx->genericClassInstanceCount].classNode = classNode;
    ctx->genericClassInstances[ctx->genericClassInstanceCount].typeNode = canonicalType;
    ctx->genericClassInstanceCount += 1;
}
