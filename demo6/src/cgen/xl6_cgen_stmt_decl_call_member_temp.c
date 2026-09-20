static int xl6CgenDeclMemberFunctionTempReceiverCallInfo(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* expr,
    const XL6_AstNode** outReceiver,
    const XL6_AstNode** outCallee,
    XL6_AstNode** outReceiverType,
    XL6_AstNode** outFunctionType,
    XL6_AstNode** outClassNode)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* receiver;
    const XL6_AstNode* returns;
    XL6_AstNode* receiverType;
    XL6_AstNode* functionType;
    XL6_AstNode* classNode;
    XL6_AstNode* targetCanonical;
    XL6_AstNode* returnCanonical;
    size_t replacementIndex;
    int matches;

    if ( outReceiver != NULL ) {
        *outReceiver = NULL;
    }
    if ( outCallee != NULL ) {
        *outCallee = NULL;
    }
    if ( outReceiverType != NULL ) {
        *outReceiverType = NULL;
    }
    if ( outFunctionType != NULL ) {
        *outFunctionType = NULL;
    }
    if ( outClassNode != NULL ) {
        *outClassNode = NULL;
    }

    if ( ctx == NULL || typeNode == NULL || expr == NULL ||
         expr->kind != XL6_AST_CALL || expr->childCount == 0 ||
         !xl6CgenDeclCallArgStaticScalarType(typeNode) ) {
        return 0;
    }

    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER || callee->childCount == 0 ) {
        return 0;
    }

    receiver = callee->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_CALL ) {
        return 0;
    }
    for ( replacementIndex = 0; replacementIndex < ctx->exprReplacementCount; ++replacementIndex ) {
        if ( ctx->exprReplacements[replacementIndex].expr == receiver &&
             ctx->exprReplacements[replacementIndex].cName != NULL ) {
            /* 接收者已由外层语句级 lowering 物化，统一表达式路径会复用该句柄。 */
            return 0;
        }
    }

    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    classNode = xl6CgenClassNodeForType(ctx, receiverType);
    if ( classNode == NULL || !xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        xl6AstFree(receiverType);
        return 0;
    }

    functionType = xl6CgenCloneMemberFieldTypeForInference(ctx, callee);
    returns = xl6CgenFunctionReturnTypes(functionType);
    targetCanonical = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
    returnCanonical = returns != NULL && returns->childCount == 1
        ? xl6CgenCloneCanonicalTypeNode(ctx, returns->children[0])
        : NULL;

    matches = xl6CgenIsFunctionType(functionType) &&
              returns != NULL &&
              returns->childCount == 1 &&
              targetCanonical != NULL &&
              returnCanonical != NULL &&
              xl6CgenTypeNodeEqual(targetCanonical, returnCanonical);

    xl6AstFree(targetCanonical);
    xl6AstFree(returnCanonical);

    if ( !matches ) {
        xl6AstFree(functionType);
        xl6AstFree(receiverType);
        return 0;
    }

    if ( outReceiver != NULL ) {
        *outReceiver = receiver;
    }
    if ( outCallee != NULL ) {
        *outCallee = callee;
    }
    if ( outReceiverType != NULL ) {
        *outReceiverType = receiverType;
    } else {
        xl6AstFree(receiverType);
    }
    if ( outFunctionType != NULL ) {
        *outFunctionType = functionType;
    } else {
        xl6AstFree(functionType);
    }
    if ( outClassNode != NULL ) {
        *outClassNode = classNode;
    }
    return 1;
}

static int xl6CgenDeclCanEmitMemberFunctionTempReceiverCallTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* expr)
{
    return xl6CgenDeclMemberFunctionTempReceiverCallInfo(ctx, typeNode, expr, NULL, NULL, NULL, NULL, NULL);
}

static int xl6CgenDeclTryMemberFunctionTempReceiverCallTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    XL6_AstNode* receiverType;
    XL6_AstNode* functionType;
    XL6_AstNode* classNode;
    XL6_AstNode* receiverFunctionType;
    int id;
    char receiverTempName[64];
    char functionTempName[64];
    char receiverFunctionTempName[64];

    if ( cType == NULL || cName == NULL ||
         !xl6CgenDeclMemberFunctionTempReceiverCallInfo(ctx,
                                                        typeNode,
                                                        expr,
                                                        &receiver,
                                                        &callee,
                                                        &receiverType,
                                                        &functionType,
                                                        &classNode) ) {
        return 0;
    }

    args = expr->childCount > 1 ? expr->children[1] : NULL;
    id = ctx->tempId++;
    snprintf(receiverTempName, sizeof(receiverTempName), "__xl6_member_recv_%d", id);
    snprintf(functionTempName, sizeof(functionTempName), "__xl6_member_fn_call_%d", id);
    snprintf(receiverFunctionTempName, sizeof(receiverFunctionTempName), "__xl6_member_fn_stmt_%d", id);

    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = {0};\n", xl6CgenTypeName(ctx, receiverType), receiverTempName);

    receiverFunctionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, receiver);
    if ( receiverFunctionType != NULL ) {
        xl6CgenEmitMemberFunctionStatementTemp(ctx, receiver, receiverFunctionTempName);
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", receiverTempName);
    if ( receiverFunctionType != NULL ) {
        xl6CgenMemberFunctionStatementInvokeExpr(ctx, receiver, receiverFunctionType, receiverFunctionTempName);
    } else {
        xl6CgenExpr(ctx, receiver);
    }
    xl6OutputPuts(";\n", ctx->out);

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( receiverType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = receiverType->children[0];
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "XL6_Function %s = ", functionTempName);
    if ( xl6CgenFindClassFieldNode(ctx, classNode, callee->text) == NULL ) {
        xl6CgenError(ctx, callee, "class function field not found");
        xl6OutputPuts("(XL6_Function){0}", ctx->out);
    } else if ( !xl6CgenEmitClassFieldFromNamedReceiver(
                    ctx,
                    receiverType,
                    classNode,
                    receiverTempName,
                    callee->text,
                    callee) ) {
        xl6CgenError(ctx, callee, "class function field suffix could not be emitted");
    }
    xl6OutputPuts(";\n", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = (%s)(", cName, cType);
    xl6CgenFunctionInvoke(ctx, functionTempName, functionType, args);
    xl6OutputPuts(");\n", ctx->out);

    xl6CgenDeclDestroyTemporaryClassValue(ctx, classNode, receiverType, receiverTempName);

    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);

    xl6AstFree(receiverFunctionType);
    xl6AstFree(functionType);
    xl6AstFree(receiverType);
    return 1;
}
