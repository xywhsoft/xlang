static void xl6CgenTypedContainerValueStmt(XL6_CgenContext* ctx, const char* cName, const char* callPrefix, const XL6_AstNode* valueExpr)
{
    int id;

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_typed_value_%d = ", id);
    xl6CgenXValueFromExpr(ctx, valueExpr);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (!%s%s, __xl6_typed_value_%d)) {\n", callPrefix, cName, id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("xl6_runtime_fatal(3);\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_typed_value_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static int xl6CgenTypedFutureExprIsOwnedDirect(const XL6_AstNode* valueExpr)
{
    return valueExpr != NULL && valueExpr->kind == XL6_AST_CALL;
}

static int xl6CgenTypedFutureExprNeedsRelease(const XL6_AstNode* valueExpr)
{
    return xl6CgenTypedFutureExprIsOwnedDirect(valueExpr) ||
           (valueExpr != NULL && valueExpr->kind == XL6_AST_CONDITIONAL && valueExpr->childCount > 2);
}

static void xl6CgenTypedFutureOwnedExpr(XL6_CgenContext* ctx, const XL6_AstNode* valueExpr)
{
    if ( valueExpr != NULL && valueExpr->kind == XL6_AST_CONDITIONAL && valueExpr->childCount > 2 ) {
        xl6OutputPuts("(", ctx->out);
        xl6CgenExpr(ctx, valueExpr->children[0]);
        xl6OutputPuts(" ? ", ctx->out);
        xl6CgenTypedFutureOwnedExpr(ctx, valueExpr->children[1]);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenTypedFutureOwnedExpr(ctx, valueExpr->children[2]);
        xl6OutputPuts(")", ctx->out);
        return;
    }
    if ( xl6CgenTypedFutureExprIsOwnedDirect(valueExpr) ) {
        xl6CgenExpr(ctx, valueExpr);
        return;
    }
    xl6OutputPuts("xl6_future_add_ref_if_needed(", ctx->out);
    xl6CgenExpr(ctx, valueExpr);
    xl6OutputPuts(")", ctx->out);
}

static void xl6CgenTypedArrayFutureStmt(XL6_CgenContext* ctx, const char* cName, const XL6_AstNode* valueExpr)
{
    int id;
    int shouldRelease;

    id = ctx->tempId++;
    shouldRelease = xl6CgenTypedFutureExprNeedsRelease(valueExpr);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xfuture* __xl6_typed_future_%d = ", id);
    if ( valueExpr != NULL && valueExpr->kind == XL6_AST_CONDITIONAL && valueExpr->childCount > 2 ) {
        xl6CgenTypedFutureOwnedExpr(ctx, valueExpr);
    } else {
        xl6CgenExpr(ctx, valueExpr);
    }
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (!xrtTypedArrayAppend(%s, &__xl6_typed_future_%d)) {\n", cName, id);
    ctx->indent += 1;
    if ( shouldRelease ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xFutureRelease(__xl6_typed_future_%d);\n", id);
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts("xl6_runtime_fatal(3);\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    if ( shouldRelease ) {
        xl6OutputPuts("} else {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xFutureRelease(__xl6_typed_future_%d);\n", id);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    } else {
        xl6OutputPuts("}\n", ctx->out);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenTypedContainerFillFromLiteral(XL6_CgenContext* ctx, const char* cName, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr)
{
    const XL6_AstNode* literal;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;
    size_t i;

    if ( initExpr == NULL ) {
        return;
    }

    literal = xl6CgenTypedContainerInitLiteral(ctx, typeNode, initExpr);
    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( literal == NULL ) {
        xl6CgenError(ctx, initExpr, "静态泛型容器目前只支持同类容器字面量初始化");
        return;
    }

    if ( xl6CTextEquals(typeName, "array") ) {
        for ( i = 0; i < literal->childCount; ++i ) {
            if ( xl6CgenIsFutureLikeType(itemType) ) {
                xl6CgenTypedArrayFutureStmt(ctx, cName, literal->children[i]);
            } else {
                xl6CgenTypedContainerValueStmt(ctx, cName, "xrtTypedArrayAppendValue(", literal->children[i]);
            }
        }
        return;
    }

    if ( xl6CTextEquals(typeName, "set") ) {
        for ( i = 0; i < literal->childCount; ++i ) {
            xl6CgenTypedContainerValueStmt(ctx, cName, "xrtTypedSetAddValue(", literal->children[i]);
        }
        return;
    }

    if ( xl6CTextEquals(typeName, "list") ) {
        for ( i = 0; i < literal->childCount; ++i ) {
            const XL6_AstNode* pair = literal->children[i];
            const XL6_AstNode* key = pair != NULL && pair->childCount > 0 ? pair->children[0] : NULL;
            const XL6_AstNode* value = pair != NULL && pair->childCount > 1 ? pair->children[1] : NULL;
            int id = ctx->tempId++;

            xl6CgenIndent(ctx);
            xl6OutputPuts("{\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvalue __xl6_typed_value_%d = ", id);
            xl6CgenXValueFromExpr(ctx, value);
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (!xrtTypedListSetValue(%s, (long long)(", cName);
            xl6CgenExpr(ctx, key);
            xl6OutputPrintf(ctx->out, "), __xl6_typed_value_%d)) {\n", id);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("xl6_runtime_fatal(3);\n", ctx->out);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_typed_value_%d);\n", id);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        }
        return;
    }

    if ( xl6CTextEquals(typeName, "dict") ) {
        for ( i = 0; i < literal->childCount; ++i ) {
            const XL6_AstNode* pair = literal->children[i];
            const XL6_AstNode* key = pair != NULL && pair->childCount > 0 ? pair->children[0] : NULL;
            const XL6_AstNode* value = pair != NULL && pair->childCount > 1 ? pair->children[1] : NULL;
            int id = ctx->tempId++;

            if ( !xl6CgenExprStaticTypeIs(ctx, key, "string") ) {
                xl6CgenError(ctx, key, "静态 dict 字面量键必须是 string");
                continue;
            }

            xl6CgenIndent(ctx);
            xl6OutputPuts("{\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvalue __xl6_typed_value_%d = ", id);
            xl6CgenXValueFromExpr(ctx, value);
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (!xrtTypedDictSetValue(%s, ", cName);
            xl6CgenExpr(ctx, key);
            xl6OutputPrintf(ctx->out, ", 0, __xl6_typed_value_%d)) {\n", id);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("xl6_runtime_fatal(3);\n", ctx->out);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_typed_value_%d);\n", id);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        }
    }
}

static int xl6CgenFuncSingleReturnUsesTypedContainerStorage(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* returns;

    if ( ctx == NULL || funcNode == NULL || funcNode->kind != XL6_AST_FUNC ) {
        return 0;
    }
    if ( xl6CgenFuncIsGeneric(funcNode) ) {
        return 0;
    }

    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    return returns != NULL &&
           returns->childCount == 1 &&
           xl6CgenIsTypedContainerType(ctx, returns->children[0]);
}

static const char* xl6CgenFuncSingleReturnCType(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* returns;

    if ( xl6CgenFuncSingleReturnUsesTypedContainerStorage(ctx, funcNode) ) {
        returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
        return xl6CgenTypedContainerCType(ctx, returns->children[0]);
    }

    returns = funcNode != NULL && funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    if ( returns != NULL && returns->childCount == 1 ) {
        return xl6CgenTypeName(ctx, returns->children[0]);
    }
    return "void";
}

static int xl6CgenParamTypeUsesTypedContainerStorage(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return ctx != NULL &&
           typeNode != NULL &&
           ctx->currentGenericParams == NULL &&
           ctx->currentGenericArgs == NULL &&
           ctx->currentClass == NULL &&
           ctx->currentNativeClass == NULL &&
           ctx->currentAnonFunc == NULL &&
           xl6CgenIsTypedContainerType(ctx, typeNode);
}

static const char* xl6CgenParamCType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    if ( xl6CgenParamTypeUsesTypedContainerStorage(ctx, typeNode) ) {
        return xl6CgenTypedContainerCType(ctx, typeNode);
    }
    return xl6CgenTypeName(ctx, typeNode);
}

static void xl6CgenTypedContainerValueFromExprMode(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr, const char* mode)
{
    const XL6_AstNode* literal;

    literal = xl6CgenTypedContainerInitLiteral(ctx, typeNode, expr);
    if ( literal != NULL ) {
        xl6CgenTypedContainerCreateExprMode(ctx, typeNode, mode);
        return;
    }

    if ( xl6CgenExprUsesTypedContainerStorage(ctx, expr) ) {
        xl6CgenTypedContainerCopyExprMode(ctx, typeNode, expr, mode);
        return;
    }

    xl6CgenExpr(ctx, expr);
}

static void xl6CgenTypedContainerValueFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr)
{
    xl6CgenTypedContainerValueFromExprMode(ctx, typeNode, expr, "XRT_OBJMODE_LOCAL");
}

static void xl6CgenTypedContainerReturnStmt(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr, int id)
{
    char returnName[64];

    snprintf(returnName, sizeof(returnName), "__xl6_return_%d", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, typeNode), returnName);
    xl6CgenTypedContainerValueFromExpr(ctx, typeNode, expr);
    xl6OutputPuts(";\n", ctx->out);
    if ( xl6CgenTypedContainerInitLiteral(ctx, typeNode, expr) != NULL ) {
        xl6CgenTypedContainerFillFromLiteral(ctx, returnName, typeNode, expr);
    }
}
static int xl6CgenIsCurrentNativeClassSelfReceiver(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    const XL6_AstNode* selfType;

    if ( ctx == NULL ||
         receiver == NULL ||
         receiver->kind != XL6_AST_NAME ||
         !xl6CTextEquals(receiver->text, "self") ) {
        return 0;
    }
    if ( ctx->currentNativeClass != NULL ) {
        return 1;
    }

    selfType = xl6CgenFindVarType(ctx, "self");
    return selfType != NULL && xl6CgenFindNativeBackedClass(ctx, selfType->text) != NULL;
}
static void xl6CgenNamedValueReference(XL6_CgenContext* ctx, const char* name)
{
    const XL6_AstNode* externalVar;

    if ( ctx == NULL || name == NULL ) {
        return;
    }
    if ( xl6CgenFindTrackedVarType(ctx, name) == NULL ) {
        externalVar = xl6CgenFindExternalVar(ctx, name);
        if ( externalVar != NULL ) {
            xl6CgenCStringSymbol(ctx->out, externalVar->text);
            return;
        }
    }
    xl6CgenFputsCIdent(ctx->out, name);
}

static void xl6CgenNativeReceiverHandleExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    if ( xl6CgenIsCurrentNativeClassSelfReceiver(ctx, receiver) ) {
        xl6OutputPuts("self->handle", ctx->out);
        return;
    }
    if ( receiver != NULL && receiver->kind == XL6_AST_NAME ) {
        xl6CgenNamedValueReference(ctx, receiver->text);
        xl6OutputPuts(".handle", ctx->out);
        return;
    }

    xl6CgenExpr(ctx, receiver);
    xl6OutputPuts(".handle", ctx->out);
}

static void xl6CgenNativeReceiverPtrExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    if ( xl6CgenIsCurrentNativeClassSelfReceiver(ctx, receiver) ) {
        xl6OutputPuts("self", ctx->out);
        return;
    }

    xl6OutputPutc('&', ctx->out);
    if ( receiver != NULL && receiver->kind == XL6_AST_NAME ) {
        xl6CgenNamedValueReference(ctx, receiver->text);
    } else {
        xl6CgenExpr(ctx, receiver);
    }
}

static int xl6CgenNativeClassMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* customNode;
    const XL6_AstNode* helperMethod;
    const XL6_AstNode* externMethod;
    const XL6_AstNode* method;
    const XL6_AstNode* params;
    const XL6_AstNode* args;
    const char* receiverName;

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || (receiver->kind != XL6_AST_NAME && receiver->kind != XL6_AST_MEMBER) ) {
        return 0;
    }

    receiverName = receiver->kind == XL6_AST_NAME ? receiver->text : NULL;
    receiverType = receiver->kind == XL6_AST_NAME ? xl6CgenFindVarType(ctx, receiverName) : xl6CgenMemberFieldType(ctx, receiver);
    customNode = receiverType != NULL ? xl6CgenFindNativeBackedClass(ctx, receiverType->text) : NULL;
    if ( customNode == NULL ) {
        return 0;
    }

    helperMethod = xl6CgenResolveNativeClassFunc(ctx, customNode, call, member->text, 0);
    externMethod = xl6CgenResolveNativeExternFuncWithReport(ctx, customNode, call, member->text, 0);
    if ( helperMethod != NULL && externMethod != NULL ) {
        xl6CgenError(ctx, member, "native-backed class method call is ambiguous between helper and extern");
        xl6OutputPuts("0", ctx->out);
        return 1;
    }

    method = helperMethod != NULL ? helperMethod : externMethod;
    if ( method == NULL ) {
        xl6CgenError(ctx, member, "native extern method not found");
        xl6OutputPuts("0", ctx->out);
        return 1;
    }

    if ( helperMethod != NULL ) {
        xl6CgenClassCallableSymbol(ctx, customNode, method);
    } else {
        xl6CgenCStringSymbol(ctx->out, method->text);
    }
    xl6OutputPutc('(', ctx->out);
    if ( helperMethod != NULL ) {
        xl6CgenNativeReceiverPtrExpr(ctx, receiver);
    } else {
        xl6CgenNativeReceiverHandleExpr(ctx, receiver);
    }
    args = call->childCount > 1 ? call->children[1] : NULL;
    params = xl6CgenCallableParams(method);
    if ( params != NULL && params->childCount > 0 ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenKnownCallArgs(ctx, method, args);
    }
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static void xl6CgenImplicitToStringCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;

    (void)call;
    receiver = member != NULL && member->childCount > 0 ? member->children[0] : NULL;
    xl6OutputPuts("xl6_xrt_value_to_string_release(", ctx->out);
    xl6CgenXValueFromExpr(ctx, receiver);
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenClassMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    XL6_AstNode* ownedReceiverType;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* args;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    char basePath[256];
    int fixedCount;

    receiverType = NULL;
    ownedReceiverType = NULL;
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || (receiver->kind != XL6_AST_NAME && receiver->kind != XL6_AST_MEMBER) ) {
        return 0;
    }

    if ( receiver->kind == XL6_AST_NAME && xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        classNode = ctx->currentClass;
    } else if ( receiver->kind == XL6_AST_MEMBER ) {
        ownedReceiverType = xl6CgenCloneMemberFieldTypeForInference(ctx, receiver);
        receiverType = ownedReceiverType;
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    } else {
        receiverType = xl6CgenFindVarType(ctx, receiver->text);
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    }
    if ( classNode == NULL ) {
        xl6AstFree(ownedReceiverType);
        return 0;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( receiverType != NULL && receiverType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = receiverType->children[0];
    }

    ownerClass = NULL;
    method = xl6CgenResolveClassFunc(ctx, classNode, call, member->text, &ownerClass);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    if ( method == NULL ) {
        xl6CgenError(ctx, member, "找不到 class 方法");
        xl6OutputPuts("0", ctx->out);
        return 1;
    }
    if ( ownerClass == NULL ) {
        ownerClass = classNode;
    }

    if ( xl6CgenVirtualClassMethodCall(ctx, call, member, receiver, classNode, ownerClass, method) ) {
        return 1;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( receiverType != NULL && receiverType->childCount > 0 && xl6CgenClassHasTypeParams(ownerClass) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(ownerClass);
        ctx->currentGenericArgs = receiverType->children[0];
        xl6CgenGenericClassCallableSymbol(ctx, ownerClass, receiverType, method);
    } else {
        xl6CgenClassCallableSymbol(ctx, ownerClass, method);
    }
    xl6OutputPutc('(', ctx->out);
    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(ctx, classNode, ownerClass, basePath, sizeof(basePath)) ) {
        xl6CgenError(ctx, member, "无法生成继承方法调用的 base 路径");
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
    } else {
        xl6OutputPutc('&', ctx->out);
        if ( receiver->kind == XL6_AST_NAME ) {
            xl6CgenNamedValueReference(ctx, receiver->text);
        } else {
            xl6CgenExpr(ctx, receiver);
        }
        xl6OutputPuts(basePath, ctx->out);
    }
    args = call->childCount > 1 ? call->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(method->childCount > 1 ? method->children[1] : NULL);
    if ( fixedCount > 0 ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenKnownCallArgs(ctx, method, args);
    }
    xl6OutputPutc(')', ctx->out);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    xl6AstFree(ownedReceiverType);
    return 1;
}

static XL6_AstNode* xl6CgenBaseClass(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* base;

    if ( classNode == NULL ) {
        return NULL;
    }
    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return NULL;
    }
    return xl6CgenFindClass(ctx, base->text);
}

static void xl6CgenEmitClassCreateCall(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* createNode, const char* selfExpr, const XL6_AstNode* args)
{
    const XL6_AstNode* params;
    int fixedCount;

    xl6CgenClassCallableSymbol(ctx, classNode, createNode);
    xl6OutputPutc('(', ctx->out);
    xl6OutputPuts(selfExpr != NULL ? selfExpr : "self", ctx->out);
    params = xl6CgenCallableParams(createNode);
    fixedCount = xl6CgenFixedParamCount(params);
    if ( fixedCount > 0 ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenParamNodeArgs(ctx, params, args);
    }
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenNativeClassCtorHasPointBindingArgs(const XL6_AstNode* args)
{
    const XL6_AstNode* arg;

    if ( args == NULL || args->childCount != 1 ) {
        return 0;
    }
    arg = args->children[0];
    return arg != NULL && arg->kind != XL6_AST_NAMED_ARG && arg->childCount > 0;
}

static int xl6CgenEmitNativeClassHandleBinding(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* varPath, const XL6_AstNode* args)
{
    const XL6_AstNode* arg;

    if ( !xl6CgenClassHasNativeHandle(classNode) || !xl6CgenNativeClassCtorHasPointBindingArgs(args) ) {
        return 0;
    }

    arg = args->children[0];
    xl6CgenIndent(ctx);
    xl6OutputPuts(varPath != NULL ? varPath : "self", ctx->out);
    xl6OutputPuts(".handle = ", ctx->out);
    xl6CgenExpr(ctx, arg->children[0]);
    xl6OutputPuts(";\n", ctx->out);
    return 1;
}

static int xl6CgenEmitClassCtorInto(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* targetExpr)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    XL6_AstNode* classNode;
    const XL6_AstNode* createNode;
    char selfExpr[256];

    if ( ctx == NULL || call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 || targetExpr == NULL ) {
        return 0;
    }

    callee = call->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }

    classNode = xl6CgenFindClass(ctx, callee->text);
    if ( classNode == NULL ) {
        return 0;
    }
    if ( xl6CgenClassHasTypeParams(classNode) ) {
        return 0;
    }

    args = call->childCount > 1 ? call->children[1] : NULL;
    createNode = xl6CgenResolveClassCreate(ctx, classNode, call);

    xl6CgenIndent(ctx);
    xl6OutputPuts(targetExpr, ctx->out);
    xl6OutputPuts(" = (", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, classNode), ctx->out);
    xl6OutputPuts("){0};\n", ctx->out);

    xl6CgenClassDefaultInitFields(ctx, classNode, targetExpr);
    if ( createNode != NULL ) {
        snprintf(selfExpr, sizeof(selfExpr), "&%s", targetExpr);
        xl6CgenIndent(ctx);
        xl6CgenEmitClassCreateCall(ctx, classNode, createNode, selfExpr, args);
        xl6OutputPuts(";\n", ctx->out);
    } else {
        if ( args != NULL && args->childCount > 0 &&
             !xl6CgenEmitNativeClassHandleBinding(ctx, classNode, targetExpr, args) ) {
            xl6CgenError(ctx, call, "class constructor has arguments but no matching create method");
        }
        xl6CgenEmitAutoBaseCreateForValue(ctx, classNode, targetExpr, call);
    }
    return 1;
}

