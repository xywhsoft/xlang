static void xl6CgenXValueFromExprForTarget(XL6_CgenContext* ctx, const XL6_AstNode* targetType, const XL6_AstNode* expr)
{
    if ( xl6CgenIsRuntimeContainerTypeDesc(ctx, targetType) ) {
        xl6OutputPuts("xl6_xrt_bind_type(", ctx->out);
        xl6CgenXValueFromExpr(ctx, expr);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitRuntimeTypeDescRef(ctx, targetType);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    xl6CgenXValueFromExpr(ctx, expr);
}

static void xl6CgenDefaultXValueForTarget(XL6_CgenContext* ctx, const XL6_AstNode* targetType)
{
    if ( xl6CgenIsRuntimeContainerTypeDesc(ctx, targetType) ) {
        xl6OutputPuts("xl6_xrt_bind_type(", ctx->out);
        xl6CgenDefaultValue(ctx, targetType);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitRuntimeTypeDescRef(ctx, targetType);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    xl6CgenDefaultValue(ctx, targetType);
}

static int xl6CgenDeclShouldUseTypedContainer(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr, int declaredAuto)
{
    if ( !xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        return 0;
    }
    if ( !declaredAuto ) {
        return 1;
    }
    return initExpr != NULL && initExpr->kind == XL6_AST_TYPED_LITERAL;
}

static void xl6CgenTypedContainerCreateExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xrtTypedArrayCreate(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xrtTypedListCreate(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xrtTypedSetCreate(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xrtTypedDictCreate(", ctx->out);
    } else {
        xl6OutputPuts("(void*)(", ctx->out);
    }
    xl6CgenEmitXrtTypeDescRef(ctx, itemType);
    xl6OutputPuts(", XRT_OBJMODE_LOCAL)", ctx->out);
}

static const XL6_AstNode* xl6CgenTypedContainerInitLiteral(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( initExpr == NULL || typeName == NULL ) {
        return NULL;
    }

    if ( initExpr->kind == XL6_AST_TYPED_LITERAL ) {
        if ( xl6CTextEquals(initExpr->text, typeName) ) {
            return initExpr->childCount > 0 ? initExpr->children[0] : NULL;
        }
        return NULL;
    }
    if ( xl6CTextEquals(typeName, "array") && initExpr->kind == XL6_AST_ARRAY_LITERAL ) {
        return initExpr;
    }
    if ( xl6CTextEquals(typeName, "list") && initExpr->kind == XL6_AST_LIST_LITERAL ) {
        return initExpr;
    }
    if ( xl6CTextEquals(typeName, "set") && initExpr->kind == XL6_AST_SET_LITERAL ) {
        return initExpr;
    }
    if ( xl6CTextEquals(typeName, "dict") && initExpr->kind == XL6_AST_DICT_LITERAL ) {
        return initExpr;
    }
    return NULL;
}

static int xl6CgenFuncSingleReturnUsesTypedContainerStorage(XL6_CgenContext* ctx, const XL6_AstNode* funcNode);

static int xl6CgenTypedContainerExprTypeMatches(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr)
{
    XL6_AstNode* targetType;
    XL6_AstNode* exprType;
    int result;

    targetType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
    exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
    result = targetType != NULL &&
             exprType != NULL &&
             xl6CgenIsTypedContainerType(ctx, exprType) &&
             xl6CgenTypeNodeEqual(targetType, exprType);
    xl6AstFree(targetType);
    xl6AstFree(exprType);
    return result;
}

static int xl6CgenExprIsTypedContainerReturnCall(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    XL6_AstNode* func;

    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }

    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }

    func = xl6CgenResolveCallFunc(ctx, expr, callee->text);
    return xl6CgenFuncSingleReturnUsesTypedContainerStorage(ctx, func);
}

static int xl6CgenTypedContainerCanCopyFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr)
{
    if ( initExpr == NULL || xl6CgenTypedContainerInitLiteral(ctx, typeNode, initExpr) != NULL ) {
        return 0;
    }
    if ( !xl6CgenExprUsesTypedContainerStorage(ctx, initExpr) ) {
        return 0;
    }
    return xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
}

static int xl6CgenTypedContainerCanMoveFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr)
{
    if ( initExpr == NULL || xl6CgenTypedContainerInitLiteral(ctx, typeNode, initExpr) != NULL ) {
        return 0;
    }
    if ( !xl6CgenExprIsTypedContainerReturnCall(ctx, initExpr) ) {
        return 0;
    }
    return xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
}

static void xl6CgenTypedContainerCopyExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6_xrt_typed_array_copy(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6_xrt_typed_list_copy(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6_xrt_typed_set_copy(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6_xrt_typed_dict_copy(", ctx->out);
    } else {
        xl6CgenError(ctx, expr, "typed container copy requires array/list/set/dict");
        xl6OutputPuts("0", ctx->out);
        return;
    }
    xl6CgenEmitXrtTypeDescRef(ctx, itemType);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPutc(')', ctx->out);
}
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

static void xl6CgenTypedContainerFillFromLiteral(XL6_CgenContext* ctx, const char* cName, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr)
{
    const XL6_AstNode* literal;
    const XL6_AstNode* concreteType;
    const char* typeName;
    size_t i;

    if ( initExpr == NULL ) {
        return;
    }

    literal = xl6CgenTypedContainerInitLiteral(ctx, typeNode, initExpr);
    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( literal == NULL ) {
        xl6CgenError(ctx, initExpr, "静态泛型容器目前只支持同类容器字面量初始化");
        return;
    }

    if ( xl6CTextEquals(typeName, "array") ) {
        for ( i = 0; i < literal->childCount; ++i ) {
            xl6CgenTypedContainerValueStmt(ctx, cName, "xrtTypedArrayAppendValue(", literal->children[i]);
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
           ctx->currentCustom == NULL &&
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

static void xl6CgenTypedContainerValueFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr)
{
    const XL6_AstNode* literal;

    literal = xl6CgenTypedContainerInitLiteral(ctx, typeNode, expr);
    if ( literal != NULL ) {
        xl6CgenTypedContainerCreateExpr(ctx, typeNode);
        return;
    }

    if ( xl6CgenExprUsesTypedContainerStorage(ctx, expr) ) {
        xl6CgenTypedContainerCopyExpr(ctx, typeNode, expr);
        return;
    }

    xl6CgenExpr(ctx, expr);
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
static int xl6CgenIsCurrentCustomSelfReceiver(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    return ctx != NULL &&
           ctx->currentCustom != NULL &&
           receiver != NULL &&
           receiver->kind == XL6_AST_NAME &&
           xl6CTextEquals(receiver->text, "self");
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

static void xl6CgenCustomReceiverHandleExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    if ( xl6CgenIsCurrentCustomSelfReceiver(ctx, receiver) ) {
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

static void xl6CgenCustomReceiverPtrExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    if ( xl6CgenIsCurrentCustomSelfReceiver(ctx, receiver) ) {
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

static void xl6CgenCustomMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
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
        xl6CgenError(ctx, member, "custom 方法调用接收者必须是变量名或 class 字段");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    receiverName = receiver->kind == XL6_AST_NAME ? receiver->text : NULL;
    receiverType = receiver->kind == XL6_AST_NAME ? xl6CgenFindVarType(ctx, receiverName) : xl6CgenMemberFieldType(ctx, receiver);
    customNode = receiverType != NULL ? xl6CgenFindCustom(ctx, receiverType->text) : NULL;
    if ( customNode == NULL ) {
        xl6CgenError(ctx, member, "custom method receiver is not a custom value");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    helperMethod = xl6CgenResolveCustomFunc(ctx, customNode, call, member->text, 0);
    externMethod = xl6CgenResolveCustomExternFuncWithReport(ctx, customNode, call, member->text, 0);
    if ( helperMethod != NULL && externMethod != NULL ) {
        xl6CgenError(ctx, member, "custom 方法调用存在 helper / extern 歧义");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    method = helperMethod != NULL ? helperMethod : externMethod;
    if ( method == NULL ) {
        xl6CgenError(ctx, member, "找不到 custom extern 方法");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    if ( helperMethod != NULL ) {
        xl6CgenCustomCallableSymbol(ctx, customNode, method);
    } else {
        xl6CgenCStringSymbol(ctx->out, method->text);
    }
    xl6OutputPutc('(', ctx->out);
    if ( helperMethod != NULL ) {
        xl6CgenCustomReceiverPtrExpr(ctx, receiver);
    } else {
        xl6CgenCustomReceiverHandleExpr(ctx, receiver);
    }
    args = call->childCount > 1 ? call->children[1] : NULL;
    params = xl6CgenCallableParams(method);
    if ( params != NULL && params->childCount > 0 ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenKnownCallArgs(ctx, method, args);
    }
    xl6OutputPutc(')', ctx->out);
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
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* args;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    char basePath[256];
    int fixedCount;

    receiverType = NULL;
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return 0;
    }

    if ( xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        classNode = ctx->currentClass;
    } else {
        receiverType = xl6CgenFindVarType(ctx, receiver->text);
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    }
    if ( classNode == NULL ) {
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
    if ( xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
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
        xl6CgenNamedValueReference(ctx, receiver->text);
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
    int fixedCount;

    xl6CgenClassCallableSymbol(ctx, classNode, createNode);
    xl6OutputPutc('(', ctx->out);
    xl6OutputPuts(selfExpr != NULL ? selfExpr : "self", ctx->out);
    fixedCount = xl6CgenFixedParamCount(createNode != NULL && createNode->childCount > 0 ? createNode->children[0] : NULL);
    if ( fixedCount > 0 ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenParamNodeArgs(ctx, createNode->childCount > 0 ? createNode->children[0] : NULL, args);
    }
    xl6OutputPutc(')', ctx->out);
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
                              receiver->kind != XL6_AST_INDEX) ) {
        xl6CgenError(ctx, member, "class field receiver must be name, member or index");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    if ( receiver->kind == XL6_AST_NAME && xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        xl6OutputPuts("self", ctx->out);
        if ( !xl6CgenEmitClassFieldSuffixWithSep(ctx, ctx->currentClass, member->text, "->") ) {
            xl6CgenError(ctx, member, "class field not found");
        }
        return;
    }

    ownedReceiverType = NULL;
    if ( receiver->kind == XL6_AST_MEMBER || receiver->kind == XL6_AST_INDEX ) {
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

    xl6CgenNamedValueReference(ctx, receiver->text);
    if ( !xl6CgenEmitClassFieldSuffix(ctx, classNode, member->text) ) {
        xl6CgenError(ctx, member, "class field not found");
    }
}
static void xl6CgenXValueMemberKeyExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const char* key)
{
    int id;

    if ( xl6CgenExprIsXValueBorrow(ctx, receiver) ) {
        xl6OutputPuts("xl6_xrt_index_safe_get_key(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenCStringLiteral(ctx->out, key);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    id = ctx->tempId++;
    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_member_values_%d = ", id);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPrintf(ctx->out, "; xvalue __xl6_member_result_%d = xl6_xrt_retain(xl6_xrt_index_safe_get_key(__xl6_member_values_%d, ", id, id);
    xl6CgenCStringLiteral(ctx->out, key);
    xl6OutputPrintf(ctx->out, ")); if (__xl6_member_values_%d != 0) { xvoUnref(__xl6_member_values_%d); } __xl6_member_result_%d; })",
                     id,
                     id,
                     id);
}

static void xl6CgenXValueMemberLengthExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* receiverType)
{
    int id;

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

    id = ctx->tempId++;
    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_member_values_%d = ", id);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPrintf(ctx->out, "; unsigned int __xl6_member_result_%d = xl6_xrt_member_length(__xl6_member_values_%d); if (__xl6_member_values_%d != 0) { xvoUnref(__xl6_member_values_%d); } __xl6_member_result_%d; })",
                     id,
                     id,
                     id,
                     id,
                     id);
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
    int id;

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
        id = ctx->tempId++;
        xl6OutputPrintf(ctx->out, "({ xvalue __xl6_member_values_%d = ", id);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPrintf(ctx->out, "; xvalue __xl6_member_result_%d = xl6_xrt_member_safe_length(__xl6_member_values_%d); if (__xl6_member_values_%d != 0) { xvoUnref(__xl6_member_values_%d); } __xl6_member_result_%d; })",
                         id,
                         id,
                         id,
                         id,
                         id);
        xl6AstFree(receiverType);
        return 1;
    }

    xl6OutputPuts("xl6_xrt_member_safe_length(", ctx->out);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPutc(')', ctx->out);
    xl6AstFree(receiverType);
    return 1;
}
