static int xl6CgenTryKnownCallStatementWithNestedClassArgTemps(XL6_CgenContext* ctx,
                                                               const XL6_AstNode* func,
                                                               const XL6_AstNode* args)
{
    XL6_CgenDeclClassArgTemp* exprTemps = NULL;
    XL6_CgenExprReplacement* exprReplacements;
    const XL6_CgenExprReplacement* prevExprReplacements;
    size_t prevExprReplacementCount;
    int exprTempCount = 0;
    int exprTempCapacity = 0;
    size_t i;

    if ( ctx == NULL || func == NULL || args == NULL ) {
        return 0;
    }

    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg != NULL && arg->childCount > 0 ) {
            xl6CgenDeclCollectClassArgTempsInExpr(ctx, arg->children[0], &exprTemps, &exprTempCount, &exprTempCapacity);
        }
    }
    if ( exprTempCount <= 0 ) {
        xl6CgenDeclFreeClassArgTemps(exprTemps, exprTempCount);
        return 0;
    }

    exprReplacements = xl6CgenDeclCreateExprReplacements(ctx, exprTemps, exprTempCount);
    if ( exprReplacements == NULL ) {
        xl6CgenDeclFreeClassArgTemps(exprTemps, exprTempCount);
        return 0;
    }

    prevExprReplacements = ctx->exprReplacements;
    prevExprReplacementCount = ctx->exprReplacementCount;
    xl6CgenDeclEmitClassArgTemps(ctx, exprTemps, exprTempCount, 0);
    xl6CgenIndent(ctx);
    ctx->exprReplacements = exprReplacements;
    ctx->exprReplacementCount = (size_t)exprTempCount;
    xl6CgenDeclEmitKnownStatementCallSymbol(ctx, func);
    xl6OutputPutc('(', ctx->out);
    xl6CgenKnownCallArgs(ctx, func, args);
    xl6OutputPuts(");\n", ctx->out);
    ctx->exprReplacements = prevExprReplacements;
    ctx->exprReplacementCount = prevExprReplacementCount;
    xl6CgenDeclDestroyClassArgTemps(ctx, exprTemps, exprTempCount);

    xrtFree(exprReplacements);
    xl6CgenDeclFreeClassArgTemps(exprTemps, exprTempCount);
    return 1;
}

static int xl6CgenTryKnownCallStatementWithScalarArgTemps(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    const XL6_AstNode* params;
    XL6_AstNode* inferredArgs;
    XL6_AstNode* func;
    XL6_AstNode* externFunc;
    XL6_AstNode* exportFunc;
    XL6_CgenDeclCallArgTemp* temps;
    XL6_CgenExprReplacement* replacements;
    const XL6_CgenExprReplacement* prevExprReplacements;
    size_t prevExprReplacementCount;
    int fixedCount;
    int tempCount;
    int i;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }

    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }

    inferredArgs = NULL;
    externFunc = xl6CgenFindExternFunc(ctx, callee->text);
    exportFunc = xl6CgenFindExportFunc(ctx, callee->text);
    if ( externFunc != NULL ) {
        func = externFunc;
    } else if ( exportFunc != NULL && xl6CgenExportFuncOverloadCount(ctx, callee->text) <= 1 ) {
        func = exportFunc;
    } else {
        func = xl6CgenResolveCallFuncInternal(ctx, expr, callee->text, &inferredArgs, 0);
        if ( inferredArgs != NULL ) {
            xl6AstFree(inferredArgs);
            return 0;
        }
    }
    if ( func == NULL ) {
        return 0;
    }

    params = func->childCount > 1 ? func->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    if ( params == NULL || fixedCount <= 0 || params->childCount > (size_t)fixedCount ) {
        return 0;
    }

    temps = (XL6_CgenDeclCallArgTemp*)xrtMalloc((size_t)fixedCount * sizeof(XL6_CgenDeclCallArgTemp));
    if ( temps == NULL ) {
        xl6CgenError(ctx, expr, "out of memory while preparing scalar argument temporaries");
        return 0;
    }
    memset(temps, 0, (size_t)fixedCount * sizeof(XL6_CgenDeclCallArgTemp));

    args = expr->childCount > 1 ? expr->children[1] : NULL;
    tempCount = 0;
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* argExpr = param != NULL ? xl6CgenFindArgForParam(args, param->text, i) : NULL;
        const XL6_AstNode* paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
        XL6_AstNode* canonicalType;

        if ( argExpr == NULL ) {
            continue;
        }

        canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, xl6CgenSubstituteGenericType(ctx, paramType));
        if ( canonicalType == NULL ) {
            continue;
        }
        if ( !xl6CgenDeclCallArgumentNeedsTemp(ctx, canonicalType, argExpr) ) {
            xl6AstFree(canonicalType);
            continue;
        }

        temps[i].active = 1;
        temps[i].expr = argExpr;
        temps[i].typeNode = canonicalType;
        snprintf(temps[i].name, sizeof(temps[i].name), "__xl6_call_arg_%d", ctx->tempId++);
        tempCount += 1;
    }

    if ( tempCount == 0 ) {
        xl6CgenDeclFreeCallArgTemps(temps, fixedCount);
        return 0;
    }

    replacements = xl6CgenDeclCreateCallArgReplacements(ctx, temps, fixedCount, tempCount);
    if ( replacements == NULL ) {
        xl6CgenDeclFreeCallArgTemps(temps, fixedCount);
        return 0;
    }

    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* argExpr = param != NULL ? xl6CgenFindArgForParam(args, param->text, i) : NULL;
        const char* cType;

        if ( !temps[i].active ) {
            continue;
        }

        cType = xl6CgenValueStorageCType(ctx, temps[i].typeNode);
        xl6CgenIndent(ctx);
        xl6OutputPuts(cType, ctx->out);
        xl6OutputPrintf(ctx->out, " %s = ", temps[i].name);
        xl6CgenDeclCallArgTempDefaultValue(ctx, temps[i].typeNode);
        xl6OutputPuts(";\n", ctx->out);
        if ( !xl6CgenDeclEmitCallArgTemp(ctx, temps[i].typeNode, cType, temps[i].name, argExpr) ) {
            xl6CgenError(ctx, argExpr, "failed to lower scalar call argument temporary");
        }
    }

    prevExprReplacements = ctx->exprReplacements;
    prevExprReplacementCount = ctx->exprReplacementCount;
    ctx->exprReplacements = replacements;
    ctx->exprReplacementCount = (size_t)tempCount;

    xl6CgenIndent(ctx);
    xl6CgenDeclEmitKnownStatementCallSymbol(ctx, func);
    xl6OutputPutc('(', ctx->out);
    xl6CgenKnownCallArgs(ctx, func, args);
    xl6OutputPuts(");\n", ctx->out);

    ctx->exprReplacements = prevExprReplacements;
    ctx->exprReplacementCount = prevExprReplacementCount;

    xrtFree(replacements);
    xl6CgenDeclFreeCallArgTemps(temps, fixedCount);
    return 1;
}

