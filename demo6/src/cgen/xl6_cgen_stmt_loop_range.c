static int xl6CgenIsRangeCall(const XL6_AstNode* node)
{
    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount < 2 ) {
        return 0;
    }
    return node->children[0] != NULL &&
           node->children[0]->kind == XL6_AST_NAME &&
           xl6CTextEquals(node->children[0]->text, "range");
}

static const XL6_AstNode* xl6CgenRangeArgExpr(const XL6_AstNode* args, int index)
{
    const XL6_AstNode* arg;

    if ( args == NULL || index < 0 || args->childCount <= (size_t)index ) {
        return NULL;
    }
    arg = args->children[index];
    if ( arg == NULL || arg->childCount == 0 ) {
        return NULL;
    }
    return arg->children[0];
}

static void xl6CgenRangePart(XL6_CgenContext* ctx, const XL6_AstNode* args, int index, const char* defaultText)
{
    const XL6_AstNode* expr;

    expr = xl6CgenRangeArgExpr(args, index);
    if ( expr != NULL ) {
        xl6CgenExpr(ctx, expr);
        return;
    }
    xl6OutputPuts(defaultText, ctx->out);
}

static void xl6CgenForRangeTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* args,
    int index,
    const char* defaultText,
    const char* name)
{
    const XL6_AstNode* expr;
    XL6_CgenTaskSpawnArgTempScope taskScope;
    int hasTaskScope;

    expr = xl6CgenRangeArgExpr(args, index);
    hasTaskScope = expr != NULL && xl6CgenTaskSpawnArgTempScopeBegin(ctx, expr, &taskScope);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "long long %s = ", name);
    if ( expr != NULL ) {
        xl6CgenExpr(ctx, expr);
    } else {
        xl6OutputPuts(defaultText, ctx->out);
    }
    xl6OutputPuts(";\n", ctx->out);

    if ( hasTaskScope ) {
        xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
    }
}

static void xl6CgenRangeContainsExpr(XL6_CgenContext* ctx, const XL6_AstNode* value, const XL6_AstNode* rangeCall)
{
    const XL6_AstNode* args;

    if ( !xl6CgenIsRangeCall(rangeCall) || rangeCall->childCount < 2 ) {
        xl6CgenError(ctx, rangeCall, "in 右侧必须是 range(...)");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    args = rangeCall->children[1];
    if ( args == NULL || args->childCount == 0 || args->childCount > 3 ) {
        xl6CgenError(ctx, rangeCall, "range 参数数量必须是 1、2 或 3");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    xl6OutputPuts("xl6_range_contains_i64(", ctx->out);
    xl6CgenExpr(ctx, value);
    xl6OutputPuts(", ", ctx->out);
    if ( args->childCount == 1 ) {
        xl6OutputPuts("0", ctx->out);
    } else {
        xl6CgenRangePart(ctx, args, 0, "0");
    }
    xl6OutputPuts(", ", ctx->out);
    if ( args->childCount == 1 ) {
        xl6CgenRangePart(ctx, args, 0, "0");
    } else {
        xl6CgenRangePart(ctx, args, 1, "0");
    }
    xl6OutputPuts(", ", ctx->out);
    if ( args->childCount >= 3 ) {
        xl6CgenRangePart(ctx, args, 2, "1");
    } else {
        xl6OutputPuts("1", ctx->out);
    }
    xl6OutputPutc(')', ctx->out);
}
