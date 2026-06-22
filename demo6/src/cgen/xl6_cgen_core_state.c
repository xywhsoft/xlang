static const XL6_CgenVar* xl6CgenFindTrackedVar(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = ctx->varCount; i > 0; --i ) {
        if ( xl6CTextEquals(ctx->vars[i - 1].name, name) ) {
            return &ctx->vars[i - 1];
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindTrackedVarType(XL6_CgenContext* ctx, const char* name)
{
    const XL6_CgenVar* varInfo;

    varInfo = xl6CgenFindTrackedVar(ctx, name);
    return varInfo != NULL ? varInfo->type : NULL;
}

static unsigned int xl6CgenFindTrackedVarFlags(XL6_CgenContext* ctx, const char* name)
{
    const XL6_CgenVar* varInfo;

    varInfo = xl6CgenFindTrackedVar(ctx, name);
    return varInfo != NULL ? varInfo->flags : 0u;
}

static const XL6_AstNode* xl6CgenFindVarType(XL6_CgenContext* ctx, const char* name)
{
    const XL6_AstNode* typeNode;
    const XL6_AstNode* externalVar;

    typeNode = xl6CgenFindTrackedVarType(ctx, name);
    if ( typeNode != NULL ) {
        return typeNode;
    }
    externalVar = xl6CgenFindExternalVar(ctx, name);
    return xl6CgenExternalVarTypeNode(externalVar);
}

static void xl6CgenAddVarTypeWithFlags(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* type, unsigned int flags)
{
    XL6_CgenVar* newItems;
    size_t newCapacity;

    if ( ctx == NULL || name == NULL || type == NULL ) {
        return;
    }
    if ( ctx->varCount == ctx->varCapacity ) {
        newCapacity = ctx->varCapacity == 0 ? 16 : ctx->varCapacity * 2;
        newItems = (XL6_CgenVar*)xrtRealloc(ctx->vars, newCapacity * sizeof(XL6_CgenVar));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, type, "C 生成变量类型表内存不足");
            return;
        }
        ctx->vars = newItems;
        ctx->varCapacity = newCapacity;
    }
    ctx->vars[ctx->varCount].name = xrtCopyStr((char*)name, strlen(name));
    ctx->vars[ctx->varCount].type = type;
    ctx->vars[ctx->varCount].flags = flags;
    ctx->varCount += 1;
}

static void xl6CgenAddVarType(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* type)
{
    xl6CgenAddVarTypeWithFlags(ctx, name, type, 0u);
}

static void xl6CgenFreeVars(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->varCount; ++i ) {
        xrtFree(ctx->vars[i].name);
    }
    xrtFree(ctx->vars);
    ctx->vars = NULL;
    ctx->varCount = 0;
    ctx->varCapacity = 0;
}

static void xl6CgenFreeVarsSince(XL6_CgenContext* ctx, size_t mark)
{
    size_t i;

    if ( ctx == NULL || mark > ctx->varCount ) {
        return;
    }
    for ( i = mark; i < ctx->varCount; ++i ) {
        xrtFree(ctx->vars[i].name);
    }
    ctx->varCount = mark;
}

static int xl6CgenClassWasEmitted(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->emittedClassCount; ++i ) {
        if ( xl6CTextEquals(ctx->emittedClasses[i], name) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenMarkClassEmitted(XL6_CgenContext* ctx, const char* name)
{
    char** newItems;
    size_t newCapacity;

    if ( ctx == NULL || name == NULL || xl6CgenClassWasEmitted(ctx, name) ) {
        return;
    }
    if ( ctx->emittedClassCount == ctx->emittedClassCapacity ) {
        newCapacity = ctx->emittedClassCapacity == 0 ? 16 : ctx->emittedClassCapacity * 2;
        newItems = (char**)xrtRealloc(ctx->emittedClasses, newCapacity * sizeof(char*));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, NULL, "C 生成 class 表内存不足");
            return;
        }
        ctx->emittedClasses = newItems;
        ctx->emittedClassCapacity = newCapacity;
    }
    ctx->emittedClasses[ctx->emittedClassCount] = xrtCopyStr((char*)name, strlen(name));
    ctx->emittedClassCount += 1;
}

static void xl6CgenFreeEmittedClasses(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->emittedClassCount; ++i ) {
        xrtFree(ctx->emittedClasses[i]);
    }
    xrtFree(ctx->emittedClasses);
    ctx->emittedClasses = NULL;
    ctx->emittedClassCount = 0;
    ctx->emittedClassCapacity = 0;
}

