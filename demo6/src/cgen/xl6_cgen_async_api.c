static void xl6CgenAsyncFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    xl6OutputPuts("xfuture* ", ctx->out);
    xl6CgenFuncSymbol(ctx, node);
    xl6OutputPutc('(', ctx->out);
    xl6CgenAsyncEmitParamList(ctx, xl6CgenAsyncParams(node));
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenIsChannelGenericInstant(const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArgs;

    if ( node == NULL || node->kind != XL6_AST_GENERIC_INSTANT || node->childCount < 2 ) {
        return 0;
    }
    nameNode = node->children[0];
    typeArgs = node->children[1];
    return nameNode != NULL &&
           nameNode->kind == XL6_AST_NAME &&
           xl6CTextEquals(nameNode->text, "channel") &&
           typeArgs != NULL &&
           typeArgs->childCount == 1;
}

static const XL6_AstNode* xl6CgenChannelElementType(const XL6_AstNode* channelType)
{
    const XL6_AstNode* args;

    if ( !xl6CgenIsChannelType(channelType) ||
         channelType->childCount == 0 ||
         channelType->children[0] == NULL ||
         channelType->children[0]->kind != XL6_AST_LIST ) {
        return NULL;
    }
    args = channelType->children[0];
    return args->childCount > 0 ? args->children[0] : NULL;
}

static int xl6CgenChannelCtorCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* callee)
{
    const XL6_AstNode* args;
    const XL6_AstNode* capArg;

    if ( !xl6CgenIsChannelGenericInstant(callee) ) {
        return 0;
    }

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    capArg = args != NULL && args->childCount > 0 && args->children[0] != NULL &&
             args->children[0]->childCount > 0 ? args->children[0]->children[0] : NULL;

    xl6OutputPuts("xl6_channel_require_handle(xrtChannelCreate(", ctx->out);
    if ( capArg != NULL ) {
        xl6CgenExpr(ctx, capArg);
    } else {
        xl6OutputPuts("0", ctx->out);
    }
    xl6OutputPrintf(ctx->out, "), \"\", %d, %d)", callee->line, callee->column);
    return 1;
}

static void xl6CgenChannelReceiverExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    if ( receiver != NULL && receiver->kind == XL6_AST_NAME ) {
        if ( xl6CgenAnonCaptureIndex(ctx->currentAnonFunc, receiver->text) >= 0 ) {
            xl6OutputPuts("__env->", ctx->out);
            xl6CgenFputsCIdent(ctx->out, receiver->text);
            return;
        }
        xl6CgenFputsCIdent(ctx->out, receiver->text);
        return;
    }
    xl6CgenExpr(ctx, receiver);
}

static int xl6CgenChannelMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* itemType;
    const XL6_AstNode* args;
    const XL6_AstNode* arg;
    const XL6_AstNode* argExpr;
    const char* cType;

    if ( call == NULL || member == NULL || member->kind != XL6_AST_MEMBER ) {
        return 0;
    }
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return 0;
    }
    receiverType = xl6CgenFindVarType(ctx, receiver->text);
    if ( !xl6CgenIsChannelType(receiverType) ) {
        return 0;
    }

    if ( xl6CTextEquals(member->text, "close") ) {
        xl6OutputPuts("(xrtChannelClose(", ctx->out);
        xl6CgenChannelReceiverExpr(ctx, receiver);
        xl6OutputPuts("), 0)", ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(member->text, "count") ) {
        xl6OutputPuts("xrtChannelCount(", ctx->out);
        xl6CgenChannelReceiverExpr(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(member->text, "isClosed") ) {
        xl6OutputPuts("xrtChannelIsClosed(", ctx->out);
        xl6CgenChannelReceiverExpr(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        return 1;
    }

    itemType = xl6CgenChannelElementType(receiverType);
    cType = itemType != NULL ? xl6CgenTypeName(ctx, itemType) : "int";
    args = call->childCount > 1 ? call->children[1] : NULL;
    arg = args != NULL && args->childCount == 1 ? args->children[0] : NULL;
    argExpr = arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;

    if ( xl6CTextEquals(member->text, "send") ) {
        xl6OutputPuts("xl6_channel_send_value(", ctx->out);
        xl6CgenChannelReceiverExpr(ctx, receiver);
        xl6OutputPuts(", &(", ctx->out);
        xl6OutputPuts(cType, ctx->out);
        xl6OutputPuts("){", ctx->out);
        xl6CgenExpr(ctx, argExpr);
        xl6OutputPuts("}, sizeof(", ctx->out);
        xl6OutputPuts(cType, ctx->out);
        xl6OutputPrintf(ctx->out, "), \"\", %d, %d)", member->line, member->column);
        return 1;
    }

    if ( xl6CTextEquals(member->text, "recvInto") ||
         xl6CTextEquals(member->text, "tryRecvInto") ) {
        xl6OutputPuts("xl6_channel_recv_into(", ctx->out);
        xl6CgenChannelReceiverExpr(ctx, receiver);
        xl6OutputPuts(", &", ctx->out);
        if ( argExpr != NULL && argExpr->kind == XL6_AST_NAME ) {
            xl6CgenFputsCIdent(ctx->out, argExpr->text);
        } else {
            xl6CgenExpr(ctx, argExpr);
        }
        xl6OutputPuts(", sizeof(", ctx->out);
        xl6OutputPuts(cType, ctx->out);
        xl6OutputPrintf(ctx->out, "), %d, \"\", %d, %d)",
                        xl6CTextEquals(member->text, "recvInto") ? 1 : 0,
                        member->line,
                        member->column);
        return 1;
    }

    return 0;
}

static int xl6CgenTaskFutureExprIsOwnedDirect(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;
    int owned;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL ) {
        return 0;
    }

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    owned = xl6CgenIsFutureLikeType(concreteType);
    xl6AstFree(typeNode);
    return owned;
}

static int xl6CgenTaskFutureExprIsOwned(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr != NULL && expr->kind == XL6_AST_CONDITIONAL && expr->childCount > 2 ) {
        return 1;
    }
    return xl6CgenTaskFutureExprIsOwnedDirect(ctx, expr);
}

static void xl6CgenTaskFutureOwnedExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr != NULL && expr->kind == XL6_AST_CONDITIONAL && expr->childCount > 2 ) {
        xl6OutputPutc('(', ctx->out);
        xl6CgenConditionExpr(ctx, expr->children[0]);
        xl6OutputPuts(" ? ", ctx->out);
        xl6CgenTaskFutureOwnedExpr(ctx, expr->children[1]);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenTaskFutureOwnedExpr(ctx, expr->children[2]);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    if ( xl6CgenTaskFutureExprIsOwnedDirect(ctx, expr) ) {
        xl6CgenExpr(ctx, expr);
        return;
    }
    xl6OutputPuts("xl6_future_add_ref_if_needed(", ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenTaskFutureArgExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( xl6CgenTaskFutureExprIsOwned(ctx, expr) &&
         expr != NULL && expr->kind == XL6_AST_CONDITIONAL && expr->childCount > 2 ) {
        xl6CgenTaskFutureOwnedExpr(ctx, expr);
        return;
    }
    xl6CgenExpr(ctx, expr);
}

static void xl6CgenTaskFutureLiteralArgs(XL6_CgenContext* ctx, const XL6_AstNode* literal)
{
    size_t i;

    if ( literal != NULL ) {
        for ( i = 0; i < literal->childCount; ++i ) {
            xl6OutputPuts(", ", ctx->out);
            xl6CgenTaskFutureArgExpr(ctx, literal->children[i]);
            xl6OutputPrintf(ctx->out, ", %d", xl6CgenTaskFutureExprIsOwned(ctx, literal->children[i]));
        }
    }
}

static const XL6_AstNode* xl6CgenTaskSpawnArgExprAt(const XL6_AstNode* call, size_t index)
{
    const XL6_AstNode* args;
    const XL6_AstNode* arg;

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    arg = args != NULL && index < args->childCount ? args->children[index] : NULL;
    return arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
}

static int xl6CgenTaskSpawnCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* args;
    const XL6_AstNode* fnExpr;
    const XL6_AstNode* paramTypes;
    const XL6_AstNode* returns;
    const XL6_AstNode* resultType;
    XL6_AstNode* functionType;
    size_t argCount;
    size_t actualCount;
    size_t fixedCount;
    int hasArrayVararg;
    size_t i;

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    argCount = args != NULL ? args->childCount : 0;
    fnExpr = argCount > 0 ? xl6CgenTaskSpawnArgExprAt(call, 0) : NULL;
    functionType = xl6CgenCloneExprTypeForInference(ctx, fnExpr);
    paramTypes = xl6CgenFunctionParamTypes(functionType);
    returns = xl6CgenFunctionReturnTypes(functionType);
    if ( !xl6CgenIsFunctionType(functionType) || paramTypes == NULL || returns == NULL || returns->childCount > 1 ) {
        xl6AstFree(functionType);
        xl6CgenError(ctx, call, "task.spawn 第一个参数必须是完整签名的 func 值");
        xl6OutputPuts("0", ctx->out);
        return 1;
    }

    resultType = returns->childCount == 1 ? returns->children[0] : NULL;
    fixedCount = xl6CgenFunctionTypeFixedParamCount(paramTypes);
    hasArrayVararg = paramTypes->childCount > fixedCount &&
                     xl6CgenFunctionTypeParamIsArrayVararg(paramTypes->children[fixedCount]);
    actualCount = argCount > 0 ? argCount - 1 : 0;
    xl6OutputPuts("xl6_task_spawn_values(", ctx->out);
    xl6CgenFunctionValueExpr(ctx, fnExpr, functionType);
    xl6OutputPuts(", ", ctx->out);
    if ( resultType != NULL ) {
        xl6CgenEmitXrtTypeDescRef(ctx, resultType);
        xl6OutputPuts(", sizeof(", ctx->out);
        xl6OutputPuts(xl6CgenTypeName(ctx, resultType), ctx->out);
        xl6OutputPutc(')', ctx->out);
    } else {
        xl6OutputPuts("xrtTypeNull(), 0", ctx->out);
    }
    xl6OutputPrintf(ctx->out, ", \"\", %d, %d, %d, %d",
                    member != NULL ? member->line : 0,
                    member != NULL ? member->column : 0,
                    xl6CgenExprIsFunctionBorrow(ctx, fnExpr) ? 1 : 0,
                    (int)actualCount);
    for ( i = 0; i < actualCount; ++i ) {
        const XL6_AstNode* argExpr = xl6CgenTaskSpawnArgExprAt(call, i + 1);
        const XL6_AstNode* targetType = i < fixedCount ? paramTypes->children[i] : NULL;

        xl6OutputPuts(", ", ctx->out);
        if ( targetType != NULL ) {
            xl6CgenXValueFromExprForTarget(ctx, targetType, argExpr);
        } else if ( hasArrayVararg ) {
            xl6CgenXValueFromExpr(ctx, argExpr);
        } else {
            xl6OutputPuts("0", ctx->out);
        }
    }
    xl6OutputPutc(')', ctx->out);
    xl6AstFree(functionType);
    return 1;
}

