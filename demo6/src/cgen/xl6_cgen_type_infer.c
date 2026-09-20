#include "src/cgen/xl6_cgen_type_infer_async.c"

static XL6_AstNode* xl6CgenCloneAsyncCallType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* func,
    const XL6_AstNode* typeArgs,
    const XL6_AstNode* at)
{
    const XL6_AstNode* returns;
    XL6_AstNode* valueType;

    returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
    if ( returns != NULL && returns->childCount == 1 ) {
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;

        if ( typeArgs != NULL ) {
            ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
            ctx->currentGenericArgs = typeArgs;
        }
        valueType = xl6CgenCloneCanonicalTypeNode(ctx, returns->children[0]);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
    } else {
        valueType = xl6CgenNewSimpleType("null", at != NULL ? at : func);
    }

    if ( xl6CgenFuncReturnsGenerator(ctx, func) ) {
        valueType->flags &= ~XL6_AST_FLAG_ASYNC;
        return valueType;
    }
    return xl6CgenNewGenericTypeNode("future", valueType, at != NULL ? at : func);
}

static XL6_AstNode* xl6CgenNewFunctionTypeFromParts(const XL6_AstNode* params, const XL6_AstNode* returns, const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* paramTypes;
    XL6_AstNode* returnTypes;
    size_t i;

    typeNode = xl6CgenNewSimpleType("function", at);
    paramTypes = xl6AstNew(XL6_AST_LIST, NULL);
    returnTypes = returns != NULL ? xl6CgenCloneTypeNode(returns) : xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    if ( typeNode == NULL || paramTypes == NULL || returnTypes == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(paramTypes);
        xl6AstFree(returnTypes);
        return NULL;
    }

    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            XL6_AstNode* typeCopy;
            const XL6_AstNode* param = params->children[i];
            if ( param == NULL || param->childCount == 0 ) {
                continue;
            }
            typeCopy = xl6CgenCloneTypeNode(param->children[0]);
            if ( typeCopy == NULL ) {
                xl6AstFree(typeNode);
                xl6AstFree(paramTypes);
                xl6AstFree(returnTypes);
                return NULL;
            }
            typeCopy->flags |= param->flags &
                (XL6_AST_FLAG_BYVAL | XL6_AST_FLAG_BYREF | XL6_AST_FLAG_OUT);
            xl6AstAddChild(paramTypes, typeCopy);
        }
    }

    xl6AstAddChild(typeNode, paramTypes);
    xl6AstAddChild(typeNode, returnTypes);
    return typeNode;
}

static XL6_AstNode* xl6CgenAnonFunctionType(const XL6_AstNode* anon)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    if ( anon == NULL || anon->kind != XL6_AST_ANON_FUNC ) {
        return NULL;
    }
    params = anon->childCount > 0 ? anon->children[0] : NULL;
    returns = anon->childCount > 1 ? anon->children[1] : NULL;
    return xl6CgenNewFunctionTypeFromParts(params, returns, anon);
}

static XL6_AstNode* xl6CgenGenericClassCtorType(XL6_CgenContext* ctx, const XL6_AstNode* call, XL6_AstNode** outClass)
{
    const XL6_AstNode* instant;
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArgs;
    XL6_AstNode* classNode;
    XL6_AstNode* typeNode;
    XL6_AstNode* argsCopy;
    const XL6_AstNode* classParams;

    if ( outClass != NULL ) {
        *outClass = NULL;
    }
    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return NULL;
    }
    instant = call->children[0];
    if ( instant == NULL || instant->kind != XL6_AST_GENERIC_INSTANT || instant->childCount < 2 ) {
        return NULL;
    }
    nameNode = instant->children[0];
    typeArgs = instant->children[1];
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME || typeArgs == NULL ) {
        return NULL;
    }

    classNode = xl6CgenFindClass(ctx, nameNode->text);
    if ( classNode == NULL || !xl6CgenClassHasTypeParams(classNode) ) {
        return NULL;
    }
    classParams = xl6CgenClassTypeParams(classNode);
    if ( classParams == NULL || classParams->childCount != typeArgs->childCount ) {
        xl6CgenError(ctx, call, "泛型 class 构造类型参数数量不匹配");
        return NULL;
    }

    typeNode = xl6CgenNewSimpleType(nameNode->text, nameNode);
    argsCopy = xl6CgenCloneCanonicalTypeNode(ctx, typeArgs);
    if ( typeNode == NULL || argsCopy == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(argsCopy);
        xl6CgenError(ctx, call, "泛型 class 构造类型节点内存不足");
        return NULL;
    }
    xl6AstAddChild(typeNode, argsCopy);
    if ( outClass != NULL ) {
        *outClass = classNode;
    }
    return typeNode;
}

