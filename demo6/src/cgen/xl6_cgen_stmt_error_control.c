static void xl6CgenEmitDefaultErrorReturn(XL6_CgenContext* ctx)
{
    int returnId;

    if ( ctx->currentReturns == NULL || ctx->currentReturns->childCount == 0 ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("return;\n", ctx->out);
        return;
    }

    returnId = ctx->tempId++;
    xl6CgenIndent(ctx);
    if ( ctx->currentReturns->childCount > 1 ) {
        if ( ctx->currentReturnFunc != NULL ) {
            xl6CgenFuncReturnStructName(ctx, ctx->currentReturnFunc);
        } else {
            xl6CgenFunctionReturnStructName(ctx, ctx->currentReturnParams, ctx->currentReturns);
        }
    } else {
        const XL6_AstNode* returnType = ctx->currentReturns->children[0];
        if ( xl6CgenIsTypedContainerType(ctx, returnType) ) {
            xl6OutputPuts(xl6CgenTypedContainerCType(ctx, returnType), ctx->out);
        } else {
            xl6OutputPuts(xl6CgenTypeName(ctx, returnType), ctx->out);
        }
    }
    xl6OutputPrintf(ctx->out, " __xl6_error_return_%d;\n", returnId);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
                    "memset(&__xl6_error_return_%d, 0, sizeof(__xl6_error_return_%d));\n",
                    returnId,
                    returnId);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "return __xl6_error_return_%d;\n", returnId);
}

static void xl6CgenEmitExceptionPropagate(XL6_CgenContext* ctx, const XL6_AstNode* at)
{
    XL6_CgenTryTarget* target;
    XL6_CgenFinallyTarget* finallyTarget;

    (void)at;
    if ( ctx == NULL || !ctx->needsErrorRuntime ) {
        return;
    }

    target = xl6CgenCurrentTryTarget(ctx);
    finallyTarget = xl6CgenCurrentFinallyTarget(ctx);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( xl6_error_has() ) {\n", ctx->out);
    ctx->indent += 1;
    if ( finallyTarget != NULL && finallyTarget->inFinally ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (__xl6_pending_kind_%d == 1) {\n", finallyTarget->id);
        ctx->indent += 1;
        xl6CgenDiscardPendingFinallyReturn(ctx, finallyTarget->id);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_pending_kind_%d = 0;\n", finallyTarget->id);
    }
    if ( xl6CgenEmitCurrentErrorScopeJump(ctx) ) {
        ;
    } else if ( target != NULL ) {
        xl6CgenEmitCleanupsSince(ctx, target->cleanupMark);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out,
                "goto __xl6_try_%s_%d;\n",
                target->targetFinally ? "finally" : "catch",
                target->id);
    } else {
        xl6CgenEmitCleanupsSince(ctx, 0);
        if ( ctx->currentErrorExitLabel != NULL ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "goto %s;\n", ctx->currentErrorExitLabel);
        } else {
            xl6CgenEmitDefaultErrorReturn(ctx);
        }
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}
static void xl6CgenEmitGotoIfError(XL6_CgenContext* ctx, const char* label)
{
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( xl6_error_has() ) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "goto %s;\n", label);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenThrowStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* kindNode;
    const XL6_AstNode* expr;
    XL6_CgenTryTarget* target;
    XL6_CgenTaskSpawnArgTempScope taskScope;
    int hasTaskScope;

    target = xl6CgenCurrentTryTarget(ctx);
    if ( !xl6CgenValidateExitPlan(
            ctx,
            node,
            XL6_SEM_EXIT_THROW,
            target != NULL ? target->region : xl6CgenCurrentCallableRegion(ctx)
        ) ) {
        return;
    }
    kindNode = node != NULL && node->childCount == 2 ? node->children[0] : NULL;
    expr = node != NULL && node->childCount > 0 ? node->children[node->childCount == 2 ? 1 : 0] : NULL;
    hasTaskScope = expr != NULL && xl6CgenTaskSpawnArgTempScopeBegin(ctx, expr, &taskScope);
    xl6CgenIndent(ctx);
    if ( expr == NULL ) {
        if ( ctx->currentCatchErrorName != NULL ) {
            xl6OutputPrintf(ctx->out, "xl6_error_reraise(%s);\n", ctx->currentCatchErrorName);
        } else {
            xl6OutputPuts("xl6_error_raise(\"Error\", \"rethrow without active catch\", \"\", 0, 0);\n", ctx->out);
        }
    } else {
        xl6OutputPrintf(ctx->out, "xl6_error_raise(\"%s\", ", kindNode != NULL && kindNode->text != NULL ? kindNode->text : "Error");
        xl6CgenExpr(ctx, expr);
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d);\n", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
    }
    if ( hasTaskScope ) {
        xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
    }
    xl6CgenEmitExceptionPropagate(ctx, node);
}

