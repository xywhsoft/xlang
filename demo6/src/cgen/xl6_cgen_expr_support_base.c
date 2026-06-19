static void xl6CgenExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);

static void xl6CgenCStringSymbol(XL6_Output* out, const char* text)
{
    size_t len;
    size_t i;

    if ( text == NULL ) {
        xl6OutputPuts("_", out);
        return;
    }

    len = strlen(text);
    if ( len >= 2 && (text[0] == '"' || text[0] == '\'') ) {
        for ( i = 1; i + 1 < len; ++i ) {
            xl6OutputPutc(text[i], out);
        }
        return;
    }
    xl6OutputPuts(text, out);
}

static int xl6CgenCustomHasDestroy(const XL6_AstNode* customNode)
{
    return xl6CgenFindCustomSpecial(customNode, "destroy") != NULL;
}

static int xl6CgenClassHasDestroy(const XL6_AstNode* classNode)
{
    return xl6CgenFindClassSpecial(classNode, "destroy") != NULL;
}

static int xl6CgenIsFunctionType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL && xl6CTextEquals(typeNode->text, "function");
}

static int xl6CgenIsBareFunctionType(const XL6_AstNode* typeNode)
{
    return xl6CgenIsFunctionType(typeNode) && typeNode->childCount == 0;
}

static int xl6CgenTypeContainsFunctionType(const XL6_AstNode* typeNode)
{
    size_t i;

    if ( xl6CgenIsFunctionType(typeNode) ) {
        return 1;
    }
    for ( i = 0; typeNode != NULL && i < typeNode->childCount; ++i ) {
        if ( xl6CgenTypeContainsFunctionType(typeNode->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenIsArrayXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "array") || xl6CTextEquals(typeNode->text, "array<var>"));
}

static int xl6CgenIsListXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "list") || xl6CTextEquals(typeNode->text, "list<var>"));
}

static int xl6CgenIsSetXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "set") || xl6CTextEquals(typeNode->text, "set<var>"));
}

static int xl6CgenIsDictXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "dict") || xl6CTextEquals(typeNode->text, "dict<var>"));
}

static int xl6CgenIsXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "var") ||
            xl6CgenIsArrayXValueType(typeNode) ||
            xl6CgenIsListXValueType(typeNode) ||
            xl6CgenIsSetXValueType(typeNode) ||
            xl6CgenIsDictXValueType(typeNode));
}

static const char* xl6CgenXValueRuntimeTypeConst(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CTextEquals(typeName, "bool") ) {
        return "XL6_XVO_DT_BOOL";
    }
    if ( xl6CgenIsIntegerTypeName(typeName) ) {
        return "XL6_XVO_DT_INT";
    }
    if ( xl6CgenIsFloatTypeName(typeName) ) {
        return "XL6_XVO_DT_FLOAT";
    }
    if ( xl6CTextEquals(typeName, "string") ) {
        return "XL6_XVO_DT_TEXT";
    }
    if ( xl6CTextEquals(typeName, "time") ) {
        return "XL6_XVO_DT_TIME";
    }
    if ( xl6CTextEquals(typeName, "point") ) {
        return "XL6_XVO_DT_POINT";
    }
    if ( xl6CgenIsArrayXValueType(concreteType) ) {
        return "XL6_XVO_DT_ARRAY";
    }
    if ( xl6CgenIsListXValueType(concreteType) ) {
        return "XL6_XVO_DT_LIST";
    }
    if ( xl6CgenIsSetXValueType(concreteType) ) {
        return "XL6_XVO_DT_COLL";
    }
    if ( xl6CgenIsDictXValueType(concreteType) ) {
        return "XL6_XVO_DT_TABLE";
    }
    return NULL;
}

static int xl6CgenTypeSupportsVarInvokeValue(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    if ( concreteType == NULL || concreteType->text == NULL ) {
        return 0;
    }
    if ( xl6CTextEquals(concreteType->text, "var") ) {
        return 1;
    }
    return xl6CgenXValueRuntimeTypeConst(ctx, concreteType) != NULL;
}

