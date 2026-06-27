static int xl6CgenFuncOverloadCount(XL6_CgenContext* ctx, const char* name)
{
    size_t i;
    int count;

    count = 0;
    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        XL6_AstNode* nameNode;
        if ( node == NULL || (node->kind != XL6_AST_FUNC && node->kind != XL6_AST_ASYNC_FUNC) || xl6CgenFuncIsGeneric(node) || node->childCount == 0 ) {
            continue;
        }
        nameNode = node->children[0];
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, name) ) {
            count += 1;
        }
    }
    return count;
}

static void xl6CgenMangledPart(XL6_Output* out, const char* text)
{
    xl6CgenFputsEncodedText(out, text);
}

static void xl6CgenMangledType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;
    size_t i;

    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        xl6OutputPutc('_', ctx->out);
        return;
    }

    xl6CgenMangledPart(ctx->out, typeNode->text);
    args = (typeNode->childCount > 0 && typeNode->children[0] != NULL &&
            typeNode->children[0]->kind == XL6_AST_LIST) ? typeNode->children[0] : NULL;
    if ( args == NULL || args->childCount == 0 ) {
        return;
    }

    xl6OutputPuts("_of", ctx->out);
    for ( i = 0; i < args->childCount; ++i ) {
        xl6OutputPuts("_", ctx->out);
        xl6CgenMangledType(ctx, args->children[i]);
    }
}

static int xl6CgenGenericFuncNeedsSignatureSuffix(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeParams;
    size_t i;
    int count;

    if ( ctx == NULL || ctx->root == NULL || funcNode == NULL || !xl6CgenFuncIsGeneric(funcNode) ) {
        return 0;
    }
    nameNode = funcNode->childCount > 0 ? funcNode->children[0] : NULL;
    typeParams = xl6CgenFuncTypeParams(funcNode);
    if ( nameNode == NULL || typeParams == NULL ) {
        return 0;
    }

    count = 0;
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        const XL6_AstNode* otherName;
        const XL6_AstNode* otherParams;

        if ( node == NULL || (node->kind != XL6_AST_FUNC && node->kind != XL6_AST_ASYNC_FUNC) || !xl6CgenFuncIsGeneric(node) ) {
            continue;
        }
        otherName = node->childCount > 0 ? node->children[0] : NULL;
        otherParams = xl6CgenFuncTypeParams(node);
        if ( otherName != NULL && otherParams != NULL &&
             xl6CTextEquals(otherName->text, nameNode->text) &&
             otherParams->childCount == typeParams->childCount ) {
            count += 1;
        }
    }
    return count > 1;
}

static void xl6CgenGenericFuncSignatureSuffix(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* params;
    size_t i;

    params = funcNode != NULL && funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    xl6OutputPuts("__sig", ctx->out);
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("__void", ctx->out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        xl6OutputPuts("__", ctx->out);
        if ( param != NULL && param->childCount > 0 && param->children[0] != NULL ) {
            xl6CgenMangledType(ctx, param->children[0]);
        } else {
            xl6OutputPutc('_', ctx->out);
        }
    }
}

static const char* xl6CgenCallableName(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    if ( node->kind == XL6_AST_FUNC ||
         node->kind == XL6_AST_ASYNC_FUNC ||
         node->kind == XL6_AST_CUSTOM_EXTERN_FUNC ||
         node->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL ) {
        const XL6_AstNode* nameNode = node->childCount > 0 ? node->children[0] : NULL;
        return nameNode != NULL ? nameNode->text : NULL;
    }
    return node->text;
}

static int xl6CgenClassCallableOverloadCount(const XL6_AstNode* classNode, const XL6_AstNode* memberNode)
{
    const XL6_AstNode* body;
    const char* name;
    size_t i;
    int count;

    body = xl6CgenClassBody(classNode);
    name = xl6CgenCallableName(memberNode);
    count = 0;
    if ( body == NULL || memberNode == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* other = body->children[i];
        if ( other != NULL &&
             other->kind == memberNode->kind &&
             xl6CTextEquals(xl6CgenCallableName(other), name) ) {
            count += 1;
        }
    }
    return count;
}