static XL6_AstNode* xl6CgenChannelCtorType(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* instant;
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArgs;
    XL6_AstNode* typeNode;
    XL6_AstNode* argsCopy;

    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return NULL;
    }
    instant = call->children[0];
    if ( instant == NULL || instant->kind != XL6_AST_GENERIC_INSTANT || instant->childCount < 2 ) {
        return NULL;
    }
    nameNode = instant->children[0];
    typeArgs = instant->children[1];
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME ||
         !xl6CTextEquals(nameNode->text, "channel") ||
         typeArgs == NULL || typeArgs->childCount != 1 ) {
        return NULL;
    }

    typeNode = xl6CgenNewSimpleType("channel", nameNode);
    argsCopy = xl6CgenCloneCanonicalTypeNode(ctx, typeArgs);
    if ( typeNode == NULL || argsCopy == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(argsCopy);
        xl6CgenError(ctx, call, "channel type inference out of memory");
        return NULL;
    }
    xl6AstAddChild(typeNode, argsCopy);
    return typeNode;
}

static XL6_AstNode* xl6CgenChannelMethodType(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* member;
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode* result;

    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return NULL;
    }
    member = call->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER ) {
        return NULL;
    }
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( !xl6CgenIsChannelType(receiverType) ) {
        xl6AstFree(receiverType);
        return NULL;
    }

    result = NULL;
    if ( xl6CTextEquals(member->text, "send") ||
         xl6CTextEquals(member->text, "trySend") ||
         xl6CTextEquals(member->text, "recvInto") ||
         xl6CTextEquals(member->text, "tryRecvInto") ||
         xl6CTextEquals(member->text, "recvTimeoutInto") ||
         xl6CTextEquals(member->text, "isClosed") ) {
        result = xl6CgenNewSimpleType("bool", call);
    } else if ( xl6CTextEquals(member->text, "count") ) {
        result = xl6CgenNewSimpleType("int", call);
    }
    xl6AstFree(receiverType);
    return result;
}

static XL6_AstNode* xl6CgenFutureMethodType(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* member;
    const XL6_AstNode* receiver;
    const XL6_AstNode* args;
    const XL6_AstNode* callbackArg;
    const XL6_AstNode* callbackExpr;
    const XL6_AstNode* callbackReturns;
    XL6_AstNode* receiverType;
    XL6_AstNode* callbackType;
    XL6_AstNode* valueType;
    XL6_AstNode* result;

    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return NULL;
    }
    member = call->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER ) {
        return NULL;
    }
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( !xl6CgenIsFutureLikeType(receiverType) ) {
        xl6AstFree(receiverType);
        return NULL;
    }

    result = NULL;
    if ( xl6CTextEquals(member->text, "wait") ||
         xl6CTextEquals(member->text, "waitTimeout") ||
         xl6CTextEquals(member->text, "waitUntil") ||
         xl6CTextEquals(member->text, "setDebugName") ||
         xl6CTextEquals(member->text, "cancel") ||
         xl6CTextEquals(member->text, "isDone") ||
         xl6CTextEquals(member->text, "isPending") ||
         xl6CTextEquals(member->text, "isCancelled") ||
         xl6CTextEquals(member->text, "isFailed") ) {
        result = xl6CgenNewSimpleType("bool", call);
    } else if ( xl6CTextEquals(member->text, "state") ||
                xl6CTextEquals(member->text, "status") ||
                xl6CTextEquals(member->text, "createTimeMs") ||
                xl6CTextEquals(member->text, "completeTimeMs") ||
                xl6CTextEquals(member->text, "pendingContinuations") ) {
        result = xl6CgenNewSimpleType("int", call);
    } else if ( xl6CTextEquals(member->text, "error") ) {
        result = xl6CgenNewSimpleType("error", call);
    } else if ( xl6CTextEquals(member->text, "debugName") ) {
        result = xl6CgenNewSimpleType("string", call);
    } else if ( xl6CTextEquals(member->text, "catch") ||
                xl6CTextEquals(member->text, "finally") ) {
        result = receiverType;
        receiverType = NULL;
    } else if ( xl6CTextEquals(member->text, "then") ) {
        args = call->childCount > 1 ? call->children[1] : NULL;
        callbackArg = args != NULL && args->childCount == 1 ? args->children[0] : NULL;
        callbackExpr = callbackArg != NULL && callbackArg->childCount > 0 ? callbackArg->children[0] : NULL;
        callbackType = xl6CgenCloneExprTypeForInference(ctx, callbackExpr);
        callbackReturns = xl6CgenFunctionReturnTypes(callbackType);
        valueType = callbackReturns != NULL && callbackReturns->childCount == 1
            ? xl6CgenCloneCanonicalTypeNode(ctx, callbackReturns->children[0])
            : NULL;
        if ( valueType != NULL ) {
            result = xl6CgenNewGenericTypeNode("future", valueType, call);
        }
        xl6AstFree(callbackType);
    }
    xl6AstFree(receiverType);
    return result;
}

static XL6_AstNode* xl6CgenSyncCtorType(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* callee;

    (void)ctx;
    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return NULL;
    }
    callee = call->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME || !xl6CgenIsSyncHandleTypeName(callee->text) ) {
        return NULL;
    }
    return xl6CgenNewSimpleType(callee->text, callee);
}

