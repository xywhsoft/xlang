static void xl6CgenFreeGenericClassInstances(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->genericClassInstanceCount; ++i ) {
        xl6AstFree((XL6_AstNode*)ctx->genericClassInstances[i].typeNode);
    }
    xrtFree(ctx->genericClassInstances);
    ctx->genericClassInstances = NULL;
    ctx->genericClassInstanceCount = 0;
    ctx->genericClassInstanceCapacity = 0;
}

static const XL6_AstNode* xl6CgenTypeNodeGenericArgs(const XL6_AstNode* typeNode)
{
    if ( typeNode == NULL || typeNode->childCount == 0 ) {
        return NULL;
    }
    if ( typeNode->childCount == 1 &&
         typeNode->children[0] != NULL &&
         typeNode->children[0]->kind == XL6_AST_LIST ) {
        return typeNode->children[0];
    }
    return NULL;
}

static int xl6CgenContainerGenericIsVar(XL6_CgenContext* ctx, const XL6_AstNode* concreteType)
{
    const XL6_AstNode* args;
    const XL6_AstNode* itemType;

    args = xl6CgenTypeNodeGenericArgs(concreteType);
    if ( args == NULL || args->childCount == 0 ) {
        return 1;
    }
    if ( args->childCount != 1 ) {
        return 0;
    }
    itemType = xl6CgenSubstituteGenericType(ctx, args->children[0]);
    itemType = xl6CgenResolveAliasType(ctx, itemType);
    return itemType != NULL && xl6CTextEquals(itemType->text, "var");
}

static int xl6CgenMemberKeyReceiverIsDynamic(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* name;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    name = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6CTextEquals(name, "var") ) {
        return 1;
    }
    if ( !xl6CTextEquals(name, "dict") && !xl6CTextEquals(name, "dict<var>") ) {
        return 0;
    }
    return xl6CgenContainerGenericIsVar(ctx, concreteType);
}

static int xl6CgenIsRuntimeContainerTypeDesc(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;
    const char* name;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    name = concreteType != NULL ? concreteType->text : NULL;
    args = xl6CgenTypeNodeGenericArgs(concreteType);
    return args != NULL && args->childCount > 0 &&
           (xl6CTextEquals(name, "array") ||
            xl6CTextEquals(name, "list") ||
            xl6CTextEquals(name, "set") ||
            xl6CTextEquals(name, "dict"));
}

static int xl6CgenIsContainerTypeName(const char* name)
{
    return xl6CTextEquals(name, "array") ||
           xl6CTextEquals(name, "list") ||
           xl6CTextEquals(name, "set") ||
           xl6CTextEquals(name, "dict");
}

static const XL6_AstNode* xl6CgenTypedContainerConcreteType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;
    const XL6_AstNode* itemType;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    if ( concreteType == NULL || !xl6CgenIsContainerTypeName(concreteType->text) ) {
        return NULL;
    }

    args = xl6CgenTypeNodeGenericArgs(concreteType);
    if ( args == NULL || args->childCount != 1 ) {
        return NULL;
    }

    itemType = xl6CgenSubstituteGenericType(ctx, args->children[0]);
    itemType = xl6CgenResolveAliasType(ctx, itemType);
    if ( itemType == NULL || xl6CTextEquals(itemType->text, "var") ) {
        return NULL;
    }
    return concreteType;
}

static int xl6CgenIsTypedContainerType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return xl6CgenTypedContainerConcreteType(ctx, typeNode) != NULL;
}

static const char* xl6CgenTypedContainerCType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* name;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    name = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6CTextEquals(name, "array") ) {
        return "xtarray";
    }
    if ( xl6CTextEquals(name, "list") ) {
        return "xtlist";
    }
    if ( xl6CTextEquals(name, "set") ) {
        return "xtset";
    }
    if ( xl6CTextEquals(name, "dict") ) {
        return "xtdict";
    }
    return "void*";
}

static const XL6_AstNode* xl6CgenTypedContainerItemType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    args = xl6CgenTypeNodeGenericArgs(concreteType);
    return args != NULL && args->childCount == 1 ? args->children[0] : NULL;
}

static int xl6CgenExprUsesTypedContainerStorage(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    return expr != NULL &&
           expr->kind == XL6_AST_NAME &&
           (xl6CgenFindTrackedVarFlags(ctx, expr->text) & XL6_CGEN_VAR_TYPED_CONTAINER) != 0;
}

