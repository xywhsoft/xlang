static int xl6CgenCanConditionTemp(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* boolType;
    int ok;

    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }

    /*
        条件语句只能接收 C 标量 bool。
        动态比较、动态逻辑、近似比较、in 等表达式如果直接嵌入 C 条件位置，
        会退回 GNU statement expression。
        这里复用参数 lowering，把它提前生成为语句级临时值，保持生成 C 的可移植性。
    */
    boolType = xl6CgenNewSimpleType("bool", expr);
    if ( boolType == NULL ) {
        return 0;
    }
    ok = xl6CgenDeclCanEmitCallArgTemp(ctx, boolType, expr);
    if ( !ok ) {
        xl6AstFree(boolType);
        return 0;
    }

    xl6AstFree(boolType);
    return 1;
}

static int xl6CgenEmitConditionTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    const char* prefix,
    char* name,
    size_t capacity)
{
    XL6_AstNode* boolType;
    XL6_CgenTaskSpawnArgTempScope taskScope;
    int id;
    int hasTaskScope;

    if ( ctx == NULL || expr == NULL || prefix == NULL || name == NULL || capacity == 0 ) {
        return 0;
    }

    boolType = xl6CgenNewSimpleType("bool", expr);
    if ( boolType == NULL ) {
        return 0;
    }

    id = ctx->tempId++;
    snprintf(name, capacity, "%s%d", prefix, id);
    hasTaskScope = xl6CgenTaskSpawnArgTempScopeBegin(ctx, expr, &taskScope);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "int %s = 0;\n", name);
    if ( !xl6CgenDeclEmitCallArgTemp(ctx, boolType, "int", name, expr) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", name);
        xl6CgenConditionExpr(ctx, expr);
        xl6OutputPuts(";\n", ctx->out);
    }
    if ( hasTaskScope ) {
        xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
    }
    xl6AstFree(boolType);
    return 1;
}

static int xl6CgenTryConditionTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    const char* prefix,
    char* name,
    size_t capacity)
{
    if ( !xl6CgenCanConditionTemp(ctx, expr) ) {
        return 0;
    }
    return xl6CgenEmitConditionTemp(ctx, expr, prefix, name, capacity);
}

static int xl6CgenHasElseBlock(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 0;
}

static void xl6CgenEmitConditionalBranch(
    XL6_CgenContext* ctx,
    const XL6_AstNode* condition,
    const XL6_AstNode* block,
    const char* tempPrefix)
{
    char conditionName[64];
    int hasConditionTemp;
    int hasTaskScope;
    XL6_CgenTaskSpawnArgTempScope taskScope;

    conditionName[0] = '\0';
    hasConditionTemp = 0;
    hasTaskScope = 0;
    if ( condition != NULL ) {
        hasConditionTemp = xl6CgenTryConditionTemp(ctx, condition, tempPrefix, conditionName, sizeof(conditionName));
        if ( !hasConditionTemp ) {
            hasTaskScope = xl6CgenTaskSpawnArgTempScopeBegin(ctx, condition, &taskScope);
        }
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("if (", ctx->out);
    if ( condition != NULL ) {
        if ( hasConditionTemp ) {
            xl6OutputPuts(conditionName, ctx->out);
        } else {
            xl6CgenConditionExpr(ctx, condition);
        }
    }
    xl6OutputPuts(") {\n", ctx->out);
    if ( hasTaskScope ) {
        xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
    }
    ctx->indent += 1;
    if ( block != NULL ) {
        xl6CgenBlock(ctx, block);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPutc('}', ctx->out);
}

static void xl6CgenEmitElseTail(
    XL6_CgenContext* ctx,
    const XL6_AstNode* elseifList,
    size_t index,
    const XL6_AstNode* elseBlock)
{
    if ( elseifList != NULL && index < elseifList->childCount ) {
        const XL6_AstNode* item = elseifList->children[index];
        xl6CgenEmitConditionalBranch(
            ctx,
            item->childCount > 0 ? item->children[0] : NULL,
            item->childCount > 1 ? item->children[1] : NULL,
            "__xl6_elseif_cond_");
        if ( index + 1 < elseifList->childCount || xl6CgenHasElseBlock(elseBlock) ) {
            xl6OutputPuts(" else {\n", ctx->out);
            ctx->indent += 1;
            xl6CgenEmitElseTail(ctx, elseifList, index + 1, elseBlock);
            xl6OutputPutc('\n', ctx->out);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPutc('}', ctx->out);
        }
        return;
    }

    if ( xl6CgenHasElseBlock(elseBlock) ) {
        xl6CgenBlock(ctx, elseBlock);
    }
}

static void xl6CgenIf(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* elseifList;
    const XL6_AstNode* elseBlock;

    elseifList = node->childCount > 2 ? node->children[2] : NULL;
    elseBlock = node->childCount > 3 ? node->children[3] : NULL;

    xl6CgenEmitConditionalBranch(
        ctx,
        node->childCount > 0 ? node->children[0] : NULL,
        node->childCount > 1 ? node->children[1] : NULL,
        "__xl6_if_cond_");

    if ( (elseifList != NULL && elseifList->childCount > 0) || xl6CgenHasElseBlock(elseBlock) ) {
        xl6OutputPuts(" else {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenEmitElseTail(ctx, elseifList, 0, elseBlock);
        xl6OutputPutc('\n', ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPutc('}', ctx->out);
    }
    xl6OutputPutc('\n', ctx->out);
}
