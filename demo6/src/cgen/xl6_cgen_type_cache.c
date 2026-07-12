/*
    代码生成类型推断缓存。

    AST 节点会在 needs 扫描、重载匹配和正式生成阶段多次访问。
    局部变量表每次变化都会推进 scopeEpoch，使旧条目自然失效；
    泛型、class 与闭包上下文则直接进入缓存键，避免跨实例复用错误类型。
*/

static size_t xl6CgenTypeCacheMix(size_t value)
{
    value ^= value >> 16;
    value *= (size_t)0x7feb352dU;
    value ^= value >> 15;
    value *= (size_t)0x846ca68bU;
    value ^= value >> 16;
    return value;
}

static size_t xl6CgenTypeCacheHash(
    const XL6_AstNode* expr,
    const XL6_AstNode* genericParams,
    const XL6_AstNode* genericArgs,
    const XL6_AstNode* currentClass,
    const XL6_AstNode* currentNativeClass,
    const XL6_CgenAnonFunc* currentAnonFunc,
    unsigned long long scopeEpoch)
{
    size_t hash;

    hash = xl6CgenTypeCacheMix((size_t)(uintptr_t)expr);
    hash ^= xl6CgenTypeCacheMix((size_t)(uintptr_t)genericParams + (size_t)0x9e3779b9U);
    hash ^= xl6CgenTypeCacheMix((size_t)(uintptr_t)genericArgs + (size_t)0x85ebca6bU);
    hash ^= xl6CgenTypeCacheMix((size_t)(uintptr_t)currentClass + (size_t)0xc2b2ae35U);
    hash ^= xl6CgenTypeCacheMix((size_t)(uintptr_t)currentNativeClass + (size_t)0x27d4eb2fU);
    hash ^= xl6CgenTypeCacheMix((size_t)(uintptr_t)currentAnonFunc + (size_t)0x165667b1U);
    hash ^= xl6CgenTypeCacheMix((size_t)scopeEpoch ^ (size_t)(scopeEpoch >> 32));
    return hash;
}

static int xl6CgenTypeCacheKeyEquals(
    const XL6_CgenTypeCacheEntry* entry,
    const XL6_CgenContext* ctx,
    const XL6_AstNode* expr)
{
    return entry != NULL && entry->expr == expr &&
           entry->genericParams == ctx->currentGenericParams &&
           entry->genericArgs == ctx->currentGenericArgs &&
           entry->currentClass == ctx->currentClass &&
           entry->currentNativeClass == ctx->currentNativeClass &&
           entry->currentAnonFunc == ctx->currentAnonFunc &&
           entry->scopeEpoch == ctx->typeScopeEpoch;
}

static size_t xl6CgenTypeCacheFindSlot(
    const XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    XL6_CgenTypeCacheEntry* entries,
    size_t capacity)
{
    size_t index;

    index = xl6CgenTypeCacheHash(
        expr,
        ctx->currentGenericParams,
        ctx->currentGenericArgs,
        ctx->currentClass,
        ctx->currentNativeClass,
        ctx->currentAnonFunc,
        ctx->typeScopeEpoch) & (capacity - 1);
    while ( entries[index].expr != NULL && !xl6CgenTypeCacheKeyEquals(&entries[index], ctx, expr) ) {
        index = (index + 1) & (capacity - 1);
    }
    return index;
}

static int xl6CgenTypeCacheGrow(XL6_CgenContext* ctx)
{
    XL6_CgenTypeCacheEntry* entries;
    size_t capacity;
    size_t i;

    capacity = ctx->typeCacheCapacity == 0 ? 256 : ctx->typeCacheCapacity * 2;
    entries = (XL6_CgenTypeCacheEntry*)xrtMalloc(capacity * sizeof(XL6_CgenTypeCacheEntry));
    if ( entries == NULL ) {
        return 0;
    }
    memset(entries, 0, capacity * sizeof(XL6_CgenTypeCacheEntry));

    for ( i = 0; i < ctx->typeCacheCapacity; ++i ) {
        XL6_CgenTypeCacheEntry* oldEntry = &ctx->typeCache[i];
        size_t index;

        if ( oldEntry->expr == NULL ) {
            continue;
        }
        index = xl6CgenTypeCacheHash(
            oldEntry->expr,
            oldEntry->genericParams,
            oldEntry->genericArgs,
            oldEntry->currentClass,
            oldEntry->currentNativeClass,
            oldEntry->currentAnonFunc,
            oldEntry->scopeEpoch) & (capacity - 1);
        while ( entries[index].expr != NULL ) {
            index = (index + 1) & (capacity - 1);
        }
        entries[index] = *oldEntry;
    }

    xrtFree(ctx->typeCache);
    ctx->typeCache = entries;
    ctx->typeCacheCapacity = capacity;
    return 1;
}

static XL6_AstNode* xl6CgenTypeCacheGet(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    size_t index;

    if ( ctx == NULL || expr == NULL || ctx->typeCacheCapacity == 0 ) {
        return NULL;
    }
    index = xl6CgenTypeCacheFindSlot(ctx, expr, ctx->typeCache, ctx->typeCacheCapacity);
    if ( ctx->typeCache[index].expr == NULL ) {
        return NULL;
    }
    return xl6CgenCloneTypeNode(ctx->typeCache[index].type);
}

static void xl6CgenTypeCachePut(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* type)
{
    XL6_AstNode* copy;
    size_t index;

    if ( ctx == NULL || expr == NULL || type == NULL ) {
        return;
    }
    if ( ctx->typeCacheCapacity == 0 ||
         (ctx->typeCacheCount + 1) * 2 >= ctx->typeCacheCapacity ) {
        if ( !xl6CgenTypeCacheGrow(ctx) ) {
            return;
        }
    }
    index = xl6CgenTypeCacheFindSlot(ctx, expr, ctx->typeCache, ctx->typeCacheCapacity);
    if ( ctx->typeCache[index].expr != NULL ) {
        return;
    }
    copy = xl6CgenCloneTypeNode(type);
    if ( copy == NULL ) {
        return;
    }
    ctx->typeCache[index].expr = expr;
    ctx->typeCache[index].genericParams = ctx->currentGenericParams;
    ctx->typeCache[index].genericArgs = ctx->currentGenericArgs;
    ctx->typeCache[index].currentClass = ctx->currentClass;
    ctx->typeCache[index].currentNativeClass = ctx->currentNativeClass;
    ctx->typeCache[index].currentAnonFunc = ctx->currentAnonFunc;
    ctx->typeCache[index].scopeEpoch = ctx->typeScopeEpoch;
    ctx->typeCache[index].type = copy;
    ctx->typeCacheCount += 1;
}

static void xl6CgenTypeCacheFree(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->typeCacheCapacity; ++i ) {
        xl6AstFree(ctx->typeCache[i].type);
    }
    xrtFree(ctx->typeCache);
    ctx->typeCache = NULL;
    ctx->typeCacheCount = 0;
    ctx->typeCacheCapacity = 0;
}