static int xl6CgenUnionWasEmitted(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->emittedUnionCount; ++i ) {
        if ( xl6CTextEquals(ctx->emittedUnions[i], name) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenMarkUnionEmitted(XL6_CgenContext* ctx, const char* name)
{
    char** newItems;
    size_t newCapacity;

    if ( ctx == NULL || name == NULL || xl6CgenUnionWasEmitted(ctx, name) ) {
        return;
    }
    if ( ctx->emittedUnionCount == ctx->emittedUnionCapacity ) {
        newCapacity = ctx->emittedUnionCapacity == 0 ? 16 : ctx->emittedUnionCapacity * 2;
        newItems = (char**)xrtRealloc(ctx->emittedUnions, newCapacity * sizeof(char*));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, NULL, "C union emit table allocation failed");
            return;
        }
        ctx->emittedUnions = newItems;
        ctx->emittedUnionCapacity = newCapacity;
    }
    ctx->emittedUnions[ctx->emittedUnionCount] = xrtCopyStr((char*)name, strlen(name));
    ctx->emittedUnionCount += 1;
}

static void xl6CgenFreeEmittedUnions(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->emittedUnionCount; ++i ) {
        xrtFree(ctx->emittedUnions[i]);
    }
    xrtFree(ctx->emittedUnions);
    ctx->emittedUnions = NULL;
    ctx->emittedUnionCount = 0;
    ctx->emittedUnionCapacity = 0;
}

static int xl6CgenHasFunctionReturnStruct(const XL6_CgenContext* ctx, unsigned long long signatureId)
{
    size_t i;

    if ( ctx == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->emittedFunctionReturnCount; ++i ) {
        if ( ctx->emittedFunctionReturns[i] == signatureId ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenRememberFunctionReturnStruct(XL6_CgenContext* ctx, unsigned long long signatureId)
{
    unsigned long long* newItems;
    size_t newCapacity;

    if ( ctx == NULL || xl6CgenHasFunctionReturnStruct(ctx, signatureId) ) {
        return 0;
    }
    if ( ctx->emittedFunctionReturnCount == ctx->emittedFunctionReturnCapacity ) {
        newCapacity = ctx->emittedFunctionReturnCapacity == 0 ? 16 : ctx->emittedFunctionReturnCapacity * 2;
        newItems = (unsigned long long*)xrtRealloc(ctx->emittedFunctionReturns, newCapacity * sizeof(unsigned long long));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, NULL, "C 生成 function 返回结构体表内存不足");
            return 0;
        }
        ctx->emittedFunctionReturns = newItems;
        ctx->emittedFunctionReturnCapacity = newCapacity;
    }
    ctx->emittedFunctionReturns[ctx->emittedFunctionReturnCount] = signatureId;
    ctx->emittedFunctionReturnCount += 1;
    return 1;
}

static void xl6CgenFreeFunctionReturnStructs(XL6_CgenContext* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xrtFree(ctx->emittedFunctionReturns);
    ctx->emittedFunctionReturns = NULL;
    ctx->emittedFunctionReturnCount = 0;
    ctx->emittedFunctionReturnCapacity = 0;
}

static int xl6CgenPushTryTarget(XL6_CgenContext* ctx, size_t cleanupMark, int id, int targetFinally)
{
    XL6_CgenTryTarget* newItems;
    size_t newCapacity;

    if ( ctx == NULL ) {
        return 0;
    }
    if ( ctx->tryTargetCount == ctx->tryTargetCapacity ) {
        newCapacity = ctx->tryTargetCapacity == 0 ? 8 : ctx->tryTargetCapacity * 2;
        newItems = (XL6_CgenTryTarget*)xrtRealloc(ctx->tryTargets, newCapacity * sizeof(XL6_CgenTryTarget));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, NULL, "C 生成异常目标栈内存不足");
            return 0;
        }
        ctx->tryTargets = newItems;
        ctx->tryTargetCapacity = newCapacity;
    }
    ctx->tryTargets[ctx->tryTargetCount].cleanupMark = cleanupMark;
    ctx->tryTargets[ctx->tryTargetCount].id = id;
    ctx->tryTargets[ctx->tryTargetCount].targetFinally = targetFinally;
    ctx->tryTargetCount += 1;
    return 1;
}

static void xl6CgenPopTryTarget(XL6_CgenContext* ctx)
{
    if ( ctx != NULL && ctx->tryTargetCount > 0 ) {
        ctx->tryTargetCount -= 1;
    }
}

static XL6_CgenTryTarget* xl6CgenCurrentTryTarget(XL6_CgenContext* ctx)
{
    if ( ctx == NULL || ctx->tryTargetCount == 0 ) {
        return NULL;
    }
    return &ctx->tryTargets[ctx->tryTargetCount - 1];
}

static void xl6CgenFreeTryTargets(XL6_CgenContext* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xrtFree(ctx->tryTargets);
    ctx->tryTargets = NULL;
    ctx->tryTargetCount = 0;
    ctx->tryTargetCapacity = 0;
}
