static int xl6CgenPushFinallyTarget(XL6_CgenContext* ctx, size_t cleanupMark, int id)
{
    XL6_CgenFinallyTarget* items;
    size_t capacity;

    if ( ctx == NULL ) {
        return 0;
    }
    if ( ctx->finallyTargetCount == ctx->finallyTargetCapacity ) {
        capacity = ctx->finallyTargetCapacity == 0 ? 8 : ctx->finallyTargetCapacity * 2;
        items = (XL6_CgenFinallyTarget*)xrtRealloc(
            ctx->finallyTargets,
            capacity * sizeof(XL6_CgenFinallyTarget));
        if ( items == NULL ) {
            xl6CgenError(ctx, NULL, "out of memory while growing finally target stack");
            return 0;
        }
        ctx->finallyTargets = items;
        ctx->finallyTargetCapacity = capacity;
    }
    ctx->finallyTargets[ctx->finallyTargetCount].cleanupMark = cleanupMark;
    ctx->finallyTargets[ctx->finallyTargetCount].loopTargetCount = ctx->loopTargetCount;
    ctx->finallyTargets[ctx->finallyTargetCount].id = id;
    ctx->finallyTargets[ctx->finallyTargetCount].inFinally = 0;
    ctx->finallyTargetCount += 1;
    return 1;
}

static void xl6CgenPopFinallyTarget(XL6_CgenContext* ctx)
{
    if ( ctx != NULL && ctx->finallyTargetCount > 0 ) {
        ctx->finallyTargetCount -= 1;
    }
}

static XL6_CgenFinallyTarget* xl6CgenCurrentFinallyTarget(XL6_CgenContext* ctx)
{
    if ( ctx == NULL || ctx->finallyTargetCount == 0 ) {
        return NULL;
    }
    return &ctx->finallyTargets[ctx->finallyTargetCount - 1];
}

static void xl6CgenFreeFinallyTargets(XL6_CgenContext* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xrtFree(ctx->finallyTargets);
    ctx->finallyTargets = NULL;
    ctx->finallyTargetCount = 0;
    ctx->finallyTargetCapacity = 0;
}
