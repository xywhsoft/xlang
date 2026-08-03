/* ------------------------------------ typed HIR 后端入口 ------------------------------------ */

static const XL6_HirNode* xl6CgenHirNode(
    const XL6_CgenContext* ctx,
    const XL6_AstNode* source
)
{
    if ( ctx == NULL || ctx->hir == NULL || source == NULL ) {
        return NULL;
    }
    return xl6HirSourceNodeGet(ctx->hir, source);
}

static const XL6_AstNode* xl6CgenHirTarget(
    const XL6_CgenContext* ctx,
    const XL6_AstNode* source
)
{
    const XL6_HirNode* node;
    const XL6_HirNode* target;

    node = xl6CgenHirNode(ctx, source);
    if ( node == NULL || node->targetSymbol == XL6_SYMBOL_ID_INVALID ) {
        return NULL;
    }
    target = xl6HirSymbolNodeGet(ctx->hir, node->targetSymbol);
    return target != NULL ? target->source : NULL;
}

static XL6_HirOwnership xl6CgenHirOwnership(
    const XL6_CgenContext* ctx,
    const XL6_AstNode* source
)
{
    const XL6_HirNode* node = xl6CgenHirNode(ctx, source);

    return node != NULL ? node->ownership : XL6_HIR_OWNERSHIP_NONE;
}

static int xl6CgenHirOwnershipIsPlanned(
    const XL6_CgenContext* ctx,
    const XL6_AstNode* source
)
{
    const XL6_HirNode* node = xl6CgenHirNode(ctx, source);

    return node != NULL && node->ownershipPlanned;
}

static XL6_SemLvalueKind xl6CgenHirLvalueKind(
    const XL6_CgenContext* ctx,
    const XL6_AstNode* source
)
{
    const XL6_HirNode* node = xl6CgenHirNode(ctx, source);

    return node != NULL ? node->lvalueKind : XL6_SEM_LVALUE_NONE;
}

static uint32 xl6CgenHirRegion(
    const XL6_CgenContext* ctx,
    const XL6_AstNode* source
)
{
    const XL6_HirNode* node = xl6CgenHirNode(ctx, source);

    return node != NULL ? node->region : 0;
}

static uint32 xl6CgenCurrentCallableRegion(const XL6_CgenContext* ctx)
{
    const XL6_AstNode* callable;

    if ( ctx == NULL ) {
        return 0;
    }
    callable = ctx->currentReturnFunc;
    if ( callable == NULL && ctx->currentAnonFunc != NULL ) {
        callable = ctx->currentAnonFunc->node;
    }
    return xl6CgenHirRegion(ctx, callable);
}

static int xl6CgenValidateExitPlan(
    XL6_CgenContext* ctx,
    const XL6_AstNode* source,
    XL6_SemExitKind exitKind,
    uint32 backendTargetRegion
)
{
    const XL6_HirNode* node = xl6CgenHirNode(ctx, source);

    if ( node == NULL ||
         node->exitKind != exitKind ||
         node->targetRegion == 0 ) {
        xl6CgenError(ctx, source, "typed HIR exit plan is missing");
        return 0;
    }
    if ( backendTargetRegion != 0 &&
         node->targetRegion != backendTargetRegion ) {
        xl6CgenError(ctx, source, "typed HIR exit target region does not match cgen target stack");
        return 0;
    }
    return 1;
}

static int xl6CgenHirExitDepths(
    const XL6_CgenContext* ctx,
    const XL6_AstNode* source,
    uint32* outScopeDepth,
    uint32* outTargetScopeDepth
)
{
    const XL6_HirNode* node = xl6CgenHirNode(ctx, source);

    if ( node == NULL || node->exitKind == XL6_SEM_EXIT_NONE ) {
        return 0;
    }
    if ( outScopeDepth != NULL ) {
        *outScopeDepth = node->scopeDepth;
    }
    if ( outTargetScopeDepth != NULL ) {
        *outTargetScopeDepth = node->targetScopeDepth;
    }
    return 1;
}