static int xl6CgenTryKnownCallStatementWithClassArgTemps(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    const XL6_AstNode* params;
    XL6_AstNode* inferredArgs;
    XL6_AstNode* func;
    XL6_AstNode* externFunc;
    XL6_AstNode* exportFunc;
    XL6_CgenDeclClassArgTemp* temps;
    int fixedCount;
    int i;
    int tempCount;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }

    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }

    inferredArgs = NULL;
    externFunc = xl6CgenFindExternFunc(ctx, callee->text);
    exportFunc = xl6CgenFindExportFunc(ctx, callee->text);
    if ( externFunc != NULL ) {
        func = externFunc;
    } else if ( exportFunc != NULL && xl6CgenExportFuncOverloadCount(ctx, callee->text) <= 1 ) {
        func = exportFunc;
    } else {
        func = xl6CgenResolveCallFuncInternal(ctx, expr, callee->text, &inferredArgs, 0);
        if ( inferredArgs != NULL ) {
            xl6AstFree(inferredArgs);
            return 0;
        }
    }
    if ( func == NULL ) {
        return 0;
    }

    params = func->childCount > 1 ? func->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    if ( params != NULL && params->childCount > (size_t)fixedCount ) {
        return 0;
    }
    if ( fixedCount <= 0 ) {
        return 0;
    }

    temps = (XL6_CgenDeclClassArgTemp*)xrtMalloc((size_t)fixedCount * sizeof(XL6_CgenDeclClassArgTemp));
    if ( temps == NULL ) {
        xl6CgenError(ctx, expr, "out of memory while preparing class argument temporaries");
        return 0;
    }
    memset(temps, 0, (size_t)fixedCount * sizeof(XL6_CgenDeclClassArgTemp));

    args = expr->childCount > 1 ? expr->children[1] : NULL;
    tempCount = 0;
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params != NULL ? params->children[i] : NULL;
        const XL6_AstNode* argExpr = param != NULL ? xl6CgenFindArgForParam(args, param->text, i) : NULL;
        const XL6_AstNode* paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;

        if ( argExpr != NULL &&
             xl6CgenDeclPrepareClassArgTemp(ctx, paramType, argExpr, ctx->tempId++, &temps[i]) ) {
            tempCount += 1;
        }
    }

    if ( tempCount == 0 ) {
        xl6CgenDeclFreeClassArgTemps(temps, fixedCount);
        if ( xl6CgenTryKnownCallStatementWithNestedClassArgTemps(ctx, func, args) ) {
            return 1;
        }
        return 0;
    }

    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params != NULL ? params->children[i] : NULL;
        const XL6_AstNode* argExpr = param != NULL ? xl6CgenFindArgForParam(args, param->text, i) : NULL;

        if ( !temps[i].active ) {
            continue;
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = {0};\n", xl6CgenTypeName(ctx, temps[i].valueType), temps[i].valueName);
        if ( temps[i].isConstructor ) {
            if ( !xl6CgenEmitClassCtorInto(ctx, argExpr, temps[i].valueName) ) {
                xl6CgenError(ctx, argExpr, "class argument constructor lowering failed");
            }
        } else if ( temps[i].isUpcast ) {
            if ( !xl6CgenClassUpcastStmt(ctx, temps[i].valueType, argExpr, temps[i].valueName) ) {
                xl6CgenError(ctx, argExpr, "class argument upcast lowering failed");
            }
        } else {
            xl6CgenEmitMemberFunctionStatementTemp(ctx, argExpr, temps[i].functionName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = ", temps[i].valueName);
            xl6CgenMemberFunctionStatementInvokeExpr(ctx, argExpr, temps[i].functionType, temps[i].functionName);
            xl6OutputPuts(";\n", ctx->out);
        }
    }

    xl6CgenIndent(ctx);
    xl6CgenDeclEmitKnownStatementCallSymbol(ctx, func);
    xl6OutputPutc('(', ctx->out);
    xl6CgenDeclEmitKnownCallArgsWithClassTemps(ctx, func, args, temps, fixedCount);
    xl6OutputPuts(");\n", ctx->out);

    for ( i = fixedCount - 1; i >= 0; --i ) {
        if ( temps[i].active ) {
            xl6CgenDeclDestroyTemporaryClassValue(ctx, temps[i].classNode, temps[i].valueType, temps[i].valueName);
        }
    }

    xl6CgenDeclFreeClassArgTemps(temps, fixedCount);
    return 1;
}
