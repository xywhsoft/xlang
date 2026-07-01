static int xl6CgenDeclCallArgTempAppend(
    XL6_CgenContext* ctx,
    XL6_CgenDeclCallArgTemp** temps,
    int* count,
    int* capacity,
    const XL6_AstNode* expr,
    XL6_AstNode* typeNode,
    const char* prefix,
    const char* label)
{
    XL6_CgenDeclCallArgTemp* newTemps;
    int newCapacity;

    if ( ctx == NULL || temps == NULL || count == NULL || capacity == NULL ||
         expr == NULL || typeNode == NULL || prefix == NULL ) {
        xl6AstFree(typeNode);
        return 0;
    }

    if ( *count == *capacity ) {
        newCapacity = *capacity == 0 ? 4 : (*capacity * 2);
        newTemps = (XL6_CgenDeclCallArgTemp*)xrtRealloc(*temps, (size_t)newCapacity * sizeof(XL6_CgenDeclCallArgTemp));
        if ( newTemps == NULL ) {
            xl6AstFree(typeNode);
            xl6CgenError(ctx, expr, label != NULL ? label : "out of memory while preparing call argument temporaries");
            return 0;
        }
        memset(newTemps + *capacity, 0, (size_t)(newCapacity - *capacity) * sizeof(XL6_CgenDeclCallArgTemp));
        *temps = newTemps;
        *capacity = newCapacity;
    }

    (*temps)[*count].active = 1;
    (*temps)[*count].expr = expr;
    (*temps)[*count].typeNode = typeNode;
    snprintf((*temps)[*count].name, sizeof((*temps)[*count].name), "%s%d", prefix, ctx->tempId++);
    *count += 1;
    return 1;
}

static void xl6CgenDeclCollectCallExprArgTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    XL6_CgenDeclCallArgTemp** temps,
    int* count,
    int* capacity)
{
    const XL6_AstNode* args;
    size_t i;

    if ( ctx == NULL || expr == NULL ) {
        return;
    }
    if ( expr->kind != XL6_AST_CALL || expr->childCount < 2 ) {
        return;
    }

    args = expr->children[1];
    for ( i = 0; args != NULL && i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        const XL6_AstNode* argExpr = xl6CgenArgExpr(arg);
        XL6_AstNode* tempType;

        if ( argExpr == NULL ) {
            continue;
        }

        tempType = xl6CgenNewSimpleType("var", argExpr);
        if ( tempType != NULL && xl6CgenDeclCanEmitCallArgTemp(ctx, tempType, argExpr) ) {
            if ( !xl6CgenDeclCallArgTempAppend(ctx, temps, count, capacity, argExpr, tempType, "__xl6_call_arg_", "out of memory while preparing nested call argument temporaries") ) {
                return;
            }
            continue;
        }
        xl6AstFree(tempType);

        /*
            如果当前实参本身不需要临时量，但内部还有 callable 调用，
            继续向下扫描，避免嵌套 kwargs 绕过 replacement。
        */
        xl6CgenDeclCollectCallExprArgTemps(ctx, argExpr, temps, count, capacity);
    }
}

static int xl6CgenDeclCallExprArgsNeedCallArgTemps(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_CgenDeclCallArgTemp* temps;
    int tempCount;
    int tempCapacity;
    int savedTempId;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL ) {
        return 0;
    }

    temps = NULL;
    tempCount = 0;
    tempCapacity = 0;
    savedTempId = ctx->tempId;
    xl6CgenDeclCollectCallExprArgTemps(ctx, expr, &temps, &tempCount, &tempCapacity);
    ctx->tempId = savedTempId;
    xl6CgenDeclFreeCallArgTemps(temps, tempCount);
    return tempCount > 0;
}

static int xl6CgenDeclEmitXValueCallExprWithArgTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* expr,
    const char* valueName)
{
    XL6_CgenDeclCallArgTemp* temps;
    XL6_CgenExprReplacement* replacements;
    const XL6_CgenExprReplacement* prevExprReplacements;
    size_t prevExprReplacementCount;
    int tempCount;
    int tempCapacity;
    int i;

    temps = NULL;
    replacements = NULL;
    tempCount = 0;
    tempCapacity = 0;
    xl6CgenDeclCollectCallExprArgTemps(ctx, expr, &temps, &tempCount, &tempCapacity);
    if ( tempCount <= 0 ) {
        xl6CgenDeclFreeCallArgTemps(temps, tempCount);
        return 0;
    }

    replacements = xl6CgenDeclCreateCallArgReplacements(ctx, temps, tempCount, tempCount);
    if ( replacements == NULL ) {
        xl6CgenDeclFreeCallArgTemps(temps, tempCount);
        return 0;
    }

    for ( i = 0; i < tempCount; ++i ) {
        const char* tempCType;

        tempCType = xl6CgenTypeName(ctx, temps[i].typeNode);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", tempCType, temps[i].name);
        xl6CgenDefaultValue(ctx, temps[i].typeNode);
        xl6OutputPuts(";\n", ctx->out);
        if ( !xl6CgenDeclEmitCallArgTemp(ctx, temps[i].typeNode, tempCType, temps[i].name, temps[i].expr) ) {
            xl6CgenError(ctx, temps[i].expr, "nested call argument lowering failed");
        }
    }

    prevExprReplacements = ctx->exprReplacements;
    prevExprReplacementCount = ctx->exprReplacementCount;
    ctx->exprReplacements = replacements;
    ctx->exprReplacementCount = (size_t)tempCount;

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", valueName);
    xl6CgenXValueFromExprForTarget(ctx, targetType, expr);
    xl6OutputPuts(";\n", ctx->out);

    ctx->exprReplacements = prevExprReplacements;
    ctx->exprReplacementCount = prevExprReplacementCount;
    xrtFree(replacements);
    xl6CgenDeclFreeCallArgTemps(temps, tempCount);
    return 1;
}
