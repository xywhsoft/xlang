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
    method = xl6CgenResolveClassFunc(ctx, classNode, call, protocolName, &ownerClass);
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
        xl6OutputPuts(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
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
                xl6OutputPuts(xl6CgenTypeName(ctx, param->children[0]), ctx->out);
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

static const char* xl6CgenUnaryProtocolName(const char* op)
{
    if ( xl6CTextEquals(op, "-") ) {
        return "__neg__";
    }
    if ( xl6CTextEquals(op, "+") ) {
        return "__pos__";
    }
    if ( xl6CTextEquals(op, "not") || xl6CTextEquals(op, "!") ) {
        return "__not__";
    }
    if ( xl6CTextEquals(op, "~") ) {
        return "__bitNot__";
    }
    return NULL;
}

static const char* xl6CgenBinaryProtocolName(const char* op)
{
    if ( xl6CTextEquals(op, "+") ) {
        return "__add__";
    }
    if ( xl6CTextEquals(op, "-") ) {
        return "__sub__";
    }
    if ( xl6CTextEquals(op, "*") ) {
        return "__mul__";
    }
    if ( xl6CTextEquals(op, "/") ) {
        return "__div__";
    }
    if ( xl6CTextEquals(op, "%") ) {
        return "__mod__";
    }
    if ( xl6CTextEquals(op, "**") ) {
        return "__pow__";
    }
    if ( xl6CTextEquals(op, "==") ) {
        return "__eq__";
    }
    if ( xl6CTextEquals(op, "!=") ) {
        return "__ne__";
    }
    if ( xl6CTextEquals(op, "<") ) {
        return "__lt__";
    }
    if ( xl6CTextEquals(op, "<=") ) {
        return "__le__";
    }
    if ( xl6CTextEquals(op, ">") ) {
        return "__gt__";
    }
    if ( xl6CTextEquals(op, ">=") ) {
        return "__ge__";
    }
    if ( xl6CTextEquals(op, "~=") ) {
        return "__approx__";
    }
    return NULL;
}

static int xl6CgenClassProtocolUnaryExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    XL6_AstNode* receiverType;
    XL6_AstNode* protocolCall;
    const char* protocolName;

    if ( node == NULL || node->kind != XL6_AST_UNARY || node->childCount == 0 ) {
        return 0;
    }
    receiver = node->children[0];
    receiverType = NULL;
    protocolName = xl6CgenUnaryProtocolName(node->text);
    protocolCall = protocolName != NULL ? xl6CgenProtocolTempCallNew(node) : NULL;
    if ( protocolCall == NULL ||
         !xl6CgenResolveClassProtocolReceiverForCall(ctx, receiver, protocolName, protocolCall, &classNode, &ownerClass, &method, &receiverType) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        return 0;
    }
    xl6CgenProtocolTempCallFree(protocolCall);

    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenClassProtocolBinaryExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* arg;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* argType;
    XL6_AstNode* receiverType;
    XL6_AstNode* protocolCall;
    const char* protocolName;

    if ( node == NULL || node->kind != XL6_AST_BINARY || node->childCount < 2 ) {
        return 0;
    }

    if ( xl6CTextEquals(node->text, "in") ) {
        receiver = node->children[1];
        arg = node->children[0];
        protocolName = "__contains__";
    } else {
        receiver = node->children[0];
        arg = node->children[1];
        protocolName = xl6CgenBinaryProtocolName(node->text);
    }
    receiverType = NULL;
    protocolCall = protocolName != NULL ? xl6CgenProtocolTempCallNew(node) : NULL;
    if ( protocolCall != NULL ) {
        xl6CgenProtocolTempCallAddArg(protocolCall, arg);
    }
    if ( protocolCall == NULL ||
         !xl6CgenResolveClassProtocolReceiverForCall(ctx, receiver, protocolName, protocolCall, &classNode, &ownerClass, &method, &receiverType) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        return 0;
    }
    xl6CgenProtocolTempCallFree(protocolCall);

    argType = xl6CgenClassProtocolParamType(method, 0);
    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenClassProtocolExprArg(ctx, argType, arg);
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenClassProtocolCallExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* args;
    const XL6_AstNode* params;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    XL6_AstNode* receiverType;
    int hasParams;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    receiver = node->children[0];
    receiverType = NULL;
    if ( !xl6CgenResolveClassProtocolReceiverForCall(ctx, receiver, "__call__", node, &classNode, &ownerClass, &method, &receiverType) ) {
        return 0;
    }

    args = node->childCount > 1 ? node->children[1] : NULL;
    params = method != NULL && method->childCount > 1 ? method->children[1] : NULL;
    hasParams = params != NULL && params->childCount > 0;
    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    if ( hasParams ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenKnownCallArgs(ctx, method, args);
    }
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenClassProtocolMemberHasLenGetter(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL ||
         !xl6CTextEquals(member->text, "length") || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    if ( !xl6CgenResolveClassProtocolReceiver(ctx, receiver, "__len__", &classNode, NULL, NULL, NULL) ) {
        return 0;
    }
    return xl6CgenFindClassFieldNode(ctx, classNode, member->text) == NULL;
}

static int xl6CgenClassProtocolLenMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    XL6_AstNode* receiverType;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL ||
         !xl6CTextEquals(member->text, "length") || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    receiverType = NULL;
    if ( !xl6CgenResolveClassProtocolReceiver(ctx, receiver, "__len__", &classNode, &ownerClass, &method, &receiverType) ) {
        return 0;
    }
    if ( xl6CgenFindClassFieldNode(ctx, classNode, member->text) != NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }

    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenClassProtocolMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    XL6_AstNode* receiverType;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    if ( xl6CgenClassProtocolLenMemberAccess(ctx, member) ) {
        return 1;
    }
    receiver = member->children[0];
    receiverType = NULL;
    if ( !xl6CgenResolveClassProtocolReceiver(ctx, receiver, "__field__", &classNode, &ownerClass, &method, &receiverType) ) {
        return 0;
    }
    if ( xl6CgenFindClassFieldNode(ctx, classNode, member->text) != NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }

    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, member->text);
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenClassProtocolMemberHasGetter(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    if ( xl6CgenClassProtocolMemberHasLenGetter(ctx, member) ) {
        return 1;
    }
    receiver = member->children[0];
    if ( !xl6CgenResolveClassProtocolReceiver(ctx, receiver, "__field__", &classNode, NULL, NULL, NULL) ) {
        return 0;
    }
    return xl6CgenFindClassFieldNode(ctx, classNode, member->text) == NULL;
}