static int xl6CgenTaskNamespaceCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* argExpr;
    XL6_AstNode* valueType;

    if ( call == NULL || member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME || !xl6CTextEquals(receiver->text, "task") ) {
        return 0;
    }

    argExpr = xl6CgenTaskCallArgExpr(call);
    if ( xl6CTextEquals(member->text, "spawn") ) {
        return xl6CgenTaskSpawnCall(ctx, call, member);
    }
    if ( xl6CTextEquals(member->text, "cancel") ) {
        xl6OutputPuts("xl6_task_cancel_value(", ctx->out);
        xl6CgenTaskFutureArgExpr(ctx, argExpr);
        xl6OutputPrintf(ctx->out, ", %d)", xl6CgenTaskFutureExprIsOwned(ctx, argExpr));
        return 1;
    }

    if ( !xl6CTextEquals(member->text, "all") && !xl6CTextEquals(member->text, "race") ) {
        return 0;
    }

    valueType = xl6CgenCloneTaskArrayItemFutureValueType(ctx, argExpr);
    if ( argExpr != NULL && argExpr->kind == XL6_AST_ARRAY_LITERAL ) {
        if ( xl6CTextEquals(member->text, "all") ) {
            xl6OutputPuts("xl6_task_all_values(", ctx->out);
            xl6CgenEmitXrtTypeDescRef(ctx, valueType);
            xl6OutputPrintf(ctx->out, ", \"\", %d, %d, %d", member->line, member->column, (int)argExpr->childCount);
            xl6CgenTaskFutureLiteralArgs(ctx, argExpr);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6OutputPrintf(ctx->out, "xl6_task_race_values(\"\", %d, %d, %d",
                            member->line,
                            member->column,
                            (int)argExpr->childCount);
            xl6CgenTaskFutureLiteralArgs(ctx, argExpr);
            xl6OutputPutc(')', ctx->out);
        }
        xl6AstFree(valueType);
        return 1;
    }

    if ( xl6CTextEquals(member->text, "all") ) {
        xl6OutputPuts("xl6_task_all_from_array((xtarray)(", ctx->out);
        xl6CgenExpr(ctx, argExpr);
        xl6OutputPuts("), ", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, valueType);
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d)", member->line, member->column);
    } else {
        xl6OutputPuts("xl6_task_race_from_array((xtarray)(", ctx->out);
        xl6CgenExpr(ctx, argExpr);
        xl6OutputPrintf(ctx->out, "), \"\", %d, %d)", member->line, member->column);
    }
    xl6AstFree(valueType);
    return 1;
}