static int xl6CgenHasRuntimeTypeDesc(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    size_t i;

    if ( ctx == NULL || typeNode == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->runtimeTypeDescCount; ++i ) {
        if ( xl6CgenTypeNodeEqual(ctx->runtimeTypeDescs[i].typeNode, typeNode) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenAddRuntimeTypeDesc(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    XL6_CgenRuntimeTypeDesc* newItems;
    XL6_AstNode* canonicalType;
    size_t newCapacity;

    if ( ctx == NULL || !xl6CgenIsRuntimeContainerTypeDesc(ctx, typeNode) ) {
        return;
    }
    canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
    if ( canonicalType == NULL ) {
        xl6CgenError(ctx, typeNode, "C 生成运行时类型描述规范化失败");
        return;
    }
    if ( xl6CgenHasRuntimeTypeDesc(ctx, canonicalType) ) {
        xl6AstFree(canonicalType);
        return;
    }
    if ( ctx->runtimeTypeDescCount == ctx->runtimeTypeDescCapacity ) {
        newCapacity = ctx->runtimeTypeDescCapacity == 0 ? 8 : ctx->runtimeTypeDescCapacity * 2;
        newItems = (XL6_CgenRuntimeTypeDesc*)xrtRealloc(ctx->runtimeTypeDescs, newCapacity * sizeof(XL6_CgenRuntimeTypeDesc));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, typeNode, "C 生成运行时类型描述表内存不足");
            xl6AstFree(canonicalType);
            return;
        }
        ctx->runtimeTypeDescs = newItems;
        ctx->runtimeTypeDescCapacity = newCapacity;
    }
    ctx->runtimeTypeDescs[ctx->runtimeTypeDescCount].typeNode = canonicalType;
    ctx->runtimeTypeDescCount += 1;
}

static void xl6CgenFreeRuntimeTypeDescs(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->runtimeTypeDescCount; ++i ) {
        xl6AstFree((XL6_AstNode*)ctx->runtimeTypeDescs[i].typeNode);
    }
    xrtFree(ctx->runtimeTypeDescs);
    ctx->runtimeTypeDescs = NULL;
    ctx->runtimeTypeDescCount = 0;
    ctx->runtimeTypeDescCapacity = 0;
}

static XL6_CgenAnonFunc* xl6CgenFindAnonFuncInfo(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->anonFuncCount; ++i ) {
        if ( ctx->anonFuncs[i].node == node ) {
            return &ctx->anonFuncs[i];
        }
    }
    return NULL;
}

static XL6_CgenAnonFunc* xl6CgenAddAnonFuncInfo(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_CgenAnonFunc* info;
    XL6_CgenAnonFunc* newItems;
    size_t newCapacity;

    info = xl6CgenFindAnonFuncInfo(ctx, node);
    if ( info != NULL ) {
        return info;
    }
    if ( ctx == NULL || node == NULL ) {
        return NULL;
    }
    if ( ctx->anonFuncCount == ctx->anonFuncCapacity ) {
        newCapacity = ctx->anonFuncCapacity == 0 ? 8 : ctx->anonFuncCapacity * 2;
        newItems = (XL6_CgenAnonFunc*)xrtRealloc(ctx->anonFuncs, newCapacity * sizeof(XL6_CgenAnonFunc));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, node, "匿名函数表内存不足");
            return NULL;
        }
        ctx->anonFuncs = newItems;
        memset(ctx->anonFuncs + ctx->anonFuncCapacity, 0, (newCapacity - ctx->anonFuncCapacity) * sizeof(XL6_CgenAnonFunc));
        ctx->anonFuncCapacity = newCapacity;
    }
    info = &ctx->anonFuncs[ctx->anonFuncCount];
    memset(info, 0, sizeof(*info));
    info->node = node;
    info->id = (int)ctx->anonFuncCount;
    ctx->anonFuncCount += 1;
    return info;
}

static int xl6CgenAnonHasCapture(const XL6_CgenAnonFunc* info, const char* name)
{
    size_t i;

    if ( info == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < info->captureCount; ++i ) {
        if ( xl6CTextEquals(info->captures[i].name, name) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenAnonAddCapture(XL6_CgenContext* ctx, XL6_CgenAnonFunc* info, const char* name, const XL6_AstNode* type)
{
    XL6_CgenAnonCapture* newItems;
    size_t newCapacity;

    if ( ctx == NULL || info == NULL || name == NULL || type == NULL || xl6CgenAnonHasCapture(info, name) ) {
        return;
    }
    if ( info->captureCount == info->captureCapacity ) {
        newCapacity = info->captureCapacity == 0 ? 4 : info->captureCapacity * 2;
        newItems = (XL6_CgenAnonCapture*)xrtRealloc(info->captures, newCapacity * sizeof(XL6_CgenAnonCapture));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, info->node, "匿名函数捕获表内存不足");
            return;
        }
        info->captures = newItems;
        info->captureCapacity = newCapacity;
    }
    info->captures[info->captureCount].name = xrtCopyStr((char*)name, strlen(name));
    info->captures[info->captureCount].type = type;
    info->captureCount += 1;
}

static void xl6CgenFreeAnonFuncs(XL6_CgenContext* ctx)
{
    size_t i;
    size_t j;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->anonFuncCount; ++i ) {
        for ( j = 0; j < ctx->anonFuncs[i].captureCount; ++j ) {
            xrtFree(ctx->anonFuncs[i].captures[j].name);
        }
        xrtFree(ctx->anonFuncs[i].captures);
    }
    xrtFree(ctx->anonFuncs);
    ctx->anonFuncs = NULL;
    ctx->anonFuncCount = 0;
    ctx->anonFuncCapacity = 0;
}

