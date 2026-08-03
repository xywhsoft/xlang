static int xl6CgenResolveClassProtocolReceiver(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiver,
    const char* protocolName,
    const XL6_AstNode** outClass,
    const XL6_AstNode** outOwnerClass,
    const XL6_AstNode** outMethod,
    XL6_AstNode** outReceiverType)
{
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;

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
    if ( ctx == NULL || receiver == NULL || protocolName == NULL ) {
        return 0;
    }

    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    concreteType = xl6CgenResolveAliasType(ctx, receiverType);
    if ( xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
        concreteType = xl6CgenClassHandleTargetType(ctx, concreteType);
    }
    classNode = concreteType != NULL && concreteType->text != NULL
        ? xl6CgenFindClass(ctx, concreteType->text)
        : NULL;
    if ( classNode == NULL && receiver->kind == XL6_AST_NAME &&
         xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        classNode = ctx->currentClass;
    }
    if ( classNode == NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }

    ownerClass = NULL;
    method = xl6CgenFindClassProtocolFunc(ctx, classNode, protocolName, &ownerClass);
    if ( method == NULL ) {
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
    return 1;
}

static int xl6CgenResolveClassProtocolReceiverForCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiver,
    const char* protocolName,
    const XL6_AstNode* call,
    const XL6_AstNode** outClass,
    const XL6_AstNode** outOwnerClass,
    const XL6_AstNode** outMethod,
    XL6_AstNode** outReceiverType)
{
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;

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
    if ( ctx == NULL || receiver == NULL || protocolName == NULL || call == NULL ) {
        return 0;
    }

    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    concreteType = xl6CgenResolveAliasType(ctx, receiverType);
    if ( xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
        concreteType = xl6CgenClassHandleTargetType(ctx, concreteType);
    }
    classNode = concreteType != NULL && concreteType->text != NULL
        ? xl6CgenFindClass(ctx, concreteType->text)
        : NULL;
    if ( classNode == NULL && receiver->kind == XL6_AST_NAME &&
         xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        classNode = ctx->currentClass;
    }
    if ( classNode == NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }

    ownerClass = NULL;
    if ( concreteType != NULL && concreteType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;

        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = concreteType->children[0];
        method = xl6CgenResolveClassFunc(ctx, classNode, call, protocolName, &ownerClass);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
    } else {
        method = xl6CgenResolveClassFunc(ctx, classNode, call, protocolName, &ownerClass);
    }
    if ( method == NULL ) {
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
    return 1;
}

static int xl6CgenProtocolCallAddIndexArgs(XL6_AstNode* call, const XL6_AstNode* node)
{
    size_t i;

    for ( i = 0; i < xl6CgenIndexArgCount(node); ++i ) {
        if ( !xl6CgenProtocolTempCallAddArg(call, xl6CgenIndexArgAt(node, i)) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6CgenClassProtocolReceiverIsAddressable(const XL6_AstNode* receiver)
{
    if ( receiver == NULL ) {
        return 0;
    }
    return receiver->kind == XL6_AST_NAME;
}

static int xl6CgenEmitClassProtocolSelf(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiver,
    const XL6_AstNode* classNode,
    const XL6_AstNode* ownerClass)
{
    char basePath[256];

    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(ctx, classNode, ownerClass, basePath, sizeof(basePath)) ) {
        xl6CgenError(ctx, receiver, "cannot generate class protocol base path");
        return 0;
    }
    if ( xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
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

    if ( receiver->kind == XL6_AST_MEMBER && xl6CgenClassProtocolReceiverIsAddressable(receiver) ) {
        xl6OutputPutc('&', ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(basePath, ctx->out);
        return 1;
    }

    xl6OutputPutc('&', ctx->out);
    xl6CgenNamedValueReference(ctx, receiver->text);
    xl6OutputPuts(basePath, ctx->out);
    return 1;
}

static const XL6_AstNode* xl6CgenClassProtocolParamType(const XL6_AstNode* method, size_t index)
{
    const XL6_AstNode* params;
    const XL6_AstNode* param;

    params = method != NULL && method->childCount > 1 ? method->children[1] : NULL;
    if ( params == NULL || index >= params->childCount ) {
        return NULL;
    }
    param = params->children[index];
    return param != NULL && param->childCount > 0 ? param->children[0] : NULL;
}

static void xl6CgenClassProtocolExprArg(XL6_CgenContext* ctx, const XL6_AstNode* paramType, const XL6_AstNode* expr)
{
    if ( xl6CgenIsXValueType(paramType) ) {
        xl6CgenXValueFromExprForTarget(ctx, paramType, expr);
    } else {
        xl6CgenExpr(ctx, expr);
    }
}

static const XL6_AstNode* xl6CgenClassProtocolReturns(const XL6_AstNode* method)
{
    return method != NULL && method->childCount > 2 ? method->children[2] : NULL;
}

static const char* xl6CgenClassProtocolName(const XL6_AstNode* method)
{
    const XL6_AstNode* nameNode;

    nameNode = method != NULL && method->childCount > 0 ? method->children[0] : NULL;
    return nameNode != NULL ? nameNode->text : NULL;
}

static int xl6CgenClassProtocolNameLooksReserved(const char* name)
{
    size_t len;

    if ( name == NULL ) {
        return 0;
    }
    len = strlen(name);
    return len >= 5 &&
           name[0] == '_' &&
           name[1] == '_' &&
           name[len - 2] == '_' &&
           name[len - 1] == '_';
}

static int xl6CgenClassProtocolWrapperSupported(const XL6_AstNode* method)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    params = method != NULL && method->childCount > 1 ? method->children[1] : NULL;
    returns = xl6CgenClassProtocolReturns(method);
    if ( returns != NULL && returns->childCount > 1 ) {
        return 0;
    }
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            if ( xl6CgenParamIsVararg(params->children[i]) ) {
                return 0;
            }
        }
    }
    return 1;
}

static void xl6CgenClassProtocolWrapperSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiverClass,
    const XL6_AstNode* method)
{
    const XL6_AstNode* params;
    const char* name;
    size_t i;

    name = xl6CgenClassProtocolName(method);
    xl6OutputPuts("xl_protocol_tmp_", ctx->out);
    xl6CgenFputsEncodedText(ctx->out, receiverClass != NULL ? receiverClass->text : "_");
    xl6OutputPuts("__", ctx->out);
    xl6CgenFputsEncodedText(ctx->out, name != NULL ? name : "_");

    params = method != NULL && method->childCount > 1 ? method->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("__void", ctx->out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        xl6OutputPuts("__", ctx->out);
        if ( param != NULL && param->childCount > 0 ) {
            xl6CgenMangledType(ctx, param->children[0]);
        } else {
            xl6OutputPutc('_', ctx->out);
        }
    }
}

static void xl6CgenClassProtocolWrapperProto(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiverClass,
    const XL6_AstNode* method)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    returns = xl6CgenClassProtocolReturns(method);
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPutc(' ', ctx->out);
    xl6CgenClassProtocolWrapperSymbol(ctx, receiverClass, method);
    xl6OutputPutc('(', ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, receiverClass), ctx->out);
    xl6OutputPuts(" self", ctx->out);

    params = method != NULL && method->childCount > 1 ? method->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            xl6OutputPuts(", ", ctx->out);
            if ( param == NULL || param->childCount == 0 || xl6CgenParamIsVararg(param) ) {
                xl6OutputPuts("int ", ctx->out);
            } else {
                xl6OutputPuts(xl6CgenValueStorageCType(ctx, param->children[0]), ctx->out);
                xl6OutputPutc(' ', ctx->out);
            }
            xl6OutputPrintf(ctx->out, "arg%u", (unsigned)i);
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenClassProtocolWrapper(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiverClass,
    const XL6_AstNode* ownerClass,
    const XL6_AstNode* method)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    char basePath[256];
    size_t i;

    if ( !xl6CgenClassProtocolWrapperSupported(method) ) {
        return;
    }

    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(ctx, receiverClass, ownerClass, basePath, sizeof(basePath)) ) {
        xl6CgenError(ctx, receiverClass, "cannot generate class protocol wrapper base path");
        return;
    }

    xl6CgenClassProtocolWrapperProto(ctx, receiverClass, method);
    xl6OutputPuts("\n{\n", ctx->out);
    returns = xl6CgenClassProtocolReturns(method);
    xl6OutputPuts("    ", ctx->out);
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("return ", ctx->out);
    }
    xl6CgenClassCallableSymbol(ctx, ownerClass, method);
    xl6OutputPuts("(&self", ctx->out);
    xl6OutputPuts(basePath, ctx->out);

    params = method != NULL && method->childCount > 1 ? method->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            xl6OutputPrintf(ctx->out, ", arg%u", (unsigned)i);
        }
    }
    xl6OutputPuts(");\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassProtocolWrapperMaybeEmit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiverClass,
    const XL6_AstNode* ownerClass,
    const XL6_AstNode* method,
    int protoOnly)
{
    if ( receiverClass == NULL || ownerClass == NULL || method == NULL ||
         xl6CgenClassHasTypeParams(receiverClass) ||
         !xl6CgenClassProtocolNameLooksReserved(xl6CgenClassProtocolName(method)) ||
         !xl6CgenClassProtocolWrapperSupported(method) ) {
        return;
    }

    if ( protoOnly ) {
        xl6CgenClassProtocolWrapperProto(ctx, receiverClass, method);
        xl6OutputPuts(";\n", ctx->out);
    } else {
        xl6CgenClassProtocolWrapper(ctx, receiverClass, ownerClass, method);
    }
}