static void xl6CgenAssertStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* cond;
    const XL6_AstNode* message;
    char conditionName[64];
    int hasConditionTemp;
    int hasConditionTaskScope;
    int hasMessageTaskScope;
    XL6_CgenTaskSpawnArgTempScope conditionTaskScope;
    XL6_CgenTaskSpawnArgTempScope messageTaskScope;

    cond = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    message = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    conditionName[0] = '\0';
    hasConditionTemp = cond != NULL &&
                       xl6CgenTryConditionTemp(ctx, cond, "__xl6_assert_cond_", conditionName, sizeof(conditionName));
    hasConditionTaskScope = 0;
    hasMessageTaskScope = 0;
    if ( cond != NULL && !hasConditionTemp ) {
        hasConditionTaskScope = xl6CgenTaskSpawnArgTempScopeBegin(ctx, cond, &conditionTaskScope);
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( !(", ctx->out);
    if ( hasConditionTemp ) {
        xl6OutputPuts(conditionName, ctx->out);
    } else {
        xl6CgenConditionExpr(ctx, cond);
    }
    xl6OutputPuts(") ) {\n", ctx->out);
    if ( hasConditionTaskScope ) {
        xl6CgenTaskSpawnArgTempScopeEnd(ctx, &conditionTaskScope);
    }
    ctx->indent += 1;
    if ( message != NULL ) {
        hasMessageTaskScope = xl6CgenTaskSpawnArgTempScopeBegin(ctx, message, &messageTaskScope);
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts("xl6_error_raise(\"AssertionError\", ", ctx->out);
    if ( message != NULL ) {
        xl6CgenExpr(ctx, message);
    } else {
        xl6OutputPuts("\"assert failed\"", ctx->out);
    }
    xl6OutputPrintf(ctx->out, ", \"\", %d, %d);\n", node != NULL ? node->line : 0, node != NULL ? node->column : 0);
    if ( hasMessageTaskScope ) {
        xl6CgenTaskSpawnArgTempScopeEnd(ctx, &messageTaskScope);
    }
    xl6CgenEmitExceptionPropagate(ctx, node);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}
static int xl6CgenTryHasCatch(const XL6_AstNode* node)
{
    const XL6_AstNode* catchList = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    return catchList != NULL && catchList->kind == XL6_AST_LIST && catchList->childCount > 0;
}

static int xl6CgenTryHasFinally(const XL6_AstNode* node)
{
    const XL6_AstNode* finallyBlock = node != NULL && node->childCount > 2 ? node->children[2] : NULL;
    return finallyBlock != NULL && finallyBlock->childCount > 0;
}

static int xl6CgenFunctionHasReturnValue(XL6_CgenContext* ctx)
{
    return ctx != NULL && ctx->currentReturns != NULL && ctx->currentReturns->childCount > 0;
}

static void xl6CgenPendingReturnType(XL6_CgenContext* ctx)
{
    if ( ctx->currentReturns != NULL && ctx->currentReturns->childCount > 1 ) {
        if ( ctx->currentReturnFunc != NULL ) {
            xl6CgenFuncReturnStructName(ctx, ctx->currentReturnFunc);
        } else {
            xl6CgenFunctionReturnStructName(ctx, ctx->currentReturnParams, ctx->currentReturns);
        }
    } else {
        if ( ctx->currentReturnFunc != NULL ) {
            xl6OutputPuts(xl6CgenFuncSingleReturnCType(ctx, ctx->currentReturnFunc), ctx->out);
        } else if ( xl6CgenIsTypedContainerType(ctx, ctx->currentReturnType) ) {
            xl6OutputPuts(xl6CgenTypedContainerCType(ctx, ctx->currentReturnType), ctx->out);
        } else {
            xl6OutputPuts(xl6CgenTypeName(ctx, ctx->currentReturnType), ctx->out);
        }
    }
}

static void xl6CgenDeclarePendingFinallyControl(XL6_CgenContext* ctx, int targetId)
{
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "int __xl6_pending_kind_%d = 0;\n", targetId);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "int __xl6_pending_target_%d = 0;\n", targetId);
    if ( xl6CgenFunctionHasReturnValue(ctx) ) {
        xl6CgenIndent(ctx);
        xl6CgenPendingReturnType(ctx);
        xl6OutputPrintf(ctx->out, " __xl6_pending_return_%d;\n", targetId);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "memset(&__xl6_pending_return_%d, 0, sizeof(__xl6_pending_return_%d));\n", targetId, targetId);
    }
}