static int xl6CgenAnonSupportsVarInvoke(XL6_CgenContext* ctx, const XL6_CgenAnonFunc* info)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int fixedCount;
    size_t i;

    if ( ctx == NULL || info == NULL || info->node == NULL ) {
        return 0;
    }

    params = info->node->childCount > 0 ? info->node->children[0] : NULL;
    returns = info->node->childCount > 1 ? info->node->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    if ( params != NULL && params->childCount > (size_t)fixedCount &&
         !xl6CgenParamIsArrayVararg(params->children[fixedCount]) ) {
        return 0;
    }
    if ( returns != NULL && returns->childCount > 1 ) {
        return 0;
    }
    if ( returns != NULL && returns->childCount == 1 &&
         !xl6CgenTypeSupportsVarInvokeValue(ctx, returns->children[0]) ) {
        return 0;
    }
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( param == NULL || param->childCount == 0 ||
                 !xl6CgenTypeSupportsVarInvokeValue(ctx, param->children[0]) ) {
                return 0;
            }
        }
    }
    return 1;
}

static const XL6_AstNode* xl6CgenFindDirectClassFieldNode(const XL6_AstNode* classNode, const char* fieldName)
{
    const XL6_AstNode* body;
    size_t i;

    if ( classNode == NULL || fieldName == NULL ) {
        return NULL;
    }
    body = xl6CgenClassBody(classNode);
    if ( body == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL && member->kind == XL6_AST_FIELD && xl6CTextEquals(member->text, fieldName) ) {
            return member;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindClassFieldNode(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* fieldName)
{
    const XL6_AstNode* field;
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;

    field = xl6CgenFindDirectClassFieldNode(classNode, fieldName);
    if ( field != NULL ) {
        return field;
    }

    base = classNode != NULL && classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return NULL;
    }
    baseClass = xl6CgenFindClass(ctx, base->text);
    return xl6CgenFindClassFieldNode(ctx, baseClass, fieldName);
}

static XL6_AstNode* xl6CgenCloneMemberFieldTypeForInference(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* field;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    XL6_AstNode* classNode;
    XL6_AstNode* result;

    if ( ctx == NULL || member == NULL || member->kind != XL6_AST_MEMBER ) {
        return NULL;
    }

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return NULL;
    }

    receiverType = NULL;
    if ( xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        classNode = (XL6_AstNode*)ctx->currentClass;
    } else {
        receiverType = xl6CgenFindVarType(ctx, receiver->text);
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    }
    if ( classNode == NULL ) {
        return NULL;
    }

    field = xl6CgenFindClassFieldNode(ctx, classNode, member->text);
    if ( field == NULL || field->childCount == 0 ) {
        return NULL;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( receiverType != NULL && receiverType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = receiverType->children[0];
    }

    result = xl6CgenCloneCanonicalTypeNode(ctx, field->children[0]);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    return result;
}

static int xl6CgenClassNeedsDestroy(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;
    size_t i;

    if ( classNode == NULL ) {
        return 0;
    }
    if ( xl6CgenClassHasDestroy(classNode) ) {
        return 1;
    }

    body = xl6CgenClassBody(classNode);
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            const XL6_AstNode* typeNode;
            XL6_AstNode* customNode;
            XL6_AstNode* nestedClass;

            if ( member == NULL || member->kind != XL6_AST_FIELD || member->childCount == 0 ) {
                continue;
            }
            typeNode = member->children[0];
            customNode = xl6CgenFindCustom(ctx, typeNode != NULL ? typeNode->text : NULL);
            if ( customNode != NULL && xl6CgenCustomHasDestroy(customNode) ) {
                return 1;
            }
            nestedClass = xl6CgenFindClass(ctx, typeNode != NULL ? typeNode->text : NULL);
            if ( nestedClass != NULL && xl6CgenClassNeedsDestroy(ctx, nestedClass) ) {
                return 1;
            }
        }
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        baseClass = xl6CgenFindClass(ctx, base->text);
        if ( xl6CgenClassNeedsDestroy(ctx, baseClass) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenAddCleanupEntry(XL6_CgenContext* ctx, const char* name, XL6_CgenCleanupKind kind, const XL6_AstNode* typeNode, const XL6_AstNode* classTypeNode)
{
    XL6_CgenCleanup* newItems;
    size_t newCapacity;
    char cName[512];

    if ( ctx == NULL || name == NULL || typeNode == NULL ) {
        return;
    }
    if ( ctx->cleanupCount == ctx->cleanupCapacity ) {
        newCapacity = ctx->cleanupCapacity == 0 ? 16 : ctx->cleanupCapacity * 2;
        newItems = (XL6_CgenCleanup*)xrtRealloc(ctx->cleanups, newCapacity * sizeof(XL6_CgenCleanup));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, typeNode, "code generation error");
            return;
        }
        ctx->cleanups = newItems;
        ctx->cleanupCapacity = newCapacity;
    }
    xl6CgenCIdentText(cName, sizeof(cName), name);
    ctx->cleanups[ctx->cleanupCount].name = xrtCopyStr(cName, strlen(cName));
    ctx->cleanups[ctx->cleanupCount].kind = kind;
    ctx->cleanups[ctx->cleanupCount].typeNode = typeNode;
    ctx->cleanups[ctx->cleanupCount].classTypeNode = classTypeNode;
    ctx->cleanupCount += 1;
}

