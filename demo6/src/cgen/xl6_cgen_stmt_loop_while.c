static void xl6CgenWhile(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    int id;
    int hasConditionTemp;
    int hasTaskScopeCondition;
    int hasTaskScope;
    char conditionName[64];
    XL6_CgenTaskSpawnArgTempScope taskScope;

    id = ctx->tempId++;
    conditionName[0] = '\0';
    hasConditionTemp = node->childCount > 0 && xl6CgenCanConditionTemp(ctx, node->children[0]);
    hasTaskScopeCondition = !hasConditionTemp &&
                            node->childCount > 0 &&
                            xl6CgenTaskSpawnArgTempScopeNeeds(ctx, node->children[0]);
    if ( hasConditionTemp || hasTaskScopeCondition ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("while (1) {\n", ctx->out);
        ctx->indent += 1;
        hasTaskScope = 0;
        if ( hasConditionTemp ) {
            xl6CgenEmitConditionTemp(ctx, node->children[0], "__xl6_while_cond_", conditionName, sizeof(conditionName));
        } else {
            hasTaskScope = xl6CgenTaskSpawnArgTempScopeBegin(ctx, node->children[0], &taskScope);
        }
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (!", ctx->out);
        if ( hasConditionTemp ) {
            xl6OutputPuts(conditionName, ctx->out);
        } else {
            xl6CgenConditionExpr(ctx, node->children[0]);
        }
        xl6OutputPuts(") {\n", ctx->out);
        if ( hasTaskScope ) {
            xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
        }
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("break;\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenPushLoopTarget(ctx, ctx->cleanupCount, ctx->cleanupCount, id, id);
        if ( node->childCount > 1 ) {
            xl6CgenBlock(ctx, node->children[1]);
        }
        xl6CgenPopLoopTarget(ctx);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_continue_%d:\n", id);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_break_%d:\n", id);
        xl6CgenIndent(ctx);
        xl6OutputPuts(";\n", ctx->out);
        return;
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("while (", ctx->out);
    if ( node->childCount > 0 ) {
        xl6CgenConditionExpr(ctx, node->children[0]);
    }
    xl6OutputPuts(") {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenPushLoopTarget(ctx, ctx->cleanupCount, ctx->cleanupCount, id, id);
    if ( node->childCount > 1 ) {
        xl6CgenBlock(ctx, node->children[1]);
    }
    xl6CgenPopLoopTarget(ctx);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_continue_%d:\n", id);
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
    int hasTaskScopeCondition;
    int hasTaskScope;
    char conditionName[64];
    XL6_CgenTaskSpawnArgTempScope taskScope;

    id = ctx->tempId++;
    conditionName[0] = '\0';
    hasConditionTemp = node->childCount > 1 && xl6CgenCanConditionTemp(ctx, node->children[1]);
    hasTaskScopeCondition = !hasConditionTemp &&
                            node->childCount > 1 &&
                            xl6CgenTaskSpawnArgTempScopeNeeds(ctx, node->children[1]);
    if ( hasConditionTemp || hasTaskScopeCondition ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("while (1) {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenPushLoopTarget(ctx, ctx->cleanupCount, ctx->cleanupCount, id, id);
        if ( node->childCount > 0 ) {
            xl6CgenBlock(ctx, node->children[0]);
        }
        xl6CgenPopLoopTarget(ctx);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_continue_%d:\n", id);
        hasTaskScope = 0;
        if ( hasConditionTemp ) {
            xl6CgenEmitConditionTemp(ctx, node->children[1], "__xl6_do_cond_", conditionName, sizeof(conditionName));
        } else {
            hasTaskScope = xl6CgenTaskSpawnArgTempScopeBegin(ctx, node->children[1], &taskScope);
        }
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (!", ctx->out);
        if ( hasConditionTemp ) {
            xl6OutputPuts(conditionName, ctx->out);
        } else {
            xl6CgenConditionExpr(ctx, node->children[1]);
        }
        xl6OutputPuts(") {\n", ctx->out);
        if ( hasTaskScope ) {
            xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
        }
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("break;\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_break_%d:\n", id);
        xl6CgenIndent(ctx);
        xl6OutputPuts(";\n", ctx->out);
        return;
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("do {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenPushLoopTarget(ctx, ctx->cleanupCount, ctx->cleanupCount, id, id);
    if ( node->childCount > 0 ) {
        xl6CgenBlock(ctx, node->children[0]);
    }
    xl6CgenPopLoopTarget(ctx);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_continue_%d:\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} while (", ctx->out);
    if ( node->childCount > 1 ) {
        xl6CgenConditionExpr(ctx, node->children[1]);
    }
    xl6OutputPuts(");\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_break_%d:\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts(";\n", ctx->out);
}