static void xl6CgenOwnTypeNode(XL6_CgenContext* ctx, XL6_AstNode* typeNode)
{
    XL6_AstNode** newItems;
    size_t newCapacity;

    if ( ctx == NULL || typeNode == NULL ) {
        return;
    }
    if ( ctx->ownedTypeNodeCount == ctx->ownedTypeNodeCapacity ) {
        newCapacity = ctx->ownedTypeNodeCapacity == 0 ? 8 : ctx->ownedTypeNodeCapacity * 2;
        newItems = (XL6_AstNode**)xrtRealloc(ctx->ownedTypeNodes, newCapacity * sizeof(XL6_AstNode*));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, typeNode, "C 生成类型节点表内存不足");
            return;
        }
        ctx->ownedTypeNodes = newItems;
        ctx->ownedTypeNodeCapacity = newCapacity;
    }
    ctx->ownedTypeNodes[ctx->ownedTypeNodeCount] = typeNode;
    ctx->ownedTypeNodeCount += 1;
}

static void xl6CgenFreeOwnedTypeNodes(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->ownedTypeNodeCount; ++i ) {
        xl6AstFree(ctx->ownedTypeNodes[i]);
    }
    xrtFree(ctx->ownedTypeNodes);
    ctx->ownedTypeNodes = NULL;
    ctx->ownedTypeNodeCount = 0;
    ctx->ownedTypeNodeCapacity = 0;
}

