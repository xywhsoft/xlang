typedef struct XL6_CgenPropertyArgTemp {
    const XL6_AstNode* expr;
    const XL6_AstNode* param;
    XL6_AstNode* typeNode;
    char name[64];
} XL6_CgenPropertyArgTemp;

static void xl6CgenPropertyTempDefaultValue(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    if ( xl6CgenIsClassType(ctx, typeNode) || xl6CgenIsNativeBackedClassType(ctx, typeNode) ) {
        xl6OutputPuts("{0}", ctx->out);
        return;
    }
    xl6CgenDeclCallArgTempDefaultValue(ctx, typeNode);
}

static void xl6CgenPropertyArgTempsFree(XL6_CgenPropertyArgTemp* temps, size_t count)
{
    size_t i;

    if ( temps == NULL ) {
        return;
    }
    for ( i = 0; i < count; ++i ) {
        xl6AstFree(temps[i].typeNode);
    }
    xrtFree(temps);
}

static XL6_CgenExprReplacement* xl6CgenPropertyReplacementsNew(
    XL6_CgenContext* ctx,
    const XL6_CgenPropertyArgTemp* temps,
    size_t tempCount,
    const XL6_AstNode* receiver,
    const char* receiverName,
    const XL6_AstNode* valueExpr,
    const char* valueName,
    size_t* outCount)
{
    XL6_CgenExprReplacement* replacements;
    size_t oldCount;
    size_t extraCount;
    size_t count;
    size_t i;

    if ( outCount != NULL ) {
        *outCount = 0;
    }
    oldCount = ctx != NULL ? ctx->exprReplacementCount : 0;
    extraCount = tempCount + (receiver != NULL && receiverName != NULL ? 1u : 0u) +
                 (valueExpr != NULL && valueName != NULL ? 1u : 0u);
    count = oldCount + extraCount;
    replacements = count > 0 ?
        (XL6_CgenExprReplacement*)xrtCalloc(count, sizeof(XL6_CgenExprReplacement)) :
        NULL;
    if ( count > 0 && replacements == NULL ) {
        xl6CgenError(ctx, valueExpr != NULL ? valueExpr : receiver, "out of memory while lowering class property compound assignment");
        return NULL;
    }

    for ( i = 0; i < oldCount; ++i ) {
        replacements[i] = ctx->exprReplacements[i];
    }
    count = oldCount;
    if ( receiver != NULL && receiverName != NULL ) {
        replacements[count].expr = receiver;
        replacements[count].cName = receiverName;
        replacements[count].flags = 0u;
        count += 1;
    }
    for ( i = 0; i < tempCount; ++i ) {
        replacements[count].expr = temps[i].expr;
        replacements[count].cName = temps[i].name;
        replacements[count].flags = 0u;
        count += 1;
    }
    if ( valueExpr != NULL && valueName != NULL ) {
        replacements[count].expr = valueExpr;
        replacements[count].cName = valueName;
        replacements[count].flags = 0u;
        count += 1;
    }
    if ( outCount != NULL ) {
        *outCount = count;
    }
    return replacements;
}