static void xl6CgenAddCustomCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* customNode)
{
    if ( customNode == NULL || !xl6CgenCustomHasDestroy(customNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_CUSTOM, customNode, NULL);
}

static void xl6CgenAddClassCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* classNode, const XL6_AstNode* classTypeNode)
{
    if ( classNode == NULL || !xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_CLASS, classNode, classTypeNode);
}

static void xl6CgenAddFunctionCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsFunctionType(typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_FUNCTION, typeNode, NULL);
}

static void xl6CgenAddXValueCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsXValueType(typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_XVALUE, typeNode, NULL);
}

static void xl6CgenAddTypedContainerCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_TYPED_CONTAINER, typeNode, NULL);
}

static void xl6CgenFreeCleanupsSince(XL6_CgenContext* ctx, size_t mark)
{
    size_t i;

    if ( ctx == NULL || mark > ctx->cleanupCount ) {
        return;
    }
    for ( i = mark; i < ctx->cleanupCount; ++i ) {
        xrtFree(ctx->cleanups[i].name);
    }
    ctx->cleanupCount = mark;
}

static void xl6CgenFreeCleanups(XL6_CgenContext* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xl6CgenFreeCleanupsSince(ctx, 0);
    xrtFree(ctx->cleanups);
    ctx->cleanups = NULL;
    ctx->cleanupCapacity = 0;
}