static void xl6CgenClassCallableSymbol(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* memberNode)
{
    const XL6_AstNode* params;
    const char* name;
    size_t i;

    name = xl6CgenCallableName(memberNode);
    xl6CgenClassMemberSymbol(ctx->out, classNode != NULL ? classNode->text : "_", name != NULL ? name : "_");

    if ( memberNode == NULL ||
         (memberNode->kind != XL6_AST_FUNC && memberNode->kind != XL6_AST_CREATE) ||
         xl6CgenClassCallableOverloadCount(classNode, memberNode) <= 1 ) {
        return;
    }

    params = xl6CgenCallableParams(memberNode);
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

static void xl6CgenCustomMemberSymbol(XL6_Output* out, const char* customName, const char* memberName)
{
    xl6OutputPuts("xl_custom_", out);
    xl6CgenFputsEncodedText(out, customName);
    xl6OutputPuts("__", out);
    xl6CgenFputsEncodedText(out, memberName);
}

static int xl6CgenCustomCallableOverloadCount(const XL6_AstNode* customNode, const XL6_AstNode* memberNode)
{
    const XL6_AstNode* body;
    const char* name;
    size_t i;
    int count;

    body = customNode != NULL && customNode->childCount > 0 ? customNode->children[0] : NULL;
    name = xl6CgenCallableName(memberNode);
    count = 0;
    if ( body == NULL || memberNode == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* other = body->children[i];
        if ( other != NULL &&
             other->kind == memberNode->kind &&
             xl6CTextEquals(xl6CgenCallableName(other), name) ) {
            count += 1;
        }
    }
    return count;
}

static void xl6CgenCustomCallableSymbol(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* memberNode)
{
    const XL6_AstNode* params;
    const char* name;
    size_t i;

    name = xl6CgenCallableName(memberNode);
    xl6CgenCustomMemberSymbol(ctx->out, customNode != NULL ? customNode->text : "_", name != NULL ? name : "_");

    if ( memberNode == NULL ||
         memberNode->kind != XL6_AST_FUNC ||
         xl6CgenCustomCallableOverloadCount(customNode, memberNode) <= 1 ) {
        return;
    }

    params = xl6CgenCallableParams(memberNode);
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

static void xl6CgenFuncSymbol(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    size_t i;

    nameNode = funcNode != NULL && funcNode->childCount > 0 ? funcNode->children[0] : NULL;
    if ( xl6CgenFuncIsGeneric(funcNode) ) {
        if ( ctx->currentGenericArgs == NULL ) {
            xl6CgenError(ctx, funcNode, "泛型函数必须先实例化再生成 C 符号");
            xl6OutputPuts("xl_gen_uninstantiated", ctx->out);
            return;
        }
        xl6OutputPuts("xl_gen_", ctx->out);
        xl6CgenMangledPart(ctx->out, nameNode != NULL ? nameNode->text : "_");
        for ( i = 0; i < ctx->currentGenericArgs->childCount; ++i ) {
            xl6OutputPuts("__", ctx->out);
            xl6CgenMangledType(ctx, ctx->currentGenericArgs->children[i]);
        }
        if ( xl6CgenGenericFuncNeedsSignatureSuffix(ctx, funcNode) ) {
            xl6CgenGenericFuncSignatureSuffix(ctx, funcNode);
        }
        return;
    }
    if ( xl6CgenFuncIsMainEntry(funcNode) ) {
        xl6OutputPuts("xlang_main", ctx->out);
        return;
    }
    if ( nameNode == NULL || (xl6CgenFuncOverloadCount(ctx, nameNode->text) <= 1 &&
         xl6CgenIsSimpleCIdent(nameNode->text) && !xl6CTextEquals(nameNode->text, "xlang_main")) ) {
        xl6OutputPuts((nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "_", ctx->out);
        return;
    }

    xl6OutputPuts("xl_fn_", ctx->out);
    xl6CgenMangledPart(ctx->out, nameNode->text);
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("__void", ctx->out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        xl6OutputPuts("__", ctx->out);
        if ( param != NULL && param->childCount > 0 && param->children[0] != NULL ) {
            xl6CgenMangledPart(ctx->out, param->children[0]->text);
        } else {
            xl6OutputPutc('_', ctx->out);
        }
    }
}

static void xl6CgenFunctionValueSymbol(XL6_CgenContext* ctx, const XL6_AstNode* funcNode, const char* suffix)
{
    xl6OutputPuts("xl6_fnvalue_", ctx->out);
    xl6CgenFuncSymbol(ctx, funcNode);
    xl6OutputPuts(suffix != NULL ? suffix : "", ctx->out);
}

static void xl6CgenCallableTargetSymbol(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    if ( funcNode != NULL && funcNode->kind == XL6_AST_EXTERN_FUNC ) {
        xl6CgenCStringSymbol(ctx->out, funcNode->text);
    } else if ( funcNode != NULL && funcNode->kind == XL6_AST_EXPORT_FUNC ) {
        xl6CgenExportInternalSymbol(ctx->out, funcNode);
    } else {
        xl6CgenFuncSymbol(ctx, funcNode);
    }
}

static void xl6CgenFunctionValueAdapterSymbol(XL6_CgenContext* ctx, const XL6_AstNode* funcNode, int paramCount, int includeVararg, const char* suffix)
{
    xl6OutputPuts("xl6_fnvalue_", ctx->out);
    xl6CgenFuncSymbol(ctx, funcNode);
    xl6OutputPrintf(ctx->out, "_arity%d", paramCount);
    if ( includeVararg ) {
        xl6OutputPuts("_vararg", ctx->out);
    }
    xl6OutputPuts(suffix != NULL ? suffix : "", ctx->out);
}

static int xl6CgenFunctionValueAdapterInfo(const XL6_AstNode* funcNode, const XL6_AstNode* targetType, int* outParamCount, int* outIncludeVararg)
{
    const XL6_AstNode* params;
    const XL6_AstNode* targetParams;
    size_t targetCount;
    size_t targetFixed;
    int targetHasArrayVararg;
    int targetHasDictVararg;
    int actualFixed;
    int actualRequired;
    int actualHasVararg;
    int includeVararg;

    if ( outParamCount != NULL ) {
        *outParamCount = 0;
    }
    if ( outIncludeVararg != NULL ) {
        *outIncludeVararg = 0;
    }
    if ( funcNode == NULL || targetType == NULL || !xl6CTextEquals(targetType->text, "function") ||
         targetType->childCount < 2 ) {
        return 0;
    }

    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    targetParams = targetType->children[0];
    targetCount = targetParams != NULL ? targetParams->childCount : 0;
    targetHasArrayVararg = targetCount > 0 && xl6CgenFunctionTypeParamIsArrayVararg(targetParams->children[targetCount - 1]);
    targetHasDictVararg = targetCount > 0 && xl6CgenFunctionTypeParamIsDictVararg(targetParams->children[targetCount - 1]);
    targetFixed = (targetHasArrayVararg || targetHasDictVararg) ? targetCount - 1 : targetCount;
    actualFixed = xl6CgenFixedParamCount(params);
    actualRequired = xl6CgenParamRequiredCount(params);
    actualHasVararg = params != NULL && params->childCount > (size_t)actualFixed;
    includeVararg = targetHasArrayVararg || targetHasDictVararg;

    if ( targetFixed >= (size_t)actualFixed || targetFixed < (size_t)actualRequired ) {
        return 0;
    }
    if ( includeVararg ) {
        const XL6_AstNode* actualVararg;

        if ( !actualHasVararg ) {
            return 0;
        }
        actualVararg = params->children[actualFixed];
        if ( targetHasArrayVararg && !xl6CgenParamIsArrayVararg(actualVararg) ) {
            return 0;
        }
        if ( targetHasDictVararg && !xl6CgenParamIsDictVararg(actualVararg) ) {
            return 0;
        }
    } else if ( actualHasVararg ) {
        return 0;
    }

    if ( outParamCount != NULL ) {
        *outParamCount = (int)targetFixed;
    }
    if ( outIncludeVararg != NULL ) {
        *outIncludeVararg = includeVararg;
    }
    return 1;
}

static void xl6CgenFunctionValueMakeCall(XL6_CgenContext* ctx, const XL6_AstNode* funcNode, const XL6_AstNode* targetType)
{
    int adapterParamCount;
    int adapterIncludeVararg;

    if ( funcNode == NULL ) {
        xl6OutputPuts("(XL6_Function){0}", ctx->out);
        return;
    }
    if ( xl6CgenFunctionValueAdapterInfo(funcNode, targetType, &adapterParamCount, &adapterIncludeVararg) ) {
        xl6CgenFunctionValueAdapterSymbol(ctx, funcNode, adapterParamCount, adapterIncludeVararg, "_make()");
        return;
    }
    xl6CgenFunctionValueSymbol(ctx, funcNode, "_make()");
}

static int xl6CgenFuncHasMultiReturn(const XL6_AstNode* funcNode)
{
    const XL6_AstNode* returns;

    returns = funcNode != NULL && funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    return returns != NULL && returns->childCount > 1;
}

static void xl6CgenFuncReturnStructName(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    params = funcNode != NULL && funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    returns = funcNode != NULL && funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    xl6CgenFunctionReturnStructName(ctx, params, returns);
}

static void xl6CgenFuncReturnStructDecl(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    if ( !xl6CgenFuncHasMultiReturn(funcNode) ) {
        return;
    }

    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    returns = funcNode->children[2];
    xl6CgenFunctionReturnStructDecl(ctx, params, returns);
}

static void xl6CgenFunctionReturnStructDeclsInNode(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return;
    }

    if ( node->kind == XL6_AST_TYPE && xl6CTextEquals(node->text, "function") && node->childCount > 1 ) {
        xl6CgenFunctionReturnStructDecl(ctx, node->children[0], node->children[1]);
    } else if ( node->kind == XL6_AST_ANON_FUNC && node->childCount > 1 ) {
        xl6CgenFunctionReturnStructDecl(ctx, node->childCount > 0 ? node->children[0] : NULL, node->children[1]);
    }

    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenFunctionReturnStructDeclsInNode(ctx, node->children[i]);
    }
}

static void xl6CgenFunctionReturnStructDecls(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    if ( ctx == NULL || root == NULL ) {
        return;
    }
    xl6CgenFunctionReturnStructDeclsInNode(ctx, root);
}
