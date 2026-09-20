static const XL6_AstNode* xl6CgenResolveClassProperty(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* call,
    const char* name,
    XL6_AstKind kind,
    const XL6_AstNode** outOwnerClass)
{
    const XL6_AstNode* method;
    XL6_AstNode* baseClass;

    if ( outOwnerClass != NULL ) {
        *outOwnerClass = NULL;
    }
    if ( classNode == NULL || name == NULL || (kind != XL6_AST_GET && kind != XL6_AST_LET) ) {
        return NULL;
    }

    method = xl6CgenResolveCallableInBody(ctx, xl6CgenClassBody(classNode), call, kind, name, 0);
    if ( method != NULL ) {
        if ( outOwnerClass != NULL ) {
            *outOwnerClass = classNode;
        }
        return method;
    }

    baseClass = xl6CgenBaseClass(ctx, classNode);
    if ( baseClass == NULL ) {
        return NULL;
    }
    return xl6CgenResolveClassProperty(ctx, baseClass, call, name, kind, outOwnerClass);
}

static XL6_AstNode* xl6CgenPropertyTempCall(const XL6_AstNode* atNode, const XL6_AstNode* indexNode, const XL6_AstNode* valueExpr)
{
    XL6_AstNode* call;
    size_t i;

    call = xl6CgenProtocolTempCallNew(atNode);
    if ( call == NULL ) {
        return NULL;
    }
    for ( i = 0; indexNode != NULL && i < xl6CgenIndexArgCount(indexNode); ++i ) {
        if ( !xl6CgenProtocolTempCallAddArg(call, xl6CgenIndexArgAt(indexNode, i)) ) {
            xl6CgenProtocolTempCallFree(call);
            return NULL;
        }
    }
    if ( valueExpr != NULL && !xl6CgenProtocolTempCallAddArg(call, valueExpr) ) {
        xl6CgenProtocolTempCallFree(call);
        return NULL;
    }
    return call;
}

static int xl6CgenClassPropertyResolve(
    XL6_CgenContext* ctx,
    const XL6_AstNode* member,
    const XL6_AstNode* indexNode,
    const XL6_AstNode* valueExpr,
    XL6_AstKind kind,
    const XL6_AstNode* semanticSource,
    const XL6_AstNode** outClass,
    const XL6_AstNode** outOwnerClass,
    const XL6_AstNode** outMethod,
    XL6_AstNode** outReceiverType,
    XL6_AstNode** outCall)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* hirTarget;
    XL6_AstNode* call;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;

    if ( outClass != NULL ) {
        *outClass = NULL;
    }
    if ( outOwnerClass != NULL ) {
        *outOwnerClass = NULL;
    }
    if ( outMethod != NULL ) {
        *outMethod = NULL;
    }
    if ( outReceiverType != NULL ) {
        *outReceiverType = NULL;
    }
    if ( outCall != NULL ) {
        *outCall = NULL;
    }
    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }

    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    concreteType = xl6CgenClassHandleTargetType(ctx, receiverType);
    if ( concreteType == NULL ) {
        concreteType = xl6CgenResolveAliasType(ctx, receiverType);
    }
    classNode = concreteType != NULL ? xl6CgenFindClass(ctx, concreteType->text) : NULL;
    if ( classNode == NULL || xl6CgenFindClassFieldNode(ctx, classNode, member->text) != NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }

    call = xl6CgenPropertyTempCall(member, indexNode, valueExpr);
    if ( call == NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( concreteType != NULL && concreteType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = concreteType->children[0];
    }
    ownerClass = NULL;
    /*
     * obj.property[index] 可能是两种完全不同的语义：
     * 1. 带维度参数的属性 getter；
     * 2. 无参数 getter 返回容器，再对返回值做普通索引。
     *
     * member 上的 HIR 目标只描述无参数的属性访问，不能拿它替代整个
     * index 表达式的解析，否则第二种写法会丢失 index。存在索引参数时
     * 必须重新按完整参数表解析；找不到维度 getter 后交回普通容器路径。
     */
    if ( semanticSource == NULL ) {
        semanticSource = indexNode != NULL ? indexNode : member;
    }
    hirTarget = semanticSource != NULL &&
                (semanticSource->semanticCallKind == XL6_SEM_CALL_PROPERTY_GET ||
                 semanticSource->semanticCallKind == XL6_SEM_CALL_PROPERTY_SET ||
                 semanticSource->semanticCallKind == XL6_SEM_CALL_WITNESS)
        ? xl6CgenHirTarget(ctx, semanticSource)
        : NULL;
    if ( hirTarget != NULL ) {
        ownerClass = xl6CgenFindCallableOwnerClass(ctx, classNode, hirTarget);
        method = ownerClass != NULL &&
                 hirTarget->kind == kind &&
                 xl6CTextEquals(xl6CgenCallableName(hirTarget), member->text)
            ? hirTarget
            : NULL;
        if ( method == NULL ) {
            xl6CgenError(ctx, semanticSource, "typed HIR 属性访问器目标无效");
        }
    } else if ( semanticSource == NULL ||
                semanticSource->semanticCallKind == XL6_SEM_CALL_NONE ) {
        method = xl6CgenResolveClassProperty(
            ctx,
            classNode,
            call,
            member->text,
            kind,
            &ownerClass
        );
    } else {
        /* A resolved non-property target (for example __field__ or __item__)
         * belongs to another semantic operation.  Do not reinterpret that
         * target, or re-resolve the same spelling, as a property accessor. */
        method = NULL;
    }
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    if ( method == NULL ) {
        xl6CgenProtocolTempCallFree(call);
        xl6AstFree(receiverType);
        return 0;
    }
    if ( ownerClass == NULL ) {
        ownerClass = classNode;
    }

    if ( outClass != NULL ) {
        *outClass = classNode;
    }
    if ( outOwnerClass != NULL ) {
        *outOwnerClass = ownerClass;
    }
    if ( outMethod != NULL ) {
        *outMethod = method;
    }
    if ( outReceiverType != NULL ) {
        *outReceiverType = receiverType;
    } else {
        xl6AstFree(receiverType);
    }
    if ( outCall != NULL ) {
        *outCall = call;
    } else {
        xl6CgenProtocolTempCallFree(call);
    }
    return 1;
}

