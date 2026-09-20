static void xl6CgenWhile(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    int id;
    int hasConditionTemp;
    int hasTaskScope;
    char conditionName[64];
    size_t iterationMark;
    XL6_CgenTaskSpawnArgTempScope taskScope;

    id = ctx->tempId++;
    conditionName[0] = '\0';

    xl6CgenIndent(ctx);
    xl6OutputPuts("while (1) {\n", ctx->out);
    ctx->indent += 1;

    iterationMark = ctx->cleanupCount;
    xl6CgenBeginTempScope(ctx, node);
    xl6CgenPushErrorScope(ctx, iterationMark);
    hasConditionTemp = node->childCount > 0 &&
                       xl6CgenTryConditionTemp(ctx, node->children[0], "__xl6_while_cond_", conditionName, sizeof(conditionName));
    hasTaskScope = 0;
    if ( !hasConditionTemp && node->childCount > 0 ) {
        hasTaskScope = xl6CgenTaskSpawnArgTempScopeBegin(ctx, node->children[0], &taskScope);
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("if (!", ctx->out);
    if ( hasConditionTemp ) {
        xl6OutputPuts(conditionName, ctx->out);
    } else if ( node->childCount > 0 ) {
        xl6CgenConditionExpr(ctx, node->children[0]);
    } else {
        xl6OutputPuts("1", ctx->out);
    }
    xl6OutputPuts(") {\n", ctx->out);
    if ( hasTaskScope ) {
        xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
    }
    ctx->indent += 1;
    xl6CgenEmitCleanupsSince(ctx, iterationMark);
    xl6CgenIndent(ctx);
    xl6OutputPuts("break;\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);

    xl6CgenPushLoopTarget(
        ctx,
        iterationMark,
        iterationMark,
        xl6CgenHirRegion(ctx, node),
        id,
        id
    );
    if ( node->childCount > 1 ) {
        xl6CgenBlock(ctx, node->children[1]);
    }
    xl6CgenPopLoopTarget(ctx);
    xl6CgenCloseErrorScope(ctx, iterationMark);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_continue_%d:\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts(";\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_break_%d:\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts(";\n", ctx->out);
}


static void xl6CgenDoWhile(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    int id;
    int hasConditionTemp;
    int hasTaskScope;
    char conditionName[64];
    size_t bodyMark;
    size_t conditionMark;
    XL6_CgenTaskSpawnArgTempScope taskScope;

    id = ctx->tempId++;
    conditionName[0] = '\0';

    xl6CgenIndent(ctx);
    xl6OutputPuts("while (1) {\n", ctx->out);
    ctx->indent += 1;

    bodyMark = ctx->cleanupCount;
    xl6CgenBeginTempScope(ctx, node);
    xl6CgenPushErrorScope(ctx, bodyMark);
    xl6CgenPushLoopTarget(
        ctx,
        bodyMark,
        bodyMark,
        xl6CgenHirRegion(ctx, node),
        id,
        id
    );
    if ( node->childCount > 0 ) {
        xl6CgenBlock(ctx, node->children[0]);
    }
    xl6CgenPopLoopTarget(ctx);
    xl6CgenCloseErrorScope(ctx, bodyMark);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_continue_%d:\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts(";\n", ctx->out);

    conditionMark = ctx->cleanupCount;
    xl6CgenBeginTempScope(ctx, node);
    xl6CgenPushErrorScope(ctx, conditionMark);
    hasConditionTemp = node->childCount > 1 &&
                       xl6CgenTryConditionTemp(ctx, node->children[1], "__xl6_do_cond_", conditionName, sizeof(conditionName));
    hasTaskScope = 0;
    if ( !hasConditionTemp && node->childCount > 1 ) {
        hasTaskScope = xl6CgenTaskSpawnArgTempScopeBegin(ctx, node->children[1], &taskScope);
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("if (!", ctx->out);
    if ( hasConditionTemp ) {
        xl6OutputPuts(conditionName, ctx->out);
    } else if ( node->childCount > 1 ) {
        xl6CgenConditionExpr(ctx, node->children[1]);
    } else {
        xl6OutputPuts("1", ctx->out);
    }
    xl6OutputPuts(") {\n", ctx->out);
    if ( hasTaskScope ) {
        xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
    }
    ctx->indent += 1;
    xl6CgenEmitCleanupsSince(ctx, conditionMark);
    xl6CgenIndent(ctx);
    xl6OutputPuts("break;\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenCloseErrorScope(ctx, conditionMark);

    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_break_%d:\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts(";\n", ctx->out);
}
