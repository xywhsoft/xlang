static void xl6CgenRegisterParamTypes(XL6_CgenContext* ctx, const XL6_AstNode* params)
{
    size_t i;

    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( param != NULL && param->childCount > 0 ) {
            xl6CgenAddVarType(ctx, param->text, param->children[0]);
            if ( xl6CgenParamIsVararg(param) ) {
                xl6CgenAddXValueCleanup(ctx, param->text, param->children[0]);
            }
        }
    }
}

static int xl6CgenNodeContainsBaseCtorCall(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME &&
         xl6CTextEquals(node->children[0]->text, "base") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeContainsBaseCtorCall(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenEmitAutoBaseCreateWithSelfExpr(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* selfExpr, const XL6_AstNode* atNode)
{
    XL6_AstNode* baseClass;
    const XL6_AstNode* createNode;

    baseClass = xl6CgenBaseClass(ctx, classNode);
    if ( baseClass == NULL ) {
        return;
    }

    createNode = xl6CgenFindClassCreateCallableWithoutArgs(baseClass);
    if ( createNode == NULL ) {
        if ( xl6CgenClassHasCreate(baseClass) ) {
            xl6CgenError(ctx, atNode, "父类 create 需要参数，子类 create 必须显式调用 base(...)");
        }
        return;
    }

    xl6CgenIndent(ctx);
    xl6CgenEmitClassCreateCall(ctx, baseClass, createNode, selfExpr, NULL);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenEmitAutoBaseCreateForSelf(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* atNode)
{
    xl6CgenEmitAutoBaseCreateWithSelfExpr(ctx, classNode, "&self->base", atNode);
}

static void xl6CgenEmitAutoBaseCreateForValue(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* varPath, const XL6_AstNode* atNode)
{
    char selfExpr[512];

    if ( varPath == NULL ) {
        return;
    }
    snprintf(selfExpr, sizeof(selfExpr), "&%s.base", varPath);
    xl6CgenEmitAutoBaseCreateWithSelfExpr(ctx, classNode, selfExpr, atNode);
}

static void xl6CgenEmitCustomHandleCleanupPath(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const char* handlePath)
{
    const XL6_AstNode* destroyNode;

    if ( customNode == NULL || handlePath == NULL ) {
        return;
    }
    destroyNode = xl6CgenFindCustomSpecial(customNode, "destroy");
    if ( destroyNode == NULL ) {
        return;
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", handlePath);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6CgenCStringSymbol(ctx->out, destroyNode->text);
    xl6OutputPrintf(ctx->out, "(%s);\n", handlePath);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = 0;\n", handlePath);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenEmitClassDirectFieldCleanups(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    if ( body == NULL ) {
        return;
    }

    for ( i = body->childCount; i > 0; --i ) {
        const XL6_AstNode* member = body->children[i - 1];
        const XL6_AstNode* typeNode;
        XL6_AstNode* customNode;
        XL6_AstNode* nestedClass;
        char path[512];
        char cFieldName[512];

        if ( member == NULL || member->kind != XL6_AST_FIELD || member->childCount == 0 || member->text == NULL ) {
            continue;
        }
        xl6CgenCIdentText(cFieldName, sizeof(cFieldName), member->text);
        typeNode = member->children[0];
        customNode = xl6CgenFindCustom(ctx, typeNode != NULL ? typeNode->text : NULL);
        if ( customNode != NULL && xl6CgenCustomHasDestroy(customNode) ) {
            snprintf(path, sizeof(path), "self->%s.handle", cFieldName);
            xl6CgenEmitCustomHandleCleanupPath(ctx, customNode, path);
            continue;
        }

        nestedClass = xl6CgenFindClass(ctx, typeNode != NULL ? typeNode->text : NULL);
        if ( nestedClass != NULL && xl6CgenClassNeedsDestroy(ctx, nestedClass) ) {
            xl6CgenIndent(ctx);
            xl6CgenClassMemberSymbol(ctx->out, nestedClass->text, "destroy");
            xl6OutputPrintf(ctx->out, "(&self->%s);\n", cFieldName);
        }
    }
}

static void xl6CgenEmitClassBaseCleanup(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;

    if ( classNode == NULL ) {
        return;
    }
    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return;
    }

    baseClass = xl6CgenFindClass(ctx, base->text);
    if ( baseClass == NULL || !xl6CgenClassNeedsDestroy(ctx, baseClass) ) {
        return;
    }

    xl6CgenIndent(ctx);
    xl6CgenClassMemberSymbol(ctx->out, baseClass->text, "destroy");
    xl6OutputPuts("(&self->base);\n", ctx->out);
}

static void xl6CgenEmitClassDestroyTail(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    xl6CgenEmitClassDirectFieldCleanups(ctx, classNode);
    xl6CgenEmitClassBaseCleanup(ctx, classNode);
}

static void xl6CgenClassFunc(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    size_t varMark;

    xl6CgenClassFuncProto(ctx, classNode, funcNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    prevReturnType = ctx->currentReturnType;
    ctx->currentClass = classNode;
    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    ctx->currentReturnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    xl6CgenAddVarType(ctx, "self", classNode);
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    xl6CgenRegisterParamTypes(ctx, params);
    if ( funcNode->childCount > 3 ) {
        xl6CgenBlock(ctx, funcNode->children[3]);
    }
    ctx->currentReturnType = prevReturnType;
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenGenericClassFunc(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    size_t varMark;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    xl6CgenGenericClassFuncProto(ctx, classNode, typeNode, funcNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    prevReturnType = ctx->currentReturnType;
    ctx->currentClass = classNode;
    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    ctx->currentReturnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    xl6CgenAddVarType(ctx, "self", typeNode);
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    xl6CgenRegisterParamTypes(ctx, params);
    if ( funcNode->childCount > 3 ) {
        xl6CgenBlock(ctx, funcNode->children[3]);
    }
    ctx->currentReturnType = prevReturnType;
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenGenericClassSpecial(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const XL6_AstNode* specialNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    const XL6_AstNode* params;
    size_t varMark;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    xl6CgenGenericClassSpecialProto(ctx, classNode, typeNode, specialNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    prevReturnType = ctx->currentReturnType;
    ctx->currentClass = classNode;
    ctx->currentReturnType = NULL;
    xl6CgenAddVarType(ctx, "self", typeNode);
    params = specialNode->childCount > 0 ? specialNode->children[0] : NULL;
    xl6CgenRegisterParamTypes(ctx, params);
    if ( specialNode->childCount > 1 ) {
        xl6CgenBlock(ctx, specialNode->children[1]);
    }
    if ( xl6CTextEquals(specialNode->text, "destroy") ) {
        xl6CgenEmitClassDestroyTail(ctx, classNode);
    }
    ctx->currentReturnType = prevReturnType;
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenGenericClassAutoDestroy(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* prevClass;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    size_t varMark;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    xl6CgenGenericClassDestroyProto(ctx, classNode, typeNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    ctx->currentClass = classNode;
    xl6CgenAddVarType(ctx, "self", typeNode);
    xl6CgenEmitClassDestroyTail(ctx, classNode);
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenClassSpecial(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* specialNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* params;
    size_t varMark;

    xl6CgenClassSpecialProto(ctx, classNode, specialNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    prevReturnType = ctx->currentReturnType;
    ctx->currentClass = classNode;
    ctx->currentReturnType = NULL;
    xl6CgenAddVarType(ctx, "self", classNode);
    params = specialNode->childCount > 0 ? specialNode->children[0] : NULL;
    xl6CgenRegisterParamTypes(ctx, params);
    if ( xl6CTextEquals(specialNode->text, "create") &&
         !xl6CgenNodeContainsBaseCtorCall(specialNode->childCount > 1 ? specialNode->children[1] : NULL) ) {
        xl6CgenEmitAutoBaseCreateForSelf(ctx, classNode, specialNode);
    }
    if ( specialNode->childCount > 1 ) {
        xl6CgenBlock(ctx, specialNode->children[1]);
    }
    if ( xl6CTextEquals(specialNode->text, "destroy") ) {
        xl6CgenEmitClassDestroyTail(ctx, classNode);
    }
    ctx->currentReturnType = prevReturnType;
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassAutoDestroy(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* prevClass;
    size_t varMark;

    xl6CgenClassDestroyProto(ctx, classNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    ctx->currentClass = classNode;
    xl6CgenAddVarType(ctx, "self", classNode);
    xl6CgenEmitClassDestroyTail(ctx, classNode);
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassMethods(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;
    int hasDestroy;

    body = xl6CgenClassBody(classNode);
    if ( body == NULL || xl6CgenClassHasTypeParams(classNode) ) {
        return;
    }
    hasDestroy = 0;
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member == NULL ) {
            continue;
        }
        if ( member->kind == XL6_AST_FUNC ) {
            xl6CgenClassFunc(ctx, classNode, member);
        } else if ( member->kind == XL6_AST_CREATE ) {
            if ( xl6CTextEquals(member->text, "destroy") ) {
                hasDestroy = 1;
            }
            xl6CgenClassSpecial(ctx, classNode, member);
        }
    }
    if ( !hasDestroy && xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        xl6CgenClassAutoDestroy(ctx, classNode);
    }
}

static void xl6CgenGenericClassMethods(XL6_CgenContext* ctx)
{
    size_t i;
    size_t j;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->genericClassInstanceCount; ++i ) {
        const XL6_AstNode* classNode = ctx->genericClassInstances[i].classNode;
        const XL6_AstNode* typeNode = ctx->genericClassInstances[i].typeNode;
        const XL6_AstNode* body = xl6CgenClassBody(classNode);
        int hasDestroy;

        if ( body == NULL ) {
            continue;
        }
        hasDestroy = 0;
        for ( j = 0; j < body->childCount; ++j ) {
            const XL6_AstNode* member = body->children[j];
            if ( member != NULL && member->kind == XL6_AST_FUNC ) {
                xl6CgenGenericClassFunc(ctx, classNode, typeNode, member);
            } else if ( member != NULL && member->kind == XL6_AST_CREATE ) {
                if ( xl6CTextEquals(member->text, "destroy") ) {
                    hasDestroy = 1;
                }
                xl6CgenGenericClassSpecial(ctx, classNode, typeNode, member);
            }
        }
        if ( !hasDestroy && xl6CgenClassNeedsDestroy(ctx, classNode) ) {
            xl6CgenGenericClassAutoDestroy(ctx, classNode, typeNode);
        }
    }
}

static int xl6CgenBlockEndsWithReturn(const XL6_AstNode* block)
{
    const XL6_AstNode* last;

    if ( block == NULL || block->childCount == 0 ) {
        return 0;
    }
    last = block->children[block->childCount - 1];
    return last != NULL && last->kind == XL6_AST_RETURN;
}

static void xl6CgenCustomFunc(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevReturnParams;
    const XL6_AstNode* prevReturns;
    const XL6_AstNode* prevReturnFunc;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* prevCustom;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    const XL6_AstNode* body;
    size_t varMark;
    size_t cleanupMark;

    xl6CgenCustomFuncProto(ctx, customNode, funcNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;

    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    prevReturnType = ctx->currentReturnType;
    prevReturnParams = ctx->currentReturnParams;
    prevReturns = ctx->currentReturns;
    prevReturnFunc = ctx->currentReturnFunc;
    prevClass = ctx->currentClass;
    prevCustom = ctx->currentCustom;

    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    body = funcNode->childCount > 3 ? funcNode->children[3] : NULL;

    ctx->currentReturnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    ctx->currentReturnParams = params;
    ctx->currentReturns = returns;
    ctx->currentReturnFunc = funcNode;
    ctx->currentClass = NULL;
    ctx->currentCustom = customNode;

    xl6CgenAddVarType(ctx, "self", customNode);
    xl6CgenRegisterParamTypes(ctx, params);
    if ( body != NULL ) {
        xl6CgenBlock(ctx, body);
    }
    if ( !xl6CgenBlockEndsWithReturn(body) ) {
        xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    }
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);

    ctx->currentReturnType = prevReturnType;
    ctx->currentReturnParams = prevReturnParams;
    ctx->currentReturns = prevReturns;
    ctx->currentReturnFunc = prevReturnFunc;
    ctx->currentClass = prevClass;
    ctx->currentCustom = prevCustom;
    xl6CgenFreeVarsSince(ctx, varMark);

    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenCustomMethods(XL6_CgenContext* ctx, const XL6_AstNode* customNode)
{
    const XL6_AstNode* body;
    size_t i;

    body = customNode != NULL && customNode->childCount > 0 ? customNode->children[0] : NULL;
    if ( body == NULL ) {
        return;
    }

    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL && member->kind == XL6_AST_FUNC ) {
            xl6CgenCustomFunc(ctx, customNode, member);
        }
    }
}