static void xl6CgenClassProtocolWrappersFromOwner(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiverClass,
    const XL6_AstNode* ownerClass,
    int protoOnly)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(ownerClass);
    if ( body == NULL ) {
        return;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* resolvedOwner;
        const XL6_AstNode* resolved;
        const char* name;

        if ( member == NULL || member->kind != XL6_AST_FUNC ) {
            continue;
        }
        name = xl6CgenClassProtocolName(member);
        if ( !xl6CgenClassProtocolNameLooksReserved(name) ) {
            continue;
        }
        resolvedOwner = NULL;
        resolved = xl6CgenFindClassProtocolFunc(ctx, receiverClass, name, &resolvedOwner);
        if ( resolved == member && resolvedOwner == ownerClass ) {
            xl6CgenClassProtocolWrapperMaybeEmit(ctx, receiverClass, ownerClass, member, protoOnly);
        }
    }
}

static void xl6CgenClassProtocolWrappersForClassAndBases(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiverClass,
    const XL6_AstNode* ownerClass,
    int protoOnly)
{
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;

    if ( receiverClass == NULL || ownerClass == NULL ) {
        return;
    }
    xl6CgenClassProtocolWrappersFromOwner(ctx, receiverClass, ownerClass, protoOnly);

    base = ownerClass->childCount > 0 ? ownerClass->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return;
    }
    baseClass = xl6CgenFindClass(ctx, base->text);
    xl6CgenClassProtocolWrappersForClassAndBases(ctx, receiverClass, baseClass, protoOnly);
}