static int xl6CgenClassConstructorExpr(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* callee;
    XL6_AstNode* classNode;

    if ( ctx == NULL || call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return 0;
    }

    callee = call->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }

    classNode = xl6CgenFindClass(ctx, callee->text);
    if ( classNode == NULL ) {
        return 0;
    }
    xl6CgenError(ctx, call, "class constructor expression requires statement-level lowering");
    xl6OutputPuts("0", ctx->out);
    return 1;
}

static int xl6CgenBaseCtorCall(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    XL6_AstNode* baseClass;
    const XL6_AstNode* createNode;

    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return 0;
    }
    callee = call->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME || !xl6CTextEquals(callee->text, "base") ) {
        return 0;
    }

    args = call->childCount > 1 ? call->children[1] : NULL;
    if ( ctx->currentClass == NULL ) {
        xl6CgenError(ctx, call, "base constructor call requires current class context");
        xl6OutputPuts("0", ctx->out);
        return 1;
    }

    baseClass = xl6CgenBaseClass(ctx, ctx->currentClass);
    if ( baseClass == NULL ) {
        xl6CgenError(ctx, call, "当前 class 没有父类，不能调用 base(...)");
        xl6OutputPuts("0", ctx->out);
        return 1;
    }

    createNode = xl6CgenResolveClassCreate(ctx, baseClass, call);
    if ( createNode == NULL ) {
        if ( args != NULL && args->childCount > 0 ) {
            xl6CgenError(ctx, call, "父类没有 create，base(...) 不能传参");
        }
        xl6OutputPuts("((void)0)", ctx->out);
        return 1;
    }

    xl6CgenEmitClassCreateCall(ctx, baseClass, createNode, "&self->base", args);
    return 1;
}