static XL6_AstNode* xl6CgenCloneClassPropertyMemberType(XL6_CgenContext* ctx, const XL6_AstNode* member, const XL6_AstNode* indexNode)
{
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* returns;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    XL6_AstNode* receiverType;
    XL6_AstNode* result;

    if ( member != NULL &&
         member->semanticCallKind == XL6_SEM_CALL_WITNESS ) {
        return xl6CgenCloneExprTypeForInference(ctx, member);
    }

    receiverType = NULL;
    if ( !xl6CgenClassPropertyResolve(
             ctx,
             member,
             indexNode,
             NULL,
             XL6_AST_GET,
             indexNode != NULL ? indexNode : member,
             &classNode,
             &ownerClass,
             &method,
             &receiverType,
             NULL) ) {
        return NULL;
    }
    returns = method != NULL && method->childCount > 2 ? method->children[2] : NULL;
    if ( returns == NULL || returns->childCount != 1 ) {
        xl6AstFree(receiverType);
        return NULL;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    {
        const XL6_AstNode* concreteReceiver = xl6CgenClassHandleTargetType(ctx, receiverType);
        if ( concreteReceiver == NULL ) {
            concreteReceiver = receiverType;
        }
        if ( concreteReceiver != NULL && concreteReceiver->childCount > 0 &&
         ownerClass != NULL && xl6CgenClassHasTypeParams(ownerClass) ) {
            ctx->currentGenericParams = xl6CgenClassTypeParams(ownerClass);
            ctx->currentGenericArgs = concreteReceiver->children[0];
        }
    }
    result = xl6CgenCloneCanonicalTypeNode(ctx, returns->children[0]);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    xl6AstFree(receiverType);
    (void)classNode;
    return result;
}

static int xl6CgenClassPropertyEmitSelf(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiver,
    const XL6_AstNode* receiverType,
    const XL6_AstNode* classNode,
    const XL6_AstNode* ownerClass)
{
    char basePath[256];

    if ( receiver == NULL ) {
        xl6CgenError(ctx, receiver, "property receiver is missing");
        xl6OutputPuts("0", ctx->out);
        return 0;
    }
    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(ctx, classNode, ownerClass, basePath, sizeof(basePath)) ) {
        xl6CgenError(ctx, receiver, "cannot generate property base path");
        basePath[0] = '\0';
    }
    if ( receiver->kind == XL6_AST_NAME && xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        if ( basePath[0] == '\0' ) {
            xl6OutputPuts("self", ctx->out);
        } else {
            xl6OutputPuts("&self->base", ctx->out);
            if ( strlen(basePath) > 5 ) {
                xl6OutputPuts(basePath + 5, ctx->out);
            }
        }
        return 1;
    }
    /*
        成员函数中的 self 已经是对象 payload 指针。即使 class 在语言层使用
        xvalue* 句柄，也不能再次调用 xl6_class_data，否则会把 payload 当成 xvalue*。
    */
    if ( xl6CgenClassHandleTargetType(ctx, receiverType) != NULL ) {
        char ownerTypeName[512];
        const XL6_AstNode* concreteReceiver = xl6CgenClassHandleTargetType(ctx, receiverType);
        xl6CgenClassTypeRefName(ownerTypeName, sizeof(ownerTypeName), concreteReceiver);
        xl6OutputPrintf(ctx->out, "((%s*)xl6_class_data(", ownerTypeName);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d))", receiver->line, receiver->column);
        return 1;
    }
    if ( receiver->kind == XL6_AST_NAME ) {
        xl6OutputPutc('&', ctx->out);
        xl6CgenNamedValueReference(ctx, receiver->text);
        xl6OutputPuts(basePath, ctx->out);
        return 1;
    }
    if ( receiver->kind == XL6_AST_MEMBER || receiver->kind == XL6_AST_INDEX ) {
        xl6OutputPuts("&(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        xl6OutputPuts(basePath, ctx->out);
        return 1;
    }
    xl6CgenError(ctx, receiver, "property receiver must be addressable");
    xl6OutputPuts("0", ctx->out);
    return 0;
}

static int xl6CgenClassPropertyEmitCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* member,
    const XL6_AstNode* indexNode,
    const XL6_AstNode* valueExpr,
    XL6_AstKind kind,
    const XL6_AstNode* semanticSource)
{
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* receiver;
    const XL6_AstNode* params;
    XL6_AstNode* receiverType;
    XL6_AstNode* call;
    int fixedCount;

    if ( xl6CgenUserProtocolPropertyCall(
            ctx,
            member,
            indexNode,
            valueExpr,
            kind,
            semanticSource) ) {
        return 1;
    }
    if ( !xl6CgenClassPropertyResolve(ctx, member, indexNode, valueExpr, kind, semanticSource, &classNode, &ownerClass, &method, &receiverType, &call) ) {
        return 0;
    }

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( xl6CgenVirtualClassPropertyCall(ctx, member, call, receiver, classNode, ownerClass, method) ) {
        xl6CgenProtocolTempCallFree(call);
        xl6AstFree(receiverType);
        return 1;
    }

    {
        const XL6_AstNode* concreteReceiver = xl6CgenClassHandleTargetType(ctx, receiverType);
        if ( concreteReceiver == NULL ) {
            concreteReceiver = receiverType;
        }
        if ( concreteReceiver != NULL && concreteReceiver->childCount > 0 && xl6CgenClassHasTypeParams(ownerClass) ) {
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
        ctx->currentGenericParams = xl6CgenClassTypeParams(ownerClass);
            ctx->currentGenericArgs = concreteReceiver->children[0];
            xl6CgenGenericClassCallableSymbol(ctx, ownerClass, concreteReceiver, method);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        } else {
        xl6CgenClassCallableSymbol(ctx, ownerClass, method);
        }
    }

    xl6OutputPutc('(', ctx->out);
    xl6CgenClassPropertyEmitSelf(ctx, receiver, receiverType, classNode, ownerClass);
    params = xl6CgenCallableParams(method);
    fixedCount = xl6CgenFixedParamCount(params);
    if ( fixedCount > 0 ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenKnownCallArgs(ctx, method, call->childCount > 1 ? call->children[1] : NULL);
    }
    xl6OutputPutc(')', ctx->out);

    xl6CgenProtocolTempCallFree(call);
    xl6AstFree(receiverType);
    return 1;
}

static int xl6CgenClassPropertyMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    return xl6CgenClassPropertyEmitCall(ctx, member, NULL, NULL, XL6_AST_GET, member);
}

static int xl6CgenClassPropertyIndexExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;

    if ( node == NULL || (node->kind != XL6_AST_INDEX && node->kind != XL6_AST_SAFE_INDEX) || node->childCount == 0 ) {
        return 0;
    }
    receiver = node->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_MEMBER ) {
        return 0;
    }
    return xl6CgenClassPropertyEmitCall(ctx, receiver, node, NULL, XL6_AST_GET, node);
}

static int xl6CgenClassPropertyMemberSetStmt(XL6_CgenContext* ctx, const XL6_AstNode* operation, const XL6_AstNode* left, const XL6_AstNode* right)
{
    if ( left == NULL || left->kind != XL6_AST_MEMBER ) {
        return 0;
    }
    if ( !xl6CgenClassPropertyEmitCall(ctx, left, NULL, right, XL6_AST_LET, operation) ) {
        return 0;
    }
    xl6OutputPuts(";\n", ctx->out);
    return 1;
}

static int xl6CgenClassPropertyIndexSetStmt(XL6_CgenContext* ctx, const XL6_AstNode* operation, const XL6_AstNode* left, const XL6_AstNode* right)
{
    const XL6_AstNode* member;

    if ( left == NULL || left->kind != XL6_AST_INDEX || left->childCount == 0 ) {
        return 0;
    }
    member = left->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER ) {
        return 0;
    }
    if ( !xl6CgenClassPropertyEmitCall(ctx, member, left, right, XL6_AST_LET, operation) ) {
        return 0;
    }
    xl6OutputPuts(";\n", ctx->out);
    return 1;
}

static int xl6CgenClassPropertyMemberHasGetter(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    XL6_AstNode* typeNode = xl6CgenCloneClassPropertyMemberType(ctx, member, NULL);
    int result = typeNode != NULL;
    xl6AstFree(typeNode);
    return result;
}

static int xl6CgenClassPropertyIndexHasGetter(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* member = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    XL6_AstNode* typeNode = member != NULL && member->kind == XL6_AST_MEMBER
        ? xl6CgenCloneClassPropertyMemberType(ctx, member, node)
        : NULL;
    int result = typeNode != NULL;
    xl6AstFree(typeNode);
    return result;
}