static int xl6CgenClassProtocolMemberSetStmt(XL6_CgenContext* ctx, const XL6_AstNode* left, const XL6_AstNode* right)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* valueType;
    XL6_AstNode* receiverType;
    int valueIsXValue;
    char valueTemp[64];

    if ( left == NULL || left->kind != XL6_AST_MEMBER || left->text == NULL || left->childCount == 0 ) {
        return 0;
    }
    if ( xl6CgenClassProtocolMemberHasLenGetter(ctx, left) ) {
        return 0;
    }
    receiver = left->children[0];
    receiverType = NULL;
    if ( !xl6CgenResolveClassProtocolReceiver(ctx, receiver, "__setField__", &classNode, &ownerClass, &method, &receiverType) ) {
        return 0;
    }
    if ( !xl6CgenClassProtocolReceiverIsAddressable(receiver) ) {
        xl6AstFree(receiverType);
        xl6CgenError(ctx, left, "协议字段写入接收者必须可寻址");
        return 0;
    }
    if ( xl6CgenFindClassFieldNode(ctx, classNode, left->text) != NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }

    valueType = xl6CgenClassProtocolParamType(method, 1);
    valueIsXValue = xl6CgenIsXValueType(valueType);
    valueTemp[0] = '\0';
    if ( valueIsXValue ) {
        snprintf(valueTemp, sizeof(valueTemp), "__xl6_protocol_value_%d", ctx->tempId++);
        xl6CgenClassProtocolTempInit(ctx, valueTemp, valueType, right);
    }
    xl6CgenIndent(ctx);
    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, left->text);
    xl6OutputPuts(", ", ctx->out);
    if ( valueIsXValue ) {
        xl6OutputPuts(valueTemp, ctx->out);
    } else {
        xl6CgenClassProtocolExprArg(ctx, valueType, right);
    }
    xl6OutputPuts(");\n", ctx->out);
    if ( valueIsXValue ) {
        xl6CgenClassProtocolTempCleanup(ctx, valueTemp);
    }
    return 1;
}

static int xl6CgenClassProtocolIndexHasGetter(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* protocolCall;
    int result;

    if ( node == NULL || (node->kind != XL6_AST_INDEX && node->kind != XL6_AST_SAFE_INDEX) || node->childCount < 2 ) {
        return 0;
    }
    receiver = node->children[0];
    protocolCall = xl6CgenProtocolTempCallNew(node);
    if ( protocolCall == NULL || !xl6CgenProtocolCallAddIndexArgs(protocolCall, node) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        return 0;
    }
    result = xl6CgenResolveClassProtocolReceiverForCall(ctx, receiver, "__item__", protocolCall, NULL, NULL, NULL, NULL);
    xl6CgenProtocolTempCallFree(protocolCall);
    return result;
}

static int xl6CgenClassProtocolIndexExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    XL6_AstNode* receiverType;
    XL6_AstNode* protocolCall;
    size_t i;

    if ( node == NULL || (node->kind != XL6_AST_INDEX && node->kind != XL6_AST_SAFE_INDEX) || node->childCount < 2 ) {
        return 0;
    }
    receiver = node->children[0];
    receiverType = NULL;
    protocolCall = xl6CgenProtocolTempCallNew(node);
    if ( protocolCall == NULL || !xl6CgenProtocolCallAddIndexArgs(protocolCall, node) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        return 0;
    }
    if ( !xl6CgenResolveClassProtocolReceiverForCall(ctx, receiver, "__item__", protocolCall, &classNode, &ownerClass, &method, &receiverType) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        return 0;
    }
    xl6CgenProtocolTempCallFree(protocolCall);

    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    for ( i = 0; i < xl6CgenIndexArgCount(node); ++i ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenClassProtocolExprArg(ctx, xl6CgenClassProtocolParamType(method, i), xl6CgenIndexArgAt(node, i));
    }
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenClassProtocolIndexSetStmt(XL6_CgenContext* ctx, const XL6_AstNode* left, const XL6_AstNode* right)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* index;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* keyType;
    const XL6_AstNode* valueType;
    XL6_AstNode* receiverType;
    XL6_AstNode* protocolCall;
    int keyIsXValue;
    int valueIsXValue;
    char keyTemp[64];
    char valueTemp[64];
    size_t argCount;
    size_t i;

    if ( left == NULL || left->kind != XL6_AST_INDEX || left->childCount < 2 ) {
        return 0;
    }
    receiver = left->children[0];
    index = xl6CgenIndexArgAt(left, 0);
    argCount = xl6CgenIndexArgCount(left);
    receiverType = NULL;
    protocolCall = xl6CgenProtocolTempCallNew(left);
    if ( protocolCall == NULL ||
         !xl6CgenProtocolCallAddIndexArgs(protocolCall, left) ||
         !xl6CgenProtocolTempCallAddArg(protocolCall, right) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        return 0;
    }
    if ( !xl6CgenResolveClassProtocolReceiverForCall(ctx, receiver, "__setItem__", protocolCall, &classNode, &ownerClass, &method, &receiverType) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        return 0;
    }
    xl6CgenProtocolTempCallFree(protocolCall);
    if ( !xl6CgenClassProtocolReceiverIsAddressable(receiver) ) {
        xl6AstFree(receiverType);
        xl6CgenError(ctx, left, "协议索引写入接收者必须可寻址");
        return 0;
    }

    keyType = xl6CgenClassProtocolParamType(method, 0);
    valueType = xl6CgenClassProtocolParamType(method, argCount);
    keyIsXValue = xl6CgenIsXValueType(keyType);
    valueIsXValue = xl6CgenIsXValueType(valueType);
    keyTemp[0] = '\0';
    valueTemp[0] = '\0';
    if ( keyIsXValue ) {
        snprintf(keyTemp, sizeof(keyTemp), "__xl6_protocol_key_%d", ctx->tempId++);
        xl6CgenClassProtocolTempInit(ctx, keyTemp, keyType, index);
    }
    if ( valueIsXValue ) {
        snprintf(valueTemp, sizeof(valueTemp), "__xl6_protocol_value_%d", ctx->tempId++);
        xl6CgenClassProtocolTempInit(ctx, valueTemp, valueType, right);
    }
    xl6CgenIndent(ctx);
    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    xl6OutputPuts(", ", ctx->out);
    if ( keyIsXValue ) {
        xl6OutputPuts(keyTemp, ctx->out);
    } else {
        xl6CgenClassProtocolExprArg(ctx, keyType, index);
    }
    for ( i = 1; i < argCount; ++i ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenClassProtocolExprArg(ctx, xl6CgenClassProtocolParamType(method, i), xl6CgenIndexArgAt(left, i));
    }
    xl6OutputPuts(", ", ctx->out);
    if ( valueIsXValue ) {
        xl6OutputPuts(valueTemp, ctx->out);
    } else {
        xl6CgenClassProtocolExprArg(ctx, valueType, right);
    }
    xl6OutputPuts(");\n", ctx->out);
    if ( valueIsXValue ) {
        xl6CgenClassProtocolTempCleanup(ctx, valueTemp);
    }
    if ( keyIsXValue ) {
        xl6CgenClassProtocolTempCleanup(ctx, keyTemp);
    }
    return 1;
}