static void xl6CgenPropertyArgTempInit(
    XL6_CgenContext* ctx,
    XL6_CgenPropertyArgTemp* temp)
{
    const char* cType;

    if ( ctx == NULL || temp == NULL || temp->typeNode == NULL || temp->expr == NULL ) {
        return;
    }

    cType = xl6CgenValueStorageCType(ctx, temp->typeNode);
    if ( xl6CgenIsStringTypeNode(ctx, temp->typeNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = xrtCopyStr(", cType, temp->name);
        xl6CgenFixedParamArgExpr(ctx, temp->param, temp->expr);
        xl6OutputPuts(", 0u);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s == 0) { xl6_runtime_fatal(3); }\n", temp->name);
        xl6CgenAddManagedCleanupForType(ctx, temp->name, temp->typeNode);
        return;
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = ", cType, temp->name);
    xl6CgenPropertyTempDefaultValue(ctx, temp->typeNode);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenAddManagedCleanupForType(ctx, temp->name, temp->typeNode);

    if ( !xl6CgenDeclEmitCallArgTemp(ctx, temp->typeNode, cType, temp->name, temp->expr) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", temp->name);
        xl6CgenFixedParamArgExpr(ctx, temp->param, temp->expr);
        xl6OutputPuts(";\n", ctx->out);
        if ( xl6CgenIsFunctionType(temp->typeNode) && xl6CgenExprIsFunctionBorrow(ctx, temp->expr) ) {
            xl6CgenRetainFunctionValue(ctx, temp->name);
        }
        xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, temp->typeNode, temp->name, temp->expr);
    }
}

static int xl6CgenClassPropertyCompoundAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* operation,
    const XL6_AstNode* left,
    const char* op,
    const XL6_AstNode* right)
{
    const XL6_AstNode* member;
    const XL6_AstNode* indexNode;
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* getter;
    const XL6_AstNode* setter;
    const XL6_AstNode* params;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    XL6_AstNode* receiverType;
    XL6_AstNode* setterReceiverType;
    XL6_AstNode* getterCall;
    XL6_AstNode* setterCall;
    XL6_AstNode* propertyType;
    XL6_CgenPropertyArgTemp* argTemps;
    XL6_CgenExprReplacement* replacements;
    const XL6_CgenExprReplacement* savedReplacements;
    size_t savedReplacementCount;
    size_t replacementCount;
    size_t argCount;
    size_t varMark;
    size_t cleanupMark;
    size_t i;
    int id;
    int receiverIsClassHandle;
    char currentName[64];
    char receiverPointerName[64];
    char receiverReplacementName[80];
    const char* receiverReplacement;
    XL6_AstNode fakeName;
    XL6_AstNode fakeAssign;
    XL6_AstNode* fakeChildren[2];

    if ( ctx == NULL || left == NULL || right == NULL || op == NULL ||
         xl6CTextEquals(op, "=") || xl6CTextEquals(op, "?=") ) {
        return 0;
    }
    if ( left->kind == XL6_AST_MEMBER ) {
        member = left;
        indexNode = NULL;
    } else if ( left->kind == XL6_AST_INDEX && left->childCount > 0 &&
                left->children[0] != NULL && left->children[0]->kind == XL6_AST_MEMBER ) {
        member = left->children[0];
        indexNode = left;
    } else {
        return 0;
    }

    receiverType = NULL;
    getterCall = NULL;
    if ( !xl6CgenClassPropertyResolve(
             ctx,
             member,
             indexNode,
             NULL,
             XL6_AST_GET,
             indexNode != NULL ? indexNode : member,
             &classNode,
             &ownerClass,
             &getter,
             &receiverType,
             &getterCall) ) {
        return 0;
    }
    setterReceiverType = NULL;
    setterCall = NULL;
    if ( !xl6CgenClassPropertyResolve(
             ctx,
             member,
             indexNode,
             left,
             XL6_AST_LET,
             operation,
             NULL,
             NULL,
             &setter,
             &setterReceiverType,
             &setterCall) ) {
        xl6CgenError(ctx, left, "class property compound assignment requires matching let");
        xl6CgenProtocolTempCallFree(getterCall);
        xl6AstFree(receiverType);
        return 1;
    }

    propertyType = xl6CgenCloneClassPropertyMemberType(ctx, member, indexNode);
    if ( propertyType == NULL ) {
        xl6CgenError(ctx, left, "cannot determine class property compound assignment type");
        xl6CgenProtocolTempCallFree(setterCall);
        xl6CgenProtocolTempCallFree(getterCall);
        xl6AstFree(setterReceiverType);
        xl6AstFree(receiverType);
        return 1;
    }

    params = xl6CgenCallableParams(getter);
    argCount = indexNode != NULL ? xl6CgenIndexArgCount(indexNode) : 0;
    argTemps = argCount > 0 ?
        (XL6_CgenPropertyArgTemp*)xrtCalloc(argCount, sizeof(XL6_CgenPropertyArgTemp)) :
        NULL;
    if ( argCount > 0 && argTemps == NULL ) {
        xl6CgenError(ctx, left, "out of memory while lowering class property dimension arguments");
        xl6AstFree(propertyType);
        xl6CgenProtocolTempCallFree(setterCall);
        xl6CgenProtocolTempCallFree(getterCall);
        xl6AstFree(setterReceiverType);
        xl6AstFree(receiverType);
        return 1;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( receiverType != NULL && receiverType->childCount > 0 &&
         ownerClass != NULL && xl6CgenClassHasTypeParams(ownerClass) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(ownerClass);
        ctx->currentGenericArgs = receiverType->children[0];
    }
    for ( i = 0; i < argCount; ++i ) {
        const XL6_AstNode* param = params != NULL && i < params->childCount ? params->children[i] : NULL;
        const XL6_AstNode* paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;

        argTemps[i].expr = xl6CgenIndexArgAt(indexNode, i);
        argTemps[i].param = param;
        argTemps[i].typeNode = xl6CgenCloneCanonicalTypeNode(ctx, paramType);
        snprintf(argTemps[i].name, sizeof(argTemps[i].name), "__xl6_property_arg_%d_%u", ctx->tempId++, (unsigned)i);
        if ( argTemps[i].expr == NULL || argTemps[i].typeNode == NULL ) {
            xl6CgenError(ctx, left, "cannot determine class property dimension argument type");
            ctx->currentGenericParams = prevParams;
            ctx->currentGenericArgs = prevArgs;
            xl6CgenPropertyArgTempsFree(argTemps, argCount);
            xl6AstFree(propertyType);
            xl6CgenProtocolTempCallFree(setterCall);
            xl6CgenProtocolTempCallFree(getterCall);
            xl6AstFree(setterReceiverType);
            xl6AstFree(receiverType);
            return 1;
        }
    }
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;

    id = ctx->tempId++;
    snprintf(currentName, sizeof(currentName), "__xl6_property_current_%d", id);
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    receiverReplacement = NULL;
    receiverPointerName[0] = '\0';
    receiverReplacementName[0] = '\0';

    cleanupMark = ctx->cleanupCount;
    varMark = ctx->varCount;
    savedReplacements = ctx->exprReplacements;
    savedReplacementCount = ctx->exprReplacementCount;
    receiverIsClassHandle = xl6CgenClassHandleTargetType(ctx, receiverType) != NULL;

    /* 复杂接收者先保存地址，getter 和 setter 共用同一个对象。 */
    if ( receiver != NULL && (receiver->kind == XL6_AST_MEMBER || receiver->kind == XL6_AST_INDEX) ) {
        snprintf(receiverPointerName, sizeof(receiverPointerName), "__xl6_property_self_%d", id);
        if ( receiverIsClassHandle ) {
            snprintf(receiverReplacementName, sizeof(receiverReplacementName), "%s", receiverPointerName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenValueStorageCType(ctx, receiverType), receiverPointerName);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenAddVarType(ctx, receiverPointerName, receiverType);
            xl6CgenAddManagedCleanupForType(ctx, receiverPointerName, receiverType);
        } else {
            snprintf(receiverReplacementName, sizeof(receiverReplacementName), "(*%s)", receiverPointerName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s* %s = &(", xl6CgenTypeName(ctx, receiverType), receiverPointerName);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(");\n", ctx->out);
        }
        receiverReplacement = receiverReplacementName;
    }

    for ( i = 0; i < argCount; ++i ) {
        xl6CgenPropertyArgTempInit(ctx, &argTemps[i]);
    }

    replacements = xl6CgenPropertyReplacementsNew(
        ctx,
        argTemps,
        argCount,
        receiverReplacement != NULL ? receiver : NULL,
        receiverReplacement,
        NULL,
        NULL,
        &replacementCount);
    if ( replacements == NULL &&
         (savedReplacementCount + argCount + (receiverReplacement != NULL ? 1u : 0u)) > 0 ) {
        xl6CgenEmitCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        xl6CgenPropertyArgTempsFree(argTemps, argCount);
        xl6AstFree(propertyType);
        xl6CgenProtocolTempCallFree(setterCall);
        xl6CgenProtocolTempCallFree(getterCall);
        xl6AstFree(setterReceiverType);
        xl6AstFree(receiverType);
        return 1;
    }
    ctx->exprReplacements = replacements;
    ctx->exprReplacementCount = replacementCount;

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenValueStorageCType(ctx, propertyType), currentName);
    if ( xl6CgenIsStringTypeNode(ctx, propertyType) ) {
        xl6OutputPuts("xrtCopyStr(", ctx->out);
        xl6CgenClassPropertyEmitCall(ctx, member, indexNode, NULL, XL6_AST_GET, indexNode != NULL ? indexNode : member);
        xl6OutputPuts(", 0u)", ctx->out);
    } else {
        xl6CgenPropertyTempDefaultValue(ctx, propertyType);
    }
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenAddVarType(ctx, currentName, propertyType);
    xl6CgenAddManagedCleanupForType(ctx, currentName, propertyType);
    if ( xl6CgenIsStringTypeNode(ctx, propertyType) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s == 0) { xl6_runtime_fatal(3); }\n", currentName);
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", currentName);
        xl6CgenClassPropertyEmitCall(ctx, member, indexNode, NULL, XL6_AST_GET, indexNode != NULL ? indexNode : member);
        xl6OutputPuts(";\n", ctx->out);
    }

    memset(&fakeName, 0, sizeof(fakeName));
    memset(&fakeAssign, 0, sizeof(fakeAssign));
    fakeName.kind = XL6_AST_NAME;
    fakeName.text = currentName;
    fakeName.line = left->line;
    fakeName.column = left->column;
    fakeChildren[0] = &fakeName;
    fakeChildren[1] = (XL6_AstNode*)right;
    fakeAssign.kind = XL6_AST_ASSIGN;
    fakeAssign.text = (char*)op;
    fakeAssign.line = left->line;
    fakeAssign.column = left->column;
    fakeAssign.children = fakeChildren;
    fakeAssign.childCount = 2;
    fakeAssign.childCapacity = 2;
    /*
        该临时赋值由属性复合赋值 lowering 构造，不属于源 AST/HIR。
        左侧一定是本函数刚声明的局部临时量，因此显式携带左值计划。
    */
    xl6CgenAssignStmtPlanned(
        ctx,
        &fakeAssign,
        XL6_SEM_LVALUE_LOCAL);

    ctx->exprReplacements = savedReplacements;
    ctx->exprReplacementCount = savedReplacementCount;
    xrtFree(replacements);
    replacements = xl6CgenPropertyReplacementsNew(
        ctx,
        argTemps,
        argCount,
        receiverReplacement != NULL ? receiver : NULL,
        receiverReplacement,
        left,
        currentName,
        &replacementCount);
    if ( replacements == NULL ) {
        xl6CgenError(ctx, left, "out of memory while lowering class property setter value");
    } else {
        ctx->exprReplacements = replacements;
        ctx->exprReplacementCount = replacementCount;
        xl6CgenIndent(ctx);
        xl6CgenClassPropertyEmitCall(ctx, member, indexNode, left, XL6_AST_LET, operation);
        xl6OutputPuts(";\n", ctx->out);
    }

    ctx->exprReplacements = savedReplacements;
    ctx->exprReplacementCount = savedReplacementCount;
    xrtFree(replacements);
    xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    xl6CgenFreeVarsSince(ctx, varMark);

    xl6CgenPropertyArgTempsFree(argTemps, argCount);
    xl6AstFree(propertyType);
    xl6CgenProtocolTempCallFree(setterCall);
    xl6CgenProtocolTempCallFree(getterCall);
    xl6AstFree(setterReceiverType);
    xl6AstFree(receiverType);
    (void)classNode;
    (void)setter;
    return 1;
}