static const XL6_AstNode* xl6CgenCallArgExpr(const XL6_AstNode* call, size_t index)
{
    const XL6_AstNode* args;
    const XL6_AstNode* arg;

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    arg = args != NULL && index < args->childCount ? args->children[index] : NULL;
    return arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
}

static int xl6CgenSyncCtorCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* callee)
{
    if ( callee == NULL || callee->kind != XL6_AST_NAME || !xl6CgenIsSyncHandleTypeName(callee->text) ) {
        return 0;
    }
    if ( xl6CTextEquals(callee->text, "mutex") ) {
        xl6OutputPrintf(ctx->out,
                        "xl6_sync_require_handle(xrtMutexCreate(), \"mutex create failed\", \"\", %d, %d)",
                        callee->line,
                        callee->column);
        return 1;
    }
    if ( xl6CTextEquals(callee->text, "cond") ) {
        xl6OutputPrintf(ctx->out,
                        "xl6_sync_require_handle(xrtCondCreate(), \"cond create failed\", \"\", %d, %d)",
                        callee->line,
                        callee->column);
        return 1;
    }
    if ( xl6CTextEquals(callee->text, "rwlock") ) {
        xl6OutputPrintf(ctx->out,
                        "xl6_sync_require_handle(xrtRWLockCreate(), \"rwlock create failed\", \"\", %d, %d)",
                        callee->line,
                        callee->column);
        return 1;
    }
    if ( xl6CTextEquals(callee->text, "sem") ) {
        xl6OutputPuts("xl6_sync_require_handle(xrtSemCreate(", ctx->out);
        xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 0));
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 1));
        xl6OutputPrintf(ctx->out, "), \"sem create failed\", \"\", %d, %d)", callee->line, callee->column);
        return 1;
    }
    return 0;
}

static void xl6CgenSyncReceiver(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    if ( receiver != NULL && receiver->kind == XL6_AST_NAME ) {
        if ( xl6CgenAnonCaptureIndex(ctx->currentAnonFunc, receiver->text) >= 0 ) {
            xl6OutputPuts("__env->", ctx->out);
            xl6CgenFputsCIdent(ctx->out, receiver->text);
            return;
        }
        xl6CgenFputsCIdent(ctx->out, receiver->text);
        return;
    }
    xl6CgenExpr(ctx, receiver);
}

