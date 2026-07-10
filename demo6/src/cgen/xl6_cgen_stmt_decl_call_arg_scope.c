static int xl6CgenTaskSpawnArgTempScopeNeeds(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_CgenDeclCallArgTemp* temps;
    int tempCount;
    int tempCapacity;
    int savedTempId;

    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }

    temps = NULL;
    tempCount = 0;
    tempCapacity = 0;
    savedTempId = ctx->tempId;
    xl6CgenDeclCollectTaskSpawnArgTemps(ctx, expr, &temps, &tempCount, &tempCapacity);
    ctx->tempId = savedTempId;
    xl6CgenDeclFreeCallArgTemps(temps, tempCapacity);
    return tempCount > 0;
}

typedef struct XL6_CgenCallArgTempScope {
    XL6_CgenDeclCallArgTemp* temps;
    XL6_CgenExprReplacement* replacements;
    const XL6_CgenExprReplacement* previousReplacements;
    size_t previousReplacementCount;
    int tempCount;
    int tempCapacity;
    int active;
} XL6_CgenCallArgTempScope;

static int xl6CgenCallArgTempScopeBegin(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    XL6_CgenCallArgTempScope* scope)
{
    XL6_CgenExprReplacement* ownReplacements;
    size_t ownCount;
    size_t totalCount;
    int i;

    if ( scope == NULL ) {
        return 0;
    }
    memset(scope, 0, sizeof(*scope));
    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }

    xl6CgenDeclCollectExprCallArgTemps(
        ctx,
        expr,
        &scope->temps,
        &scope->tempCount,
        &scope->tempCapacity);
    if ( scope->tempCount <= 0 ) {
        xl6CgenDeclFreeCallArgTemps(scope->temps, scope->tempCapacity);
        memset(scope, 0, sizeof(*scope));
        return 0;
    }

    ownReplacements = xl6CgenDeclCreateCallArgReplacements(
        ctx,
        scope->temps,
        scope->tempCapacity,
        scope->tempCount);
    if ( ownReplacements == NULL ) {
        xl6CgenDeclFreeCallArgTemps(scope->temps, scope->tempCapacity);
        memset(scope, 0, sizeof(*scope));
        return 0;
    }

    scope->previousReplacements = ctx->exprReplacements;
    scope->previousReplacementCount = ctx->exprReplacementCount;
    ownCount = (size_t)scope->tempCount;
    totalCount = ownCount + scope->previousReplacementCount;
    scope->replacements = (XL6_CgenExprReplacement*)xrtMalloc(
        totalCount * sizeof(XL6_CgenExprReplacement));
    if ( scope->replacements == NULL ) {
        xrtFree(ownReplacements);
        xl6CgenDeclFreeCallArgTemps(scope->temps, scope->tempCapacity);
        memset(scope, 0, sizeof(*scope));
        xl6CgenError(ctx, expr, "out of memory while preparing assignment call arguments");
        return 0;
    }
    memcpy(scope->replacements, ownReplacements, ownCount * sizeof(XL6_CgenExprReplacement));
    if ( scope->previousReplacementCount > 0 ) {
        memcpy(
            scope->replacements + ownCount,
            scope->previousReplacements,
            scope->previousReplacementCount * sizeof(XL6_CgenExprReplacement));
    }
    xrtFree(ownReplacements);

    for ( i = 0; i < scope->tempCapacity; ++i ) {
        const char* cType;

        if ( !scope->temps[i].active ) {
            continue;
        }
        cType = xl6CgenValueStorageCType(ctx, scope->temps[i].typeNode);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", cType, scope->temps[i].name);
        xl6CgenDeclCallArgTempDefaultValue(ctx, scope->temps[i].typeNode);
        xl6OutputPuts(";\n", ctx->out);
        if ( !xl6CgenDeclEmitCallArgTemp(
                 ctx,
                 scope->temps[i].typeNode,
                 cType,
                 scope->temps[i].name,
                 scope->temps[i].expr) ) {
            xl6CgenError(ctx, scope->temps[i].expr, "assignment call argument lowering failed");
        }
    }

    ctx->exprReplacements = scope->replacements;
    ctx->exprReplacementCount = totalCount;
    scope->active = 1;
    return 1;
}

static void xl6CgenCallArgTempScopeEnd(XL6_CgenContext* ctx, XL6_CgenCallArgTempScope* scope)
{
    if ( ctx == NULL || scope == NULL || !scope->active ) {
        return;
    }

    ctx->exprReplacements = scope->previousReplacements;
    ctx->exprReplacementCount = scope->previousReplacementCount;
    xl6CgenDeclDestroyCallArgTemps(ctx, scope->temps, scope->tempCapacity);
    xrtFree(scope->replacements);
    xl6CgenDeclFreeCallArgTemps(scope->temps, scope->tempCapacity);
    memset(scope, 0, sizeof(*scope));
}