static XL6_AstNode* xl6CgenThreadCtorType(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* callee;

    (void)ctx;
    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return NULL;
    }
    callee = call->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME || !xl6CTextEquals(callee->text, "thread") ) {
        return NULL;
    }
    return xl6CgenNewSimpleType("thread", callee);
}

static XL6_AstNode* xl6CgenThreadMethodType(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* member;
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode* result;

    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return NULL;
    }
    member = call->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER ) {
        return NULL;
    }
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( !xl6CgenIsThreadType(receiverType) ) {
        xl6AstFree(receiverType);
        return NULL;
    }

    result = NULL;
    if ( xl6CTextEquals(member->text, "wait") ||
         xl6CTextEquals(member->text, "waitTimeout") ||
         xl6CTextEquals(member->text, "stop") ) {
        result = xl6CgenNewSimpleType("bool", call);
    } else if ( xl6CTextEquals(member->text, "state") ||
                xl6CTextEquals(member->text, "exitCode") ) {
        result = xl6CgenNewSimpleType("int", call);
    }
    xl6AstFree(receiverType);
    return result;
}

static XL6_AstNode* xl6CgenSyncMethodType(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* member;
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;

    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return NULL;
    }
    member = call->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER ) {
        return NULL;
    }
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( !xl6CgenIsSyncHandleType(receiverType) ) {
        xl6AstFree(receiverType);
        return NULL;
    }
    xl6AstFree(receiverType);
    return xl6CgenNewSimpleType("bool", call);
}

#include "src/cgen/xl6_cgen_type_infer_task.c"
static XL6_AstNode* xl6CgenCloneSingleCallableReturnType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* func,
    const XL6_AstNode* receiverType)
{
    const XL6_AstNode* returns;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    XL6_AstNode* result;

    returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
    if ( returns == NULL || returns->childCount != 1 ) {
        return NULL;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( receiverType != NULL &&
         receiverType->childCount > 0 &&
         func != NULL &&
         ctx != NULL &&
         ctx->currentClass != NULL &&
         xl6CgenClassHasTypeParams(ctx->currentClass) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(ctx->currentClass);
        ctx->currentGenericArgs = receiverType->children[0];
    }
    result = xl6CgenCloneCanonicalTypeNode(ctx, returns->children[0]);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    return result;
}

static XL6_AstNode* xl6CgenCloneClassMethodCallType(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* member;
    const XL6_AstNode* receiver;
    const XL6_AstNode* ownerClass;
    XL6_AstNode* classNode;
    XL6_AstNode* receiverType;
    const XL6_AstNode* method;
    XL6_AstNode* result;
    const XL6_AstNode* prevClass;

    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return NULL;
    }
    member = call->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL || member->childCount == 0 ) {
        return NULL;
    }

    classNode = xl6CgenClassDeclFromTypeMemberReceiver(ctx, member);
    if ( classNode != NULL ) {
        ownerClass = NULL;
        method = xl6CgenResolveStaticClassFunc(ctx, classNode, call, member->text, &ownerClass);
        return xl6CgenCloneSingleCallableReturnType(ctx, method, NULL);
    }

    receiver = member->children[0];
    receiverType = NULL;
    classNode = NULL;
    if ( receiver != NULL && receiver->kind == XL6_AST_NAME &&
         xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        classNode = (XL6_AstNode*)ctx->currentClass;
    } else if ( receiver != NULL && receiver->kind == XL6_AST_NAME &&
                xl6CTextEquals(receiver->text, "base") && ctx->currentClass != NULL ) {
        const XL6_AstNode* baseType = ctx->currentClass->childCount > 0
            ? ctx->currentClass->children[0]
            : NULL;

        classNode = xl6CgenBaseClass(ctx, ctx->currentClass);
        receiverType = xl6CgenCloneCanonicalTypeNode(ctx, baseType);
    } else {
        receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
        {
            const XL6_AstNode* classType = xl6CgenClassHandleTargetType(ctx, receiverType);
            if ( classType == NULL ) {
                classType = receiverType;
            }
            classNode = classType != NULL ? xl6CgenFindClass(ctx, classType->text) : NULL;
        }
    }
    if ( classNode == NULL ) {
        xl6AstFree(receiverType);
        return NULL;
    }

    prevClass = ctx->currentClass;
    ctx->currentClass = classNode;
    ownerClass = NULL;
    method = xl6CgenResolveClassFunc(ctx, classNode, call, member->text, &ownerClass);
    if ( ownerClass != NULL ) {
        ctx->currentClass = ownerClass;
    }
    result = xl6CgenCloneSingleCallableReturnType(ctx, method, receiverType);
    ctx->currentClass = prevClass;
    xl6AstFree(receiverType);
    return result;
}

#include "xl6_cgen_type_infer_expr.c"