static void xl6CgenEmitOneCleanup(XL6_CgenContext* ctx, const XL6_CgenCleanup* cleanup)
{
    const XL6_AstNode* destroyNode;

    if ( ctx == NULL || cleanup == NULL || cleanup->name == NULL ) {
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_AVLTB_ITER ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xrtAVLTB_IterEnd(%s);\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( cleanup->typeNode == NULL ) {
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_CUSTOM ) {
        destroyNode = xl6CgenFindCustomSpecial(cleanup->typeNode, "destroy");
        if ( destroyNode == NULL ) {
            return;
        }

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s.handle != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6CgenCStringSymbol(ctx->out, destroyNode->text);
        xl6OutputPrintf(ctx->out, "(%s.handle);\n", cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s.handle = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }

    if ( cleanup->kind == XL6_CGEN_CLEANUP_FUNCTION ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s.release != 0 && %s.env != 0) {\n", cleanup->name, cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s.release(%s.env);\n", cleanup->name, cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s.env = 0;\n", cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s.release = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }

    if ( cleanup->kind == XL6_CGEN_CLEANUP_XVALUE ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }

    if ( cleanup->kind == XL6_CGEN_CLEANUP_TYPED_CONTAINER ) {
        const XL6_AstNode* concreteType = xl6CgenTypedContainerConcreteType(ctx, cleanup->typeNode);
        const char* typeName = concreteType != NULL ? concreteType->text : NULL;

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        if ( xl6CTextEquals(typeName, "array") ) {
            xl6OutputPrintf(ctx->out, "xrtTypedArrayDestroy(%s);\n", cleanup->name);
        } else if ( xl6CTextEquals(typeName, "list") ) {
            xl6OutputPrintf(ctx->out, "xrtTypedListDestroy(%s);\n", cleanup->name);
        } else if ( xl6CTextEquals(typeName, "set") ) {
            xl6OutputPrintf(ctx->out, "xrtTypedSetDestroy(%s);\n", cleanup->name);
        } else if ( xl6CTextEquals(typeName, "dict") ) {
            xl6OutputPrintf(ctx->out, "xrtTypedDictDestroy(%s);\n", cleanup->name);
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }

    if ( !xl6CgenClassNeedsDestroy(ctx, cleanup->typeNode) ) {
        return;
    }

    xl6CgenIndent(ctx);
    if ( cleanup->classTypeNode != NULL && cleanup->classTypeNode->childCount > 0 ) {
        xl6CgenGenericClassMemberSymbol(ctx, cleanup->typeNode, cleanup->classTypeNode, "destroy");
    } else {
    xl6CgenClassMemberSymbol(ctx->out, cleanup->typeNode->text, "destroy");
    }
    xl6OutputPrintf(ctx->out, "(&%s);\n", cleanup->name);
}

static void xl6CgenRetainFunctionValue(XL6_CgenContext* ctx, const char* name)
{
    char cName[512];

    xl6CgenCIdentText(cName, sizeof(cName), name);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s.retain != 0 && %s.env != 0) {\n", cName, cName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.retain(%s.env);\n", cName, cName);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenReleaseFunctionValue(XL6_CgenContext* ctx, const char* name)
{
    char cName[512];

    xl6CgenCIdentText(cName, sizeof(cName), name);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s.release != 0 && %s.env != 0) {\n", cName, cName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.release(%s.env);\n", cName, cName);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static int xl6CgenExprIsFunctionBorrow(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* typeNode;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_NAME ) {
        return 0;
    }
    typeNode = xl6CgenFindVarType(ctx, expr->text);
    return xl6CgenIsFunctionType(typeNode);
}

static int xl6CgenExprIsXValueBorrow(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* typeNode;

    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }
    if ( expr->kind == XL6_AST_INDEX || expr->kind == XL6_AST_SAFE_INDEX ) {
        const XL6_AstNode* receiver = expr->childCount > 0 ? expr->children[0] : NULL;
        if ( xl6CgenExprUsesTypedContainerStorage(ctx, receiver) ) {
            return 0;
        }
        return 1;
    }
    if ( expr->kind != XL6_AST_NAME ) {
        return 0;
    }
    if ( xl6CgenExprUsesTypedContainerStorage(ctx, expr) ) {
        return 0;
    }
    typeNode = xl6CgenFindVarType(ctx, expr->text);
    return xl6CgenIsXValueType(typeNode);
}

static void xl6CgenEmitCleanupsSince(XL6_CgenContext* ctx, size_t mark)
{
    size_t i;

    if ( ctx == NULL || mark > ctx->cleanupCount ) {
        return;
    }
    for ( i = ctx->cleanupCount; i > mark; --i ) {
        xl6CgenEmitOneCleanup(ctx, &ctx->cleanups[i - 1]);
    }
}

static void xl6CgenCloseScope(XL6_CgenContext* ctx, size_t mark)
{
    xl6CgenEmitCleanupsSince(ctx, mark);
    xl6CgenFreeCleanupsSince(ctx, mark);
}

static void xl6CgenPushLoopTarget(XL6_CgenContext* ctx, size_t breakMark, size_t continueMark, int breakId, int continueId)
{
    XL6_CgenLoopTarget* newItems;
    size_t newCapacity;

    if ( ctx == NULL ) {
        return;
    }
    if ( ctx->loopTargetCount == ctx->loopTargetCapacity ) {
        newCapacity = ctx->loopTargetCapacity == 0 ? 8 : ctx->loopTargetCapacity * 2;
        newItems = (XL6_CgenLoopTarget*)xrtRealloc(ctx->loopTargets, newCapacity * sizeof(XL6_CgenLoopTarget));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, NULL, "code generation error");
            return;
        }
        ctx->loopTargets = newItems;
        ctx->loopTargetCapacity = newCapacity;
    }
    ctx->loopTargets[ctx->loopTargetCount].breakCleanupMark = breakMark;
    ctx->loopTargets[ctx->loopTargetCount].continueCleanupMark = continueMark;
    ctx->loopTargets[ctx->loopTargetCount].breakId = breakId;
    ctx->loopTargets[ctx->loopTargetCount].continueId = continueId;
    ctx->loopTargetCount += 1;
}

static void xl6CgenPopLoopTarget(XL6_CgenContext* ctx)
{
    if ( ctx != NULL && ctx->loopTargetCount > 0 ) {
        ctx->loopTargetCount -= 1;
    }
}

static XL6_CgenLoopTarget* xl6CgenLoopTargetAtLevel(XL6_CgenContext* ctx, int level)
{
    size_t index;

    if ( ctx == NULL || level <= 0 || (size_t)level > ctx->loopTargetCount ) {
        return NULL;
    }
    index = ctx->loopTargetCount - (size_t)level;
    return &ctx->loopTargets[index];
}

static void xl6CgenFreeLoopTargets(XL6_CgenContext* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xrtFree(ctx->loopTargets);
    ctx->loopTargets = NULL;
    ctx->loopTargetCount = 0;
    ctx->loopTargetCapacity = 0;
}