static int xl6CgenClassHasDirectField(const XL6_AstNode* classNode, const char* fieldName)
{
    return xl6CgenFindDirectClassFieldNode(classNode, fieldName) != NULL;
}
static int xl6CgenEmitClassFieldSuffixWithSep(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* fieldName, const char* sep)
{
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;

    if ( classNode == NULL || fieldName == NULL ) {
        return 0;
    }
    if ( xl6CgenClassHasDirectField(classNode, fieldName) ) {
        xl6OutputPuts(sep != NULL ? sep : ".", ctx->out);
        xl6CgenFputsCIdent(ctx->out, fieldName);
        return 1;
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return 0;
    }
    baseClass = xl6CgenFindClass(ctx, base->text);
    if ( baseClass == NULL ) {
        return 0;
    }

    xl6OutputPuts(sep != NULL ? sep : ".", ctx->out);
    xl6OutputPuts("base", ctx->out);
    return xl6CgenEmitClassFieldSuffixWithSep(ctx, baseClass, fieldName, ".");
}

static int xl6CgenEmitClassFieldSuffix(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* fieldName)
{
    return xl6CgenEmitClassFieldSuffixWithSep(ctx, classNode, fieldName, ".");
}

static const XL6_AstNode* xl6CgenMemberFieldType(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* field;
    XL6_AstNode* classNode;
    XL6_AstNode* ownedReceiverType;

    if ( member == NULL || member->kind != XL6_AST_MEMBER ) {
        return NULL;
    }

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || (receiver->kind != XL6_AST_NAME &&
                              receiver->kind != XL6_AST_MEMBER &&
                              receiver->kind != XL6_AST_INDEX) ) {
        return NULL;
    }

    receiverType = NULL;
    ownedReceiverType = NULL;
    classNode = NULL;
    if ( receiver->kind == XL6_AST_MEMBER || receiver->kind == XL6_AST_INDEX ) {
        ownedReceiverType = receiver->kind == XL6_AST_MEMBER
            ? xl6CgenCloneMemberFieldTypeForInference(ctx, receiver)
            : xl6CgenCloneExprTypeForInference(ctx, receiver);
        receiverType = ownedReceiverType;
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    } else if ( xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        classNode = (XL6_AstNode*)ctx->currentClass;
    } else {
        receiverType = xl6CgenFindVarType(ctx, receiver->text);
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    }

    if ( receiverType != NULL && xl6CgenIsUnionType(ctx, receiverType) ) {
        field = xl6CgenFindUnionFieldNode(xl6CgenFindUnion(ctx, receiverType->text), member->text);
        xl6AstFree(ownedReceiverType);
        return field != NULL && field->childCount > 0 ? field->children[0] : NULL;
    }
    if ( classNode == NULL ) {
        xl6AstFree(ownedReceiverType);
        return NULL;
    }

    field = xl6CgenFindClassFieldNode(ctx, classNode, member->text);
    xl6AstFree(ownedReceiverType);
    return field != NULL && field->childCount > 0 ? field->children[0] : NULL;
}

