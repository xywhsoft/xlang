static void xl6CgenPushLoopTarget(XL6_CgenContext* ctx, size_t breakMark, size_t continueMark, int breakId, int continueId)
{
    XL6_CgenLoopTarget* newItems;
    size_t newCapacity;

    if ( ctx == NULL ) {
        return;
    }
    if ( ctx->loopTargetCount == ctx->loopTargetCapacity ) {
        newCapacity = ctx->loopTargetCapacity == 0 ? 8 : ctx->loopTargetCapacity * 2;
        newItems = (XL6_CgenLoopTarget*)xrtRealloc(ctx->loopTargets, newCapacity * sizeof(XL6_CgenLoopTarget));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, NULL, "out of memory while growing cgen loop target stack");
            return;
        }
        ctx->loopTargets = newItems;
        ctx->loopTargetCapacity = newCapacity;
    }
    ctx->loopTargets[ctx->loopTargetCount].breakCleanupMark = breakMark;
    ctx->loopTargets[ctx->loopTargetCount].continueCleanupMark = continueMark;
    ctx->loopTargets[ctx->loopTargetCount].breakId = breakId;
    ctx->loopTargets[ctx->loopTargetCount].continueId = continueId;
    ctx->loopTargetCount += 1;
}

static void xl6CgenPopLoopTarget(XL6_CgenContext* ctx)
{
    if ( ctx != NULL && ctx->loopTargetCount > 0 ) {
        ctx->loopTargetCount -= 1;
    }
}

static XL6_CgenLoopTarget* xl6CgenLoopTargetAtLevel(XL6_CgenContext* ctx, int level)
{
    size_t index;

    if ( ctx == NULL || level <= 0 || (size_t)level > ctx->loopTargetCount ) {
        return NULL;
    }
    index = ctx->loopTargetCount - (size_t)level;
    return &ctx->loopTargets[index];
}

static void xl6CgenFreeLoopTargets(XL6_CgenContext* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xrtFree(ctx->loopTargets);
    ctx->loopTargets = NULL;
    ctx->loopTargetCount = 0;
    ctx->loopTargetCapacity = 0;
}