static void xl6CgenClassProtocolWrapperProtos(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( node != NULL && node->kind == XL6_AST_CLASS && !xl6CgenIsBuiltinObjectClass(node) ) {
            xl6CgenClassProtocolWrappersForClassAndBases(ctx, node, node, 1);
        }
    }
}

static void xl6CgenClassProtocolWrappers(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( node != NULL && node->kind == XL6_AST_CLASS && !xl6CgenIsBuiltinObjectClass(node) ) {
            xl6CgenClassProtocolWrappersForClassAndBases(ctx, node, node, 0);
        }
    }
}

static int xl6CgenClassProtocolCallPrefix(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiver,
    const XL6_AstNode* classNode,
    const XL6_AstNode* ownerClass,
    const XL6_AstNode* method,
    const XL6_AstNode* receiverType)
{
    const XL6_AstNode* refTarget = xl6CgenClassHandleTargetType(ctx, receiverType);

    if ( refTarget != NULL ) {
        char ownerTypeName[512];
        char basePath[256];

        if ( refTarget->childCount > 0 && xl6CgenClassHasTypeParams(ownerClass) ) {
            const XL6_AstNode* prevParams = ctx->currentGenericParams;
            const XL6_AstNode* prevArgs = ctx->currentGenericArgs;

            ctx->currentGenericParams = xl6CgenClassTypeParams(ownerClass);
            ctx->currentGenericArgs = refTarget->children[0];
            xl6CgenGenericClassCallableSymbol(ctx, ownerClass, refTarget, method);
            ctx->currentGenericParams = prevParams;
            ctx->currentGenericArgs = prevArgs;
        } else {
            xl6CgenClassCallableSymbol(ctx, ownerClass, method);
        }
        xl6CgenClassTypeRefName(ownerTypeName, sizeof(ownerTypeName), refTarget);
        xl6OutputPutc('(', ctx->out);
        basePath[0] = '\0';
        if ( !xl6CgenClassBasePath(ctx, classNode, ownerClass, basePath, sizeof(basePath)) ) {
            xl6CgenError(ctx, receiver, "cannot generate managed class protocol base path");
            xl6OutputPuts("0", ctx->out);
            return 0;
        }
        if ( basePath[0] == '\0' ) {
            xl6OutputPrintf(ctx->out, "((%s*)xl6_class_data(", ownerTypeName);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPrintf(ctx->out, ", \"\", %d, %d))", receiver->line, receiver->column);
        } else {
            xl6OutputPrintf(ctx->out, "&((%s*)xl6_class_data(", ownerTypeName);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPrintf(ctx->out, ", \"\", %d, %d))->base", receiver->line, receiver->column);
            if ( strlen(basePath) > 5 ) {
                xl6OutputPuts(basePath + 5, ctx->out);
            }
        }
        return 1;
    }

    if ( xl6CgenClassProtocolReceiverIsAddressable(receiver) ) {
        if ( receiverType != NULL && receiverType->childCount > 0 && xl6CgenClassHasTypeParams(ownerClass) ) {
            const XL6_AstNode* prevParams = ctx->currentGenericParams;
            const XL6_AstNode* prevArgs = ctx->currentGenericArgs;

            ctx->currentGenericParams = xl6CgenClassTypeParams(ownerClass);
            ctx->currentGenericArgs = receiverType->children[0];
            xl6CgenGenericClassCallableSymbol(ctx, ownerClass, receiverType, method);
            ctx->currentGenericParams = prevParams;
            ctx->currentGenericArgs = prevArgs;
        } else {
            xl6CgenClassCallableSymbol(ctx, ownerClass, method);
        }
        xl6OutputPutc('(', ctx->out);
        xl6CgenEmitClassProtocolSelf(ctx, receiver, classNode, ownerClass);
        return 1;
    }

    if ( xl6CgenClassHasTypeParams(classNode) || !xl6CgenClassProtocolWrapperSupported(method) ) {
        xl6CgenError(ctx, receiver, "复杂协议接收者暂不支持泛型 class 或可变参数协议");
        xl6OutputPuts("0", ctx->out);
        return 0;
    }

    xl6CgenClassProtocolWrapperSymbol(ctx, classNode, method);
    xl6OutputPutc('(', ctx->out);
    xl6CgenExpr(ctx, receiver);
    return 1;
}

static void xl6CgenClassProtocolTempInit(XL6_CgenContext* ctx, const char* tempName, const XL6_AstNode* paramType, const XL6_AstNode* expr)
{
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue %s = ", tempName);
    xl6CgenXValueFromExprForTarget(ctx, paramType, expr);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenClassProtocolTempCleanup(XL6_CgenContext* ctx, const char* tempName)
{
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s != 0) { xvoUnref(%s); }\n", tempName, tempName);
}