static int xl6CgenCallIsCustomCtor(XL6_CgenContext* ctx, const XL6_AstNode* expr, XL6_AstNode** outCustom);
static int xl6CgenCallIsClassCtor(XL6_CgenContext* ctx, const XL6_AstNode* expr, XL6_AstNode** outClass);
static XL6_AstNode* xl6CgenResolveCallFunc(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name);
static const XL6_AstNode* xl6CgenResolveClassFunc(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* call, const char* name, const XL6_AstNode** outOwnerClass);
static const XL6_AstNode* xl6CgenResolveClassCreate(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* call);
static const XL6_AstNode* xl6CgenResolveCustomFunc(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* call, const char* name, int reportNoMatch);
static const XL6_AstNode* xl6CgenResolveCustomExternFuncWithReport(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* call, const char* name, int reportNoMatch);
static const XL6_AstNode* xl6CgenResolveCustomExternFunc(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* call, const char* name);
static const XL6_AstNode* xl6CgenResolveCustomCreate(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* call);
static XL6_AstNode* xl6CgenBaseClass(XL6_CgenContext* ctx, const XL6_AstNode* classNode);
static XL6_AstNode* xl6CgenFindExternFunc(XL6_CgenContext* ctx, const char* name);
static void xl6CgenFuncSymbol(XL6_CgenContext* ctx, const XL6_AstNode* funcNode);
static void xl6CgenClassCallableSymbol(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* memberNode);
static void xl6CgenClassDefaultInitFields(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* varPath);
static void xl6CgenCollectGenericInstances(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenCollectRuntimeTypeDescs(XL6_CgenContext* ctx, const XL6_AstNode* root);
static void xl6CgenTypeText(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, char* buffer, size_t capacity);
static void xl6CgenGenericClassMemberSymbol(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const char* memberName);
static void xl6CgenGenericClassCallableSymbol(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const XL6_AstNode* memberNode);
static int xl6CgenFixedParamCount(const XL6_AstNode* params);
static int xl6CgenParamRequiredCount(const XL6_AstNode* params);
static const XL6_AstNode* xl6CgenCallableParams(const XL6_AstNode* node);
static int xl6CgenParamIsArrayVararg(const XL6_AstNode* param);
static int xl6CgenParamIsDictVararg(const XL6_AstNode* param);
static int xl6CgenParamIsVararg(const XL6_AstNode* param);
static const XL6_AstNode* xl6CgenFindArgForParam(const XL6_AstNode* args, const char* name, int index);
static const XL6_AstNode* xl6CgenMemberFieldType(XL6_CgenContext* ctx, const XL6_AstNode* member);
static XL6_AstNode* xl6CgenCloneMemberFieldTypeForInference(XL6_CgenContext* ctx, const XL6_AstNode* member);
static int xl6CgenIsFunctionType(const XL6_AstNode* typeNode);
static int xl6CgenIsBareFunctionType(const XL6_AstNode* typeNode);
static int xl6CgenIsArrayXValueType(const XL6_AstNode* typeNode);
static int xl6CgenIsListXValueType(const XL6_AstNode* typeNode);
static int xl6CgenIsSetXValueType(const XL6_AstNode* typeNode);
static int xl6CgenIsDictXValueType(const XL6_AstNode* typeNode);
static int xl6CgenIsXValueType(const XL6_AstNode* typeNode);
static const XL6_AstNode* xl6CgenFunctionReturnTypes(const XL6_AstNode* functionType);
static void xl6CgenRegisterParamTypes(XL6_CgenContext* ctx, const XL6_AstNode* params);
static unsigned long long xl6CgenFunctionSignatureId(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns);
static unsigned long long xl6CgenFunctionSignatureIdForPrefix(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns, size_t paramCount, int includeVararg);
static void xl6CgenFunctionReturnStructName(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns);
static void xl6CgenFunctionReturnStructNameForPrefix(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns, size_t paramCount, int includeVararg);
static void xl6CgenFunctionReturnStructDecl(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns);
static void xl6CgenFunctionReturnStructDeclForPrefix(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns, size_t paramCount, int includeVararg);
static void xl6CgenEmitFunctionSigObject(XL6_CgenContext* ctx, const char* localName, const char* name, unsigned long long signatureId, const XL6_AstNode* params, const XL6_AstNode* returns);
static void xl6CgenEmitFunctionSigObjectForCounts(XL6_CgenContext* ctx, const char* localName, const char* name, unsigned long long signatureId, unsigned int paramCount, unsigned int returnCount);
static XL6_AstNode* xl6CgenFindFunctionValueFunc(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* functionType, const XL6_AstNode* atNode);
static void xl6CgenFunctionValueMakeCall(XL6_CgenContext* ctx, const XL6_AstNode* funcNode, const XL6_AstNode* targetType);
static void xl6CgenFunctionValueExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* targetType);
static int xl6CgenAnonCaptureIndex(const XL6_CgenAnonFunc* info, const char* name);
static int xl6CgenIsRangeCall(const XL6_AstNode* node);
static void xl6CgenRangeContainsExpr(XL6_CgenContext* ctx, const XL6_AstNode* value, const XL6_AstNode* rangeCall);
static void xl6CgenInExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static int xl6CgenExprIntPowKind(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static void xl6CgenPowExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenApproxExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenStringConcatExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenStringCompareExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenTimeLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenIndexExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenXValueFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static const XL6_AstNode* xl6CgenArgExpr(const XL6_AstNode* arg);
static void xl6CgenCStringLiteral(XL6_Output* out, const char* text);
static void xl6CgenArrayVarargExpr(XL6_CgenContext* ctx, const XL6_AstNode* args, int fixedCount);
static void xl6CgenEmitAutoBaseCreateForValue(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* varPath, const XL6_AstNode* atNode);
static void xl6CgenCustomCtorValue(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* call);
static void xl6CgenHashText(unsigned long long* hash, const char* text);
static void xl6CgenHashTypeNode(unsigned long long* hash, const XL6_AstNode* typeNode);
static int xl6CgenIsKnownTypeName(XL6_CgenContext* ctx, const char* name);
static void xl6CgenEmitTypeIdForName(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* at);
static void xl6CgenEmitTypeOfExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static void xl6CgenEmitRuntimeTypeDescRef(XL6_CgenContext* ctx, const XL6_AstNode* typeNode);
static void xl6CgenEmitXrtTypeDescRef(XL6_CgenContext* ctx, const XL6_AstNode* typeNode);
static XL6_AstNode* xl6CgenCloneExprTypeForInference(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static XL6_AstNode* xl6CgenCloneExprTypeForTypeValue(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static int xl6CgenIsToStringMemberCallSyntax(const XL6_AstNode* node);
static int xl6CgenIsImplicitToStringCall(XL6_CgenContext* ctx, const XL6_AstNode* node);
static XL6_AstNode* xl6CgenResolveCallFuncInternal(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name, XL6_AstNode** outTypeArgs, int report);
static XL6_AstNode* xl6CgenResolveCallFuncWithTypeArgs(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name, XL6_AstNode** outTypeArgs);
static XL6_AstNode* xl6CgenResolveExplicitGenericCallFunc(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name, const XL6_AstNode* typeArgs, int report);