static int xl6CgenSyncMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const char* typeName;

    if ( call == NULL || member == NULL || member->kind != XL6_AST_MEMBER ) {
        return 0;
    }
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return 0;
    }
    receiverType = xl6CgenFindVarType(ctx, receiver->text);
    if ( !xl6CgenIsSyncHandleType(receiverType) ) {
        return 0;
    }
    typeName = receiverType->text;

    if ( xl6CTextEquals(typeName, "mutex") ) {
        if ( xl6CTextEquals(member->text, "lock") ) {
            xl6OutputPuts("(xrtMutexLock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "tryLock") ) {
            xl6OutputPuts("xrtMutexTryLock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "unlock") ) {
            xl6OutputPuts("(xrtMutexUnlock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
    }

    if ( xl6CTextEquals(typeName, "sem") ) {
        if ( xl6CTextEquals(member->text, "wait") ) {
            xl6OutputPuts("(xrtSemWait(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "tryWait") ) {
            xl6OutputPuts("xrtSemTryWait(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "waitTimeout") ) {
            xl6OutputPuts("(xrtSemWaitTimeout(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 0));
            xl6OutputPuts(") == 0)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "post") ) {
            xl6OutputPuts("xrtSemPost(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "postMultiple") ) {
            xl6OutputPuts("xrtSemPostMultiple(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 0));
            xl6OutputPutc(')', ctx->out);
            return 1;
        }
    }

    if ( xl6CTextEquals(typeName, "cond") ) {
        if ( xl6CTextEquals(member->text, "wait") ) {
            xl6OutputPuts("(xrtCondWait(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 0));
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "waitTimeout") ) {
            xl6OutputPuts("(xrtCondWaitTimeout(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 0));
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 1));
            xl6OutputPuts(") == 0)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "signal") ) {
            xl6OutputPuts("(xrtCondSignal(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "broadcast") ) {
            xl6OutputPuts("(xrtCondBroadcast(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
    }

    if ( xl6CTextEquals(typeName, "rwlock") ) {
        if ( xl6CTextEquals(member->text, "readLock") ) {
            xl6OutputPuts("(xrtRWLockReadLock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "tryReadLock") ) {
            xl6OutputPuts("xrtRWLockTryReadLock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "readUnlock") ) {
            xl6OutputPuts("(xrtRWLockReadUnlock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "writeLock") ) {
            xl6OutputPuts("(xrtRWLockWriteLock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "tryWriteLock") ) {
            xl6OutputPuts("xrtRWLockTryWriteLock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "writeUnlock") ) {
            xl6OutputPuts("(xrtRWLockWriteUnlock(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "downgrade") ) {
            xl6OutputPuts("(xrtRWLockDowngrade(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPuts("), true)", ctx->out);
            return 1;
        }
        if ( xl6CTextEquals(member->text, "upgrade") ) {
            xl6OutputPuts("xrtRWLockUpgrade(", ctx->out);
            xl6CgenSyncReceiver(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
            return 1;
        }
    }

    return 0;
}

static XL6_AstNode* xl6CgenThreadEntryType(const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* params;
    XL6_AstNode* returns;
    XL6_AstNode* intType;

    typeNode = xl6CgenNewSimpleType("function", at);
    params = xl6AstNew(XL6_AST_LIST, NULL);
    returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    intType = xl6CgenNewSimpleType("int", at);
    if ( typeNode == NULL || params == NULL || returns == NULL || intType == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(params);
        xl6AstFree(returns);
        xl6AstFree(intType);
        return NULL;
    }
    xl6AstAddChild(returns, intType);
    xl6AstAddChild(typeNode, params);
    xl6AstAddChild(typeNode, returns);
    return typeNode;
}

static int xl6CgenThreadCtorCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* callee)
{
    const XL6_AstNode* entryExpr;
    XL6_AstNode* targetType;

    if ( callee == NULL || callee->kind != XL6_AST_NAME || !xl6CTextEquals(callee->text, "thread") ) {
        return 0;
    }

    entryExpr = xl6CgenCallArgExpr(call, 0);
    targetType = xl6CgenThreadEntryType(callee);
    xl6OutputPuts("xl6_thread_require_handle(xl6_thread_start(", ctx->out);
    if ( targetType != NULL && entryExpr != NULL ) {
        xl6CgenFunctionValueExpr(ctx, entryExpr, targetType);
    } else {
        xl6OutputPuts("(XL6_Function){0}", ctx->out);
    }
    xl6OutputPrintf(ctx->out,
                    ", %d, \"\", %d, %d), \"\", %d, %d)",
                    xl6CgenExprIsFunctionBorrow(ctx, entryExpr) ? 1 : 0,
                    callee->line,
                    callee->column,
                    callee->line,
                    callee->column);
    xl6AstFree(targetType);
    return 1;
}

static void xl6CgenThreadReceiver(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    if ( receiver != NULL && receiver->kind == XL6_AST_NAME ) {
        if ( xl6CgenAnonCaptureIndex(ctx->currentAnonFunc, receiver->text) >= 0 ) {
            xl6OutputPuts("__env->", ctx->out);
            xl6CgenFputsCIdent(ctx->out, receiver->text);
            return;
        }
        xl6CgenFputsCIdent(ctx->out, receiver->text);
        return;
    }
    xl6CgenExpr(ctx, receiver);
}

static int xl6CgenThreadMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;

    if ( call == NULL || member == NULL || member->kind != XL6_AST_MEMBER ) {
        return 0;
    }
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return 0;
    }
    receiverType = xl6CgenFindVarType(ctx, receiver->text);
    if ( !xl6CgenIsThreadType(receiverType) ) {
        return 0;
    }

    if ( xl6CTextEquals(member->text, "wait") ) {
        xl6OutputPuts("(xrtThreadWait(", ctx->out);
        xl6CgenThreadReceiver(ctx, receiver);
        xl6OutputPuts("), true)", ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(member->text, "waitTimeout") ) {
        xl6OutputPuts("(xrtThreadWaitTimeout(", ctx->out);
        xl6CgenThreadReceiver(ctx, receiver);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, xl6CgenCallArgExpr(call, 0));
        xl6OutputPuts(") == 0)", ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(member->text, "stop") ) {
        xl6OutputPuts("(xrtThreadStop(", ctx->out);
        xl6CgenThreadReceiver(ctx, receiver);
        xl6OutputPuts("), true)", ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(member->text, "state") ) {
        xl6OutputPuts("xrtThreadGetState(", ctx->out);
        xl6CgenThreadReceiver(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(member->text, "exitCode") ) {
        xl6OutputPuts("(int)xrtThreadGetExitCode(", ctx->out);
        xl6CgenThreadReceiver(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        return 1;
    }
    return 0;
}