static void xl6CgenDiscardPendingFinallyReturn(XL6_CgenContext* ctx, int targetId)
{
    char name[96];

    if ( !xl6CgenFunctionHasReturnValue(ctx) ) {
        return;
    }
    snprintf(name, sizeof(name), "__xl6_pending_return_%d", targetId);
    if ( ctx->currentReturns->childCount > 1 ) {
        xl6CgenDiscardTemporaryMultiResultByTypes(ctx, ctx->currentReturns, name);
    } else {
        xl6CgenDiscardTemporaryResultByType(ctx, ctx->currentReturnType, name);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "memset(&%s, 0, sizeof(%s));\n", name, name);
}

static int xl6CgenEmitPendingFinallyReturn(
    XL6_CgenContext* ctx,
    const char* returnTempName,
    const char* movedCleanupName)
{
    XL6_CgenFinallyTarget* target;

    target = xl6CgenCurrentFinallyTarget(ctx);
    if ( target == NULL ) {
        return 0;
    }
    xl6CgenEmitCleanupsSinceExcept(
        ctx,
        target->cleanupMark,
        movedCleanupName != NULL && movedCleanupName[0] != '\0' ? movedCleanupName : NULL);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_pending_kind_%d == 1) {\n", target->id);
    ctx->indent += 1;
    xl6CgenDiscardPendingFinallyReturn(ctx, target->id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(
        ctx->out,
        "__xl6_pending_return_%d = %s;\n",
        target->id,
        returnTempName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_pending_kind_%d = 1;\n", target->id);
    xl6CgenIndent(ctx);
    if ( target->inFinally ) {
        xl6OutputPrintf(ctx->out, "goto __xl6_try_finally_dispatch_%d;\n", target->id);
    } else {
        xl6OutputPrintf(ctx->out, "goto __xl6_try_finally_%d;\n", target->id);
    }
    return 1;
}

static int xl6CgenEmitPendingFinallyVoidReturn(XL6_CgenContext* ctx)
{
    XL6_CgenFinallyTarget* target = xl6CgenCurrentFinallyTarget(ctx);

    if ( target == NULL ) {
        return 0;
    }
    xl6CgenEmitCleanupsSince(ctx, target->cleanupMark);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_pending_kind_%d == 1) {\n", target->id);
    ctx->indent += 1;
    xl6CgenDiscardPendingFinallyReturn(ctx, target->id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_pending_kind_%d = 1;\n", target->id);
    xl6CgenIndent(ctx);
    if ( target->inFinally ) {
        xl6OutputPrintf(ctx->out, "goto __xl6_try_finally_dispatch_%d;\n", target->id);
    } else {
        xl6OutputPrintf(ctx->out, "goto __xl6_try_finally_%d;\n", target->id);
    }
    return 1;
}

static void xl6CgenGotoPendingFinallyTarget(XL6_CgenContext* ctx, const XL6_CgenFinallyTarget* target)
{
    if ( target->inFinally ) {
        xl6OutputPrintf(ctx->out, "goto __xl6_try_finally_dispatch_%d;\n", target->id);
    } else {
        xl6OutputPrintf(ctx->out, "goto __xl6_try_finally_%d;\n", target->id);
    }
}
static void xl6CgenEmitPendingFinallyReturnDispatch(
    XL6_CgenContext* ctx,
    const XL6_CgenFinallyTarget* target)
{
    XL6_CgenFinallyTarget* parent = xl6CgenCurrentFinallyTarget(ctx);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_pending_kind_%d == 1) {\n", target->id);
    ctx->indent += 1;
    if ( parent != NULL ) {
        if ( xl6CgenFunctionHasReturnValue(ctx) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (__xl6_pending_kind_%d == 1) {\n", parent->id);
            ctx->indent += 1;
            xl6CgenDiscardPendingFinallyReturn(ctx, parent->id);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "__xl6_pending_return_%d = __xl6_pending_return_%d;\n",
                parent->id,
                target->id);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "memset(&__xl6_pending_return_%d, 0, sizeof(__xl6_pending_return_%d));\n",
                target->id,
                target->id);
        }
        xl6CgenEmitCleanupsSince(ctx, parent->cleanupMark);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_pending_kind_%d = 1;\n", parent->id);
        xl6CgenIndent(ctx);
        xl6CgenGotoPendingFinallyTarget(ctx, parent);
    } else {
        if ( ctx->currentAsyncWorker ) {
            xl6CgenAsyncPendingResultCompletion(ctx, target->id);
        } else if ( !xl6CgenFunctionHasReturnValue(ctx) ) {
            xl6CgenEmitCleanupsSince(ctx, 0);
            xl6CgenIndent(ctx);
            xl6OutputPuts("return;\n", ctx->out);
        } else if ( ctx->currentReturns->childCount > 1 ) {
            xl6CgenEmitCleanupsSince(ctx, 0);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "return __xl6_pending_return_%d;\n", target->id);
        } else if ( xl6CgenIsOwnedStringTypeNode(ctx, ctx->currentReturnType) ) {
            xl6CgenEmitCleanupsSince(ctx, 0);
            xl6CgenIndent(ctx);
            if ( xl6CgenIsNullableStringTypeNode(ctx, ctx->currentReturnType) ) {
                xl6OutputPrintf(
                    ctx->out,
                    "return __xl6_pending_return_%d != 0 ? xl6_temp_string_take((char*)__xl6_pending_return_%d, 0u) : 0;\n",
                    target->id,
                    target->id);
            } else {
                xl6OutputPrintf(ctx->out, "return xl6_temp_string_take((char*)__xl6_pending_return_%d, 0u);\n", target->id);
            }
        } else {
            xl6CgenEmitCleanupsSince(ctx, 0);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "return __xl6_pending_return_%d;\n", target->id);
        }
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenEmitPendingFinallyLoopDispatch(
    XL6_CgenContext* ctx,
    const XL6_CgenFinallyTarget* target,
    int kind)
{
    XL6_CgenFinallyTarget* parent = xl6CgenCurrentFinallyTarget(ctx);
    size_t i;

    for ( i = 0; i < target->loopTargetCount && i < ctx->loopTargetCount; ++i ) {
        XL6_CgenLoopTarget* loop = &ctx->loopTargets[i];
        int labelId = kind == 3 ? loop->continueId : loop->breakId;

        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "if (__xl6_pending_kind_%d == %d && __xl6_pending_target_%d == %d) {\n",
            target->id,
            kind,
            target->id,
            labelId);
        ctx->indent += 1;
        if ( parent != NULL && i < parent->loopTargetCount ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (__xl6_pending_kind_%d == 1) {\n", parent->id);
            ctx->indent += 1;
            xl6CgenDiscardPendingFinallyReturn(ctx, parent->id);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenEmitCleanupsSince(ctx, parent->cleanupMark);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "__xl6_pending_kind_%d = %d;\n", parent->id, kind);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "__xl6_pending_target_%d = %d;\n", parent->id, labelId);
            xl6CgenIndent(ctx);
            xl6CgenGotoPendingFinallyTarget(ctx, parent);
        } else {
            xl6CgenEmitCleanupsSince(
                ctx,
                kind == 3 ? loop->continueCleanupMark : loop->breakCleanupMark);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "goto __xl6_%s_%d;\n", kind == 3 ? "continue" : "break", labelId);
        }
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    }
}
static void xl6CgenTryStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* tryBlock;
    const XL6_AstNode* catchList;
    const XL6_AstNode* finallyBlock;
    char caughtName[64];
    char catchLabel[64];
    char finallyLabel[64];
    char endLabel[64];
    int id;
    int hasCatch;
    int hasFinally;
    int hasFinallyTarget;
    XL6_CgenFinallyTarget finallyTarget;
    size_t tryCleanupMark;
    size_t tryVarMark;

    tryBlock = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    catchList = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    finallyBlock = node != NULL && node->childCount > 2 ? node->children[2] : NULL;
    hasCatch = xl6CgenTryHasCatch(node);
    hasFinally = xl6CgenTryHasFinally(node);
    id = ctx->tempId++;

    snprintf(catchLabel, sizeof(catchLabel), "__xl6_try_catch_%d", id);
    snprintf(finallyLabel, sizeof(finallyLabel), "__xl6_try_finally_%d", id);
    snprintf(endLabel, sizeof(endLabel), "__xl6_try_end_%d", id);
    snprintf(caughtName, sizeof(caughtName), "__xl6_caught_error_%d", id);

    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;

    tryCleanupMark = ctx->cleanupCount;
    tryVarMark = ctx->varCount;
    hasFinallyTarget = 0;
    memset(&finallyTarget, 0, sizeof(finallyTarget));
    if ( hasFinally ) {
        xl6CgenDeclarePendingFinallyControl(ctx, id);
        hasFinallyTarget = xl6CgenPushFinallyTarget(ctx, tryCleanupMark, id);
    }
    xl6CgenPushTryTarget(
        ctx,
        tryCleanupMark,
        xl6CgenHirRegion(ctx, node),
        id,
        hasCatch ? 0 : 1
    );
    xl6CgenBlock(ctx, tryBlock);
    xl6CgenFreeVarsSince(ctx, tryVarMark);
    xl6CgenPopTryTarget(ctx);
    xl6CgenEmitGotoIfError(ctx, hasCatch ? catchLabel : finallyLabel);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "goto %s;\n", hasFinally ? finallyLabel : endLabel);

    if ( hasCatch ) {
        size_t catchIndex;

        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s:\n", catchLabel);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "XL6_Error* %s = xl6_error_take();\n", caughtName);

        for ( catchIndex = 0; catchList != NULL && catchIndex < catchList->childCount; ++catchIndex ) {
            const XL6_AstNode* catchNode = catchList->children[catchIndex];
            const XL6_AstNode* catchKind = NULL;
            const XL6_AstNode* catchName = NULL;
            const XL6_AstNode* catchBlock = NULL;
            const XL6_AstNode* errorType;
            const char* prevCatchErrorName;
            char catchCName[512];
            size_t catchCleanupMark;
            size_t catchVarMark;

            if ( catchNode == NULL || catchNode->kind != XL6_AST_CATCH ) {
                continue;
            }
            if ( catchNode->childCount == 3 ) {
                catchKind = catchNode->children[0];
                catchName = catchNode->children[1];
                catchBlock = catchNode->children[2];
            } else {
                catchName = catchNode->childCount > 0 ? catchNode->children[0] : NULL;
                catchBlock = catchNode->childCount > 1 ? catchNode->children[1] : NULL;
            }
            xl6CgenIndent(ctx);
            if ( catchKind != NULL && catchKind->text != NULL ) {
                xl6OutputPrintf(ctx->out, "if (%s != 0 && xl6_error_is(%s, \"%s\")) {\n", caughtName, caughtName, catchKind->text);
            } else {
                xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", caughtName);
            }
            ctx->indent += 1;
            xl6CgenCIdentText(catchCName, sizeof(catchCName), catchName != NULL ? catchName->text : "__xl6_error");
            catchVarMark = ctx->varCount;
            catchCleanupMark = ctx->cleanupCount;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "XL6_Error* %s = %s;\n", catchCName, caughtName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = 0;\n", caughtName);
            errorType = xl6CgenOwnSimpleType(ctx, "error", catchName != NULL ? catchName : catchNode);
            xl6CgenAddVarType(ctx, catchName != NULL ? catchName->text : "__xl6_error", errorType);
            xl6CgenAddErrorCleanup(ctx, catchName != NULL ? catchName->text : "__xl6_error", errorType);

            prevCatchErrorName = ctx->currentCatchErrorName;
            ctx->currentCatchErrorName = catchCName;
            if ( hasFinally ) {
                xl6CgenPushTryTarget(
                    ctx,
                    catchCleanupMark,
                    xl6CgenHirRegion(ctx, node),
                    id,
                    1
                );
            }
            xl6CgenBlock(ctx, catchBlock);
            if ( hasFinally ) {
                xl6CgenPopTryTarget(ctx);
            }
            ctx->currentCatchErrorName = prevCatchErrorName;
            xl6CgenCloseScope(ctx, catchCleanupMark);
            xl6CgenFreeVarsSince(ctx, catchVarMark);
            xl6CgenEmitGotoIfError(ctx, hasFinally ? finallyLabel : endLabel);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "goto %s;\n", hasFinally ? finallyLabel : endLabel);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        }

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", caughtName);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xl6_error_set_active(%s);\n", caughtName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", caughtName);
        if ( hasFinally ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "goto %s;\n", finallyLabel);
        } else {
            xl6CgenEmitExceptionPropagate(ctx, node);
        }
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "goto %s;\n", hasFinally ? finallyLabel : endLabel);
    }

    if ( hasFinally ) {
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s:\n", finallyLabel);
        ctx->indent += 1;
        if ( hasFinallyTarget && xl6CgenCurrentFinallyTarget(ctx) != NULL ) {
            xl6CgenCurrentFinallyTarget(ctx)->inFinally = 1;
        }
        xl6CgenBlock(ctx, finallyBlock);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (xl6_error_has() && __xl6_pending_kind_%d == 1) {\n", id);
        ctx->indent += 1;
        xl6CgenDiscardPendingFinallyReturn(ctx, id);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenEmitExceptionPropagate(ctx, node);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_try_finally_dispatch_%d:\n", id);
        ctx->indent += 1;
        if ( hasFinallyTarget && xl6CgenCurrentFinallyTarget(ctx) != NULL ) {
            finallyTarget = *xl6CgenCurrentFinallyTarget(ctx);
            xl6CgenPopFinallyTarget(ctx);
            hasFinallyTarget = 0;
            xl6CgenEmitPendingFinallyReturnDispatch(ctx, &finallyTarget);
            xl6CgenEmitPendingFinallyLoopDispatch(ctx, &finallyTarget, 2);
            xl6CgenEmitPendingFinallyLoopDispatch(ctx, &finallyTarget, 3);
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "goto %s;\n", endLabel);
    }

    if ( hasFinallyTarget ) {
        xl6CgenPopFinallyTarget(ctx);
    }

    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s:\n", endLabel);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts(";\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}