static int xl6CgenUnionFieldAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* unionNode;
    const XL6_AstNode* field;
    XL6_AstNode* ownedReceiverType;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    if ( receiver == NULL || (receiver->kind != XL6_AST_NAME &&
                              receiver->kind != XL6_AST_MEMBER &&
                              receiver->kind != XL6_AST_INDEX) ) {
        return 0;
    }

    ownedReceiverType = NULL;
    if ( receiver->kind == XL6_AST_MEMBER || receiver->kind == XL6_AST_INDEX ) {
        ownedReceiverType = receiver->kind == XL6_AST_MEMBER
            ? xl6CgenCloneMemberFieldTypeForInference(ctx, receiver)
            : xl6CgenCloneExprTypeForInference(ctx, receiver);
        receiverType = ownedReceiverType;
    } else {
        receiverType = xl6CgenFindVarType(ctx, receiver->text);
    }
    if ( !xl6CgenIsUnionType(ctx, receiverType) ) {
        xl6AstFree(ownedReceiverType);
        return 0;
    }

    unionNode = xl6CgenFindUnion(ctx, receiverType->text);
    field = xl6CgenFindUnionFieldNode(unionNode, member->text);
    if ( field == NULL ) {
        xl6AstFree(ownedReceiverType);
        return 0;
    }

    if ( receiver->kind == XL6_AST_NAME ) {
        xl6CgenNamedValueReference(ctx, receiver->text);
    } else {
        xl6CgenExpr(ctx, receiver);
    }
    xl6OutputPutc('.', ctx->out);
    xl6CgenFputsCIdent(ctx->out, member->text);
    xl6AstFree(ownedReceiverType);
    return 1;
}
static void xl6CgenClassFieldAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    XL6_AstNode* classNode;
    XL6_AstNode* ownedReceiverType;

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || (receiver->kind != XL6_AST_NAME &&
                              receiver->kind != XL6_AST_MEMBER &&
                              receiver->kind != XL6_AST_INDEX &&
                              receiver->kind != XL6_AST_CALL) ) {
        xl6CgenError(ctx, member, "class field receiver must be name, member, index or call");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    if ( receiver->kind == XL6_AST_NAME && xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        if ( xl6CgenFindClassFieldNode(ctx, ctx->currentClass, member->text) == NULL &&
             xl6CgenClassPropertyMemberAccess(ctx, member) ) {
            return;
        }
        if ( xl6CgenFindClassFieldNode(ctx, ctx->currentClass, member->text) == NULL &&
             xl6CgenClassProtocolMemberAccess(ctx, member) ) {
            return;
        }
        xl6OutputPuts("self", ctx->out);
        if ( !xl6CgenEmitClassFieldSuffixWithSep(ctx, ctx->currentClass, member->text, "->") ) {
            xl6CgenError(ctx, member, "class field not found");
        }
        return;
    }

    ownedReceiverType = NULL;
    if ( receiver->kind == XL6_AST_MEMBER || receiver->kind == XL6_AST_INDEX || receiver->kind == XL6_AST_CALL ) {
        ownedReceiverType = receiver->kind == XL6_AST_MEMBER
            ? xl6CgenCloneMemberFieldTypeForInference(ctx, receiver)
            : xl6CgenCloneExprTypeForInference(ctx, receiver);
        receiverType = ownedReceiverType;
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
        if ( classNode == NULL ) {
            xl6AstFree(ownedReceiverType);
            xl6CgenError(ctx, member, "class field receiver is not a class value");
            xl6OutputPuts("0", ctx->out);
            return;
        }

        if ( xl6CgenFindClassFieldNode(ctx, classNode, member->text) == NULL ) {
            if ( xl6CgenClassPropertyMemberAccess(ctx, member) ||
                 xl6CgenClassProtocolMemberAccess(ctx, member) ) {
                xl6AstFree(ownedReceiverType);
                return;
            }
        }
        xl6CgenExpr(ctx, receiver);
        if ( !xl6CgenEmitClassFieldSuffix(ctx, classNode, member->text) ) {
            xl6CgenError(ctx, member, "class field not found");
        }
        xl6AstFree(ownedReceiverType);
        return;
    }

    receiverType = xl6CgenFindVarType(ctx, receiver->text);
    classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    if ( classNode == NULL ) {
        xl6CgenError(ctx, member, "class field receiver is not a class value");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    if ( xl6CgenFindClassFieldNode(ctx, classNode, member->text) == NULL ) {
        if ( xl6CgenClassPropertyMemberAccess(ctx, member) ||
             xl6CgenClassProtocolMemberAccess(ctx, member) ) {
            return;
        }
    }
    xl6CgenNamedValueReference(ctx, receiver->text);
    if ( !xl6CgenEmitClassFieldSuffix(ctx, classNode, member->text) ) {
        xl6CgenError(ctx, member, "class field not found");
    }
}
static void xl6CgenXValueMemberKeyExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const char* key)
{
    if ( xl6CgenExprIsXValueBorrow(ctx, receiver) ) {
        xl6OutputPuts("xl6_xrt_index_safe_get_key(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenCStringLiteral(ctx->out, key);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    xl6OutputPuts("xl6_xrt_index_safe_get_key_retain_release(", ctx->out);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, key);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenXValueMemberLengthExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* receiverType)
{
    if ( xl6CgenExprUsesTypedContainerStorage(ctx, receiver) && xl6CgenIsTypedContainerType(ctx, receiverType) ) {
        const XL6_AstNode* concreteType = xl6CgenTypedContainerConcreteType(ctx, receiverType);
        const char* typeName = concreteType != NULL ? concreteType->text : NULL;

        if ( xl6CTextEquals(typeName, "array") ) {
            xl6OutputPuts("xrtTypedArrayCount(", ctx->out);
        } else if ( xl6CTextEquals(typeName, "list") ) {
            xl6OutputPuts("xrtTypedListCount(", ctx->out);
        } else if ( xl6CTextEquals(typeName, "set") ) {
            xl6OutputPuts("xrtTypedSetCount(", ctx->out);
        } else if ( xl6CTextEquals(typeName, "dict") ) {
            xl6OutputPuts("xrtTypedDictCount(", ctx->out);
        } else {
            xl6CgenError(ctx, receiver, "typed container length requires array/list/set/dict");
            xl6OutputPuts("0", ctx->out);
            return;
        }
        xl6CgenExpr(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    if ( xl6CgenExprIsXValueBorrow(ctx, receiver) ) {
        if ( receiver != NULL && receiver->kind == XL6_AST_NAME && xl6CgenIsArrayXValueType(receiverType) ) {
            xl6OutputPuts("xvoArrayItemCount(", ctx->out);
            xl6CgenNamedValueReference(ctx, receiver->text);
            xl6OutputPutc(')', ctx->out);
        } else if ( receiver != NULL && receiver->kind == XL6_AST_NAME && xl6CgenIsListXValueType(receiverType) ) {
            xl6OutputPuts("xvoListItemCount(", ctx->out);
            xl6CgenNamedValueReference(ctx, receiver->text);
            xl6OutputPutc(')', ctx->out);
        } else if ( receiver != NULL && receiver->kind == XL6_AST_NAME && xl6CgenIsSetXValueType(receiverType) ) {
            xl6OutputPuts("xvoSetItemCount(", ctx->out);
            xl6CgenNamedValueReference(ctx, receiver->text);
            xl6OutputPutc(')', ctx->out);
        } else if ( receiver != NULL && receiver->kind == XL6_AST_NAME && xl6CgenIsDictXValueType(receiverType) ) {
            xl6OutputPuts("xvoTableItemCount(", ctx->out);
            xl6CgenNamedValueReference(ctx, receiver->text);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6OutputPuts("xl6_xrt_member_length(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
        }
        return;
    }

    xl6OutputPuts("xl6_xrt_member_length_release(", ctx->out);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenXValueMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;

    if ( member == NULL || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( !xl6CgenIsXValueType(receiverType) ) {
        xl6AstFree(receiverType);
        return 0;
    }

    if ( xl6CTextEquals(member->text, "length") ) {
        xl6CgenXValueMemberLengthExpr(ctx, receiver, receiverType);
    } else if ( xl6CgenMemberKeyReceiverIsDynamic(ctx, receiverType) ) {
        xl6CgenXValueMemberKeyExpr(ctx, receiver, member->text);
    } else {
        xl6CgenError(ctx, member, "member key receiver must be var or dynamic dict");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
    }
    xl6AstFree(receiverType);
    return 1;
}

static int xl6CgenErrorMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;

    if ( member == NULL || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return 0;
    }
    receiverType = xl6CgenFindVarType(ctx, receiver->text);
    if ( receiverType == NULL || !xl6CTextEquals(receiverType->text, "error") ) {
        return 0;
    }

    if ( xl6CTextEquals(member->text, "kind") ) {
        xl6OutputPuts("xl6_error_kind(", ctx->out);
    } else if ( xl6CTextEquals(member->text, "message") ) {
        xl6OutputPuts("xl6_error_message(", ctx->out);
    } else if ( xl6CTextEquals(member->text, "file") ) {
        xl6OutputPuts("xl6_error_file(", ctx->out);
    } else if ( xl6CTextEquals(member->text, "line") ) {
        xl6OutputPuts("xl6_error_line(", ctx->out);
    } else if ( xl6CTextEquals(member->text, "column") ) {
        xl6OutputPuts("xl6_error_column(", ctx->out);
    } else {
        return 0;
    }
    xl6CgenNamedValueReference(ctx, receiver->text);
    xl6OutputPutc(')', ctx->out);
    return 1;
}
static int xl6CgenSafeXValueMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;

    if ( member == NULL || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }

    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( !xl6CgenIsXValueType(receiverType) ) {
        xl6AstFree(receiverType);
        return 0;
    }

    if ( !xl6CTextEquals(member->text, "length") ) {
        if ( xl6CgenMemberKeyReceiverIsDynamic(ctx, receiverType) ) {
            xl6CgenXValueMemberKeyExpr(ctx, receiver, member->text);
        } else {
            xl6CgenError(ctx, member, "member key receiver must be var or dynamic dict");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        }
        xl6AstFree(receiverType);
        return 1;
    }


    if ( !xl6CgenExprIsXValueBorrow(ctx, receiver) ) {
        xl6OutputPuts("xl6_xrt_member_safe_length_release(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        xl6AstFree(receiverType);
        return 1;
    }

    xl6OutputPuts("xl6_xrt_member_safe_length(", ctx->out);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPutc(')', ctx->out);
    xl6AstFree(receiverType);
    return 1;
}
