static void xl6CgenStmt(XL6_CgenContext* ctx, const XL6_AstNode* node);

static int xl6CgenDeclTryXValueArrayLiteralInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);
static int xl6CgenDeclTryXValueNullCoalesceInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);
static int xl6CgenDeclTryXValueBinaryInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);
static int xl6CgenDeclTryXValueConditionalInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);
static int xl6CgenDeclTryXValueExprInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);
static void xl6CgenDeclEmitXValueExprInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);
static void xl6CgenDeclEmitXValueExprAssign(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);

static void xl6CgenBlock(XL6_CgenContext* ctx, const XL6_AstNode* block)
{
    size_t i;
    size_t cleanupMark;

    if ( block == NULL ) {
        return;
    }
    cleanupMark = ctx->cleanupCount;
    for ( i = 0; i < block->childCount; ++i ) {
        xl6CgenStmt(ctx, block->children[i]);
    }
    xl6CgenCloseScope(ctx, cleanupMark);
}

static int xl6CgenDeclTemporaryClassFieldTypeIsPlain(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( concreteType == NULL ) {
        return 0;
    }

    /*
        当前只接管普通按值字段。function、class、custom、var 和容器字段涉及
        借用/保留语义，必须由后续专门路径处理，避免从临时 class 中借出悬空值。
    */
    if ( xl6CgenIsXValueType(concreteType) ||
         xl6CgenIsFunctionType(concreteType) ||
         xl6CgenIsClassType(ctx, concreteType) ||
         xl6CgenIsCustomType(ctx, concreteType) ||
         xl6CgenIsTypedContainerType(ctx, concreteType) ||
         xl6CgenIsFutureLikeType(concreteType) ||
         xl6CgenIsChannelType(concreteType) ||
         xl6CgenIsThreadType(concreteType) ||
         xl6CgenIsGeneratorType(concreteType) ||
         xl6CgenIsSyncHandleType(concreteType) ) {
        return 0;
    }

    return 1;
}

static void xl6CgenDeclDestroyTemporaryClassValue(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* classTypeNode, const char* tempName)
{
    if ( classNode == NULL || tempName == NULL || !xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        return;
    }

    xl6CgenIndent(ctx);
    if ( classTypeNode != NULL && classTypeNode->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        xl6CgenGenericClassMemberSymbol(ctx, classNode, classTypeNode, "destroy");
    } else {
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
    }
    xl6OutputPrintf(ctx->out, "(&%s);\n", tempName);
}

typedef struct XL6_CgenDeclClassArgTemp {
    int active;
    const XL6_AstNode* expr;
    XL6_AstNode* valueType;
    XL6_AstNode* functionType;
    const XL6_AstNode* classNode;
    char valueName[64];
    char functionName[64];
} XL6_CgenDeclClassArgTemp;

static void xl6CgenDeclFreeClassArgTemps(XL6_CgenDeclClassArgTemp* temps, int count)
{
    int i;

    if ( temps == NULL ) {
        return;
    }
    for ( i = 0; i < count; ++i ) {
        xl6AstFree(temps[i].valueType);
        xl6AstFree(temps[i].functionType);
    }
    xrtFree(temps);
}

static int xl6CgenDeclPrepareClassArgTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* paramType,
    const XL6_AstNode* expr,
    int id,
    XL6_CgenDeclClassArgTemp* temp)
{
    const XL6_AstNode* returns;
    const XL6_AstNode* returnType;
    XL6_AstNode* functionType;
    XL6_AstNode* paramCanonical;
    XL6_AstNode* returnCanonical;
    const XL6_AstNode* classNode;
    int matches;

    if ( temp == NULL ) {
        return 0;
    }
    temp->active = 0;
    temp->expr = NULL;
    temp->valueType = NULL;
    temp->functionType = NULL;
    temp->classNode = NULL;
    temp->valueName[0] = '\0';
    temp->functionName[0] = '\0';

    functionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, expr);
    if ( functionType == NULL ) {
        return 0;
    }

    returns = xl6CgenFunctionReturnTypes(functionType);
    returnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    paramCanonical = xl6CgenCloneCanonicalTypeNode(ctx, paramType);
    returnCanonical = xl6CgenCloneCanonicalTypeNode(ctx, returnType);
    classNode = xl6CgenClassNodeForType(ctx, returnCanonical);
    matches = paramCanonical != NULL &&
              returnCanonical != NULL &&
              classNode != NULL &&
              xl6CgenTypeNodeEqual(paramCanonical, returnCanonical);
    xl6AstFree(paramCanonical);
    if ( !matches ) {
        xl6AstFree(returnCanonical);
        xl6AstFree(functionType);
        return 0;
    }

    temp->active = 1;
    temp->expr = expr;
    temp->valueType = returnCanonical;
    temp->functionType = functionType;
    temp->classNode = classNode;
    snprintf(temp->valueName, sizeof(temp->valueName), "__xl6_class_arg_%d", id);
    snprintf(temp->functionName, sizeof(temp->functionName), "__xl6_member_fn_stmt_%d", id);
    return 1;
}

static int xl6CgenDeclAppendClassArgTemp(XL6_CgenContext* ctx,
                                         XL6_CgenDeclClassArgTemp** temps,
                                         int* count,
                                         int* capacity,
                                         const XL6_AstNode* paramType,
                                         const XL6_AstNode* argExpr)
{
    XL6_CgenDeclClassArgTemp temp;
    XL6_CgenDeclClassArgTemp* newItems;
    int newCapacity;

    if ( temps == NULL || count == NULL || capacity == NULL || argExpr == NULL ) {
        return 0;
    }
    if ( !xl6CgenDeclPrepareClassArgTemp(ctx, paramType, argExpr, ctx->tempId++, &temp) ) {
        return 0;
    }

    if ( *count >= *capacity ) {
        newCapacity = *capacity > 0 ? (*capacity * 2) : 4;
        newItems = (XL6_CgenDeclClassArgTemp*)xrtRealloc(*temps, (size_t)newCapacity * sizeof(XL6_CgenDeclClassArgTemp));
        if ( newItems == NULL ) {
            xl6AstFree(temp.valueType);
            xl6AstFree(temp.functionType);
            xl6CgenError(ctx, argExpr, "out of memory while preparing class argument temporaries");
            return 0;
        }
        *temps = newItems;
        *capacity = newCapacity;
    }
    (*temps)[*count] = temp;
    *count += 1;
    return 1;
}

static void xl6CgenDeclCollectClassArgTempsInExpr(XL6_CgenContext* ctx,
                                                  const XL6_AstNode* expr,
                                                  XL6_CgenDeclClassArgTemp** temps,
                                                  int* count,
                                                  int* capacity)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    const XL6_AstNode* params;
    XL6_AstNode* inferredArgs;
    XL6_AstNode* func;
    int fixedCount;
    int i;

    if ( ctx == NULL || expr == NULL ) {
        return;
    }

    if ( expr->kind == XL6_AST_CALL && expr->childCount > 0 ) {
        callee = expr->children[0];
        if ( callee != NULL && callee->kind == XL6_AST_NAME ) {
            inferredArgs = NULL;
            func = xl6CgenResolveCallFuncInternal(ctx, expr, callee->text, &inferredArgs, 0);
            if ( inferredArgs == NULL && func != NULL ) {
                params = func->childCount > 1 ? func->children[1] : NULL;
                fixedCount = xl6CgenFixedParamCount(params);
                if ( params == NULL || params->childCount == (size_t)fixedCount ) {
                    args = expr->childCount > 1 ? expr->children[1] : NULL;
                    for ( i = 0; i < fixedCount; ++i ) {
                        const XL6_AstNode* param = params->children[i];
                        const XL6_AstNode* paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
                        const XL6_AstNode* argExpr = param != NULL ? xl6CgenFindArgForParam(args, param->text, i) : NULL;
                        xl6CgenDeclAppendClassArgTemp(ctx, temps, count, capacity, paramType, argExpr);
                    }
                }
            }
            xl6AstFree(inferredArgs);
        }
    }

    for ( i = 0; i < (int)expr->childCount; ++i ) {
        xl6CgenDeclCollectClassArgTempsInExpr(ctx, expr->children[i], temps, count, capacity);
    }
}

static void xl6CgenDeclEmitClassArgTemps(XL6_CgenContext* ctx,
                                         XL6_CgenDeclClassArgTemp* temps,
                                         int count,
                                         int firstLineAlreadyIndented)
{
    int i;
    int firstOutput;

    firstOutput = 1;
    for ( i = 0; i < count; ++i ) {
        if ( !temps[i].active ) {
            continue;
        }
        if ( firstOutput ) {
            if ( !firstLineAlreadyIndented ) {
                xl6CgenIndent(ctx);
            }
            firstOutput = 0;
        } else {
            xl6CgenIndent(ctx);
        }
        xl6OutputPrintf(ctx->out, "%s %s = {0};\n", xl6CgenTypeName(ctx, temps[i].valueType), temps[i].valueName);
        xl6CgenEmitMemberFunctionStatementTemp(ctx, temps[i].expr, temps[i].functionName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", temps[i].valueName);
        xl6CgenMemberFunctionStatementInvokeExpr(ctx, temps[i].expr, temps[i].functionType, temps[i].functionName);
        xl6OutputPuts(";\n", ctx->out);
    }
}

static void xl6CgenDeclDestroyClassArgTemps(XL6_CgenContext* ctx, XL6_CgenDeclClassArgTemp* temps, int count)
{
    int i;

    for ( i = count - 1; i >= 0; --i ) {
        if ( temps[i].active ) {
            xl6CgenDeclDestroyTemporaryClassValue(ctx, temps[i].classNode, temps[i].valueType, temps[i].valueName);
        }
    }
}

static XL6_CgenExprReplacement* xl6CgenDeclCreateExprReplacements(XL6_CgenContext* ctx, XL6_CgenDeclClassArgTemp* temps, int count)
{
    XL6_CgenExprReplacement* replacements;
    int i;

    if ( count <= 0 ) {
        return NULL;
    }
    replacements = (XL6_CgenExprReplacement*)xrtMalloc((size_t)count * sizeof(XL6_CgenExprReplacement));
    if ( replacements == NULL ) {
        xl6CgenError(ctx, NULL, "out of memory while preparing expression replacements");
        return NULL;
    }
    for ( i = 0; i < count; ++i ) {
        replacements[i].expr = temps[i].expr;
        replacements[i].cName = temps[i].valueName;
    }
    return replacements;
}

typedef struct XL6_CgenDeclCallArgTemp {
    int active;
    const XL6_AstNode* expr;
    XL6_AstNode* typeNode;
    char name[64];
} XL6_CgenDeclCallArgTemp;

static void xl6CgenDeclFreeCallArgTemps(XL6_CgenDeclCallArgTemp* temps, int count)
{
    int i;

    if ( temps == NULL ) {
        return;
    }
    for ( i = 0; i < count; ++i ) {
        xl6AstFree(temps[i].typeNode);
    }
    xrtFree(temps);
}

static XL6_CgenExprReplacement* xl6CgenDeclCreateCallArgReplacements(XL6_CgenContext* ctx, XL6_CgenDeclCallArgTemp* temps, int count, int activeCount)
{
    XL6_CgenExprReplacement* replacements;
    int i;
    int replacementIndex;

    if ( activeCount <= 0 ) {
        return NULL;
    }
    replacements = (XL6_CgenExprReplacement*)xrtMalloc((size_t)activeCount * sizeof(XL6_CgenExprReplacement));
    if ( replacements == NULL ) {
        xl6CgenError(ctx, NULL, "out of memory while preparing call argument replacements");
        return NULL;
    }

    replacementIndex = 0;
    for ( i = 0; i < count; ++i ) {
        if ( !temps[i].active ) {
            continue;
        }
        replacements[replacementIndex].expr = temps[i].expr;
        replacements[replacementIndex].cName = temps[i].name;
        replacementIndex += 1;
    }
    return replacements;
}

static void xl6CgenDeclEmitKnownCallArgsWithClassTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* func,
    const XL6_AstNode* args,
    const XL6_CgenDeclClassArgTemp* temps,
    int fixedCount)
{
    const XL6_AstNode* params;
    int i;

    params = func != NULL && func->childCount > 1 ? func->children[1] : NULL;
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params != NULL ? params->children[i] : NULL;
        const XL6_AstNode* expr = param != NULL ? xl6CgenFindArgForParam(args, param->text, i) : NULL;

        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        if ( temps != NULL && temps[i].active ) {
            xl6OutputPuts(temps[i].valueName, ctx->out);
        } else if ( expr != NULL ) {
            xl6CgenExpr(ctx, expr);
        } else if ( param != NULL && param->childCount > 1 ) {
            xl6CgenExpr(ctx, param->children[1]);
        } else {
            xl6CgenError(ctx, param, "missing required argument for parameter");
            xl6OutputPuts("0", ctx->out);
        }
    }
}

static void xl6CgenDeclEmitKnownStatementCallSymbol(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    if ( func == NULL ) {
        xl6OutputPuts("/*missing_call*/", ctx->out);
        return;
    }
    if ( func->kind == XL6_AST_EXTERN_FUNC ) {
        xl6CgenCStringSymbol(ctx->out, func->text);
    } else if ( func->kind == XL6_AST_EXPORT_FUNC ) {
        xl6CgenExportInternalSymbol(ctx->out, func);
    } else {
        xl6CgenFuncSymbol(ctx, func);
    }
}

static int xl6CgenDeclTryTemporaryClassArgInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    const XL6_AstNode* params;
    XL6_AstNode* inferredArgs;
    XL6_AstNode* func;
    XL6_CgenDeclClassArgTemp* temps;
    int fixedCount;
    int i;
    int tempCount;
    int firstOutput;

    if ( ctx == NULL || node == NULL || typeNode == NULL || cType == NULL || cName == NULL ||
         initExpr == NULL || initExpr->kind != XL6_AST_CALL || initExpr->childCount == 0 ||
         !xl6CgenDeclTemporaryClassFieldTypeIsPlain(ctx, typeNode) ) {
        return 0;
    }

    callee = initExpr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }

    inferredArgs = NULL;
    func = xl6CgenResolveCallFuncInternal(ctx, initExpr, callee->text, &inferredArgs, 0);
    if ( inferredArgs != NULL ) {
        xl6AstFree(inferredArgs);
        return 0;
    }
    if ( func == NULL ) {
        return 0;
    }

    params = func->childCount > 1 ? func->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    if ( params != NULL && params->childCount > (size_t)fixedCount ) {
        return 0;
    }
    if ( fixedCount <= 0 ) {
        return 0;
    }

    temps = (XL6_CgenDeclClassArgTemp*)xrtMalloc((size_t)fixedCount * sizeof(XL6_CgenDeclClassArgTemp));
    if ( temps == NULL ) {
        xl6CgenError(ctx, initExpr, "out of memory while preparing class argument temporaries");
        return 0;
    }
    memset(temps, 0, (size_t)fixedCount * sizeof(XL6_CgenDeclClassArgTemp));

    args = initExpr->childCount > 1 ? initExpr->children[1] : NULL;
    tempCount = 0;
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params != NULL ? params->children[i] : NULL;
        const XL6_AstNode* expr = param != NULL ? xl6CgenFindArgForParam(args, param->text, i) : NULL;
        const XL6_AstNode* paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;

        if ( expr != NULL &&
             xl6CgenDeclPrepareClassArgTemp(ctx, paramType, expr, ctx->tempId++, &temps[i]) ) {
            tempCount += 1;
        }
    }

    if ( tempCount == 0 ) {
        xl6CgenDeclFreeClassArgTemps(temps, fixedCount);
        return 0;
    }

    firstOutput = 1;
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params != NULL ? params->children[i] : NULL;
        const XL6_AstNode* expr = param != NULL ? xl6CgenFindArgForParam(args, param->text, i) : NULL;

        if ( !temps[i].active ) {
            continue;
        }
        if ( !firstOutput ) {
            xl6CgenIndent(ctx);
        }
        firstOutput = 0;
        xl6OutputPrintf(ctx->out, "%s %s = {0};\n", xl6CgenTypeName(ctx, temps[i].valueType), temps[i].valueName);
        xl6CgenEmitMemberFunctionStatementTemp(ctx, expr, temps[i].functionName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", temps[i].valueName);
        xl6CgenMemberFunctionStatementInvokeExpr(ctx, expr, temps[i].functionType, temps[i].functionName);
        xl6OutputPuts(";\n", ctx->out);
    }

    xl6CgenAddVarType(ctx, node->text, typeNode);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = ", cType, cName);
    xl6CgenFuncSymbol(ctx, func);
    xl6OutputPutc('(', ctx->out);
    xl6CgenDeclEmitKnownCallArgsWithClassTemps(ctx, func, args, temps, fixedCount);
    xl6OutputPuts(");\n", ctx->out);

    for ( i = fixedCount - 1; i >= 0; --i ) {
        if ( temps[i].active ) {
            xl6CgenDeclDestroyTemporaryClassValue(ctx, temps[i].classNode, temps[i].valueType, temps[i].valueName);
        }
    }

    xl6CgenDeclFreeClassArgTemps(temps, fixedCount);
    return 1;
}

static int xl6CgenDeclIsNullCoalesceExpr(const XL6_AstNode* expr)
{
    return expr != NULL && expr->kind == XL6_AST_BINARY && expr->childCount >= 2 &&
           xl6CTextEquals(expr->text, "??");
}

static int xl6CgenDeclCanLowerStaticNullCoalesceType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( concreteType == NULL ) {
        return 0;
    }

    /*
        ?? 的语句级展开会接管变量初始化与赋值时机。
        当前只处理普通标量/指针类型，复杂托管类型继续走原有专门路径。
    */
    if ( xl6CgenIsXValueType(concreteType) ||
         xl6CgenIsFunctionType(concreteType) ||
         xl6CgenIsClassType(ctx, concreteType) ||
         xl6CgenIsCustomType(ctx, concreteType) ||
         xl6CgenIsTypedContainerType(ctx, concreteType) ||
         xl6CgenIsFutureLikeType(concreteType) ||
         xl6CgenIsChannelType(concreteType) ||
         xl6CgenIsThreadType(concreteType) ||
         xl6CgenIsGeneratorType(concreteType) ||
         xl6CgenIsSyncHandleType(concreteType) ) {
        return 0;
    }

    return 1;
}

static int xl6CgenDeclTryXValueNullCoalesceInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    XL6_AstNode* leftType;
    int id;

    if ( ctx == NULL || typeNode == NULL || cName == NULL ||
         !xl6CgenDeclIsNullCoalesceExpr(initExpr) ) {
        return 0;
    }

    left = initExpr->children[0];
    right = initExpr->children[1];
    if ( left == NULL || left->kind == XL6_AST_NULL ) {
        return 0;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, left);
    if ( !xl6CgenIsXValueType(leftType) ) {
        xl6AstFree(leftType);
        return 0;
    }
    xl6AstFree(leftType);

    id = ctx->tempId++;
    xl6OutputPrintf(ctx->out, "xvalue %s = 0;\n", cName);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    {
        char leftName[64];

        snprintf(leftName, sizeof(leftName), "__xl6_nc_%d", id);
        xl6CgenDeclEmitXValueExprInit(ctx, typeNode, leftName, left);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (xvoIsNull(__xl6_nc_%d)) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_nc_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_nc_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenDeclEmitXValueExprAssign(ctx, typeNode, cName, right);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = __xl6_nc_%d;\n", cName, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenDeclTryXValueBinaryInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    int id;

    if ( ctx == NULL || typeNode == NULL || cName == NULL ||
         initExpr == NULL || initExpr->kind != XL6_AST_BINARY || initExpr->childCount < 2 ||
         !xl6CgenBinaryArithmeticIsXValue(ctx, initExpr) ) {
        return 0;
    }

    left = initExpr->children[0];
    right = initExpr->children[1];
    id = ctx->tempId++;

    xl6OutputPrintf(ctx->out, "xvalue %s = 0;\n", cName);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    {
        char leftName[64];
        char rightName[64];

        snprintf(leftName, sizeof(leftName), "__xl6_bin_left_%d", id);
        snprintf(rightName, sizeof(rightName), "__xl6_bin_right_%d", id);
        xl6CgenDeclEmitXValueExprInit(ctx, typeNode, leftName, left);
        xl6CgenDeclEmitXValueExprInit(ctx, typeNode, rightName, right);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
                    "%s = xl6_xrt_basic_binary(__xl6_bin_left_%d, __xl6_bin_right_%d, %d);\n",
                    cName,
                    id,
                    id,
                    xl6CgenXValueBinaryMode(initExpr->text));
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_bin_left_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_bin_left_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_bin_right_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_bin_right_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenDeclTryXValueConditionalInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* cond;
    const XL6_AstNode* thenExpr;
    const XL6_AstNode* elseExpr;

    if ( ctx == NULL || typeNode == NULL || cName == NULL ||
         initExpr == NULL || initExpr->kind != XL6_AST_CONDITIONAL || initExpr->childCount < 3 ||
         !xl6CgenConditionalIsXValue(ctx, initExpr) ) {
        return 0;
    }

    cond = initExpr->children[0];
    thenExpr = initExpr->children[1];
    elseExpr = initExpr->children[2];

    xl6OutputPrintf(ctx->out, "xvalue %s = 0;\n", cName);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("if (", ctx->out);
    xl6CgenConditionExpr(ctx, cond);
    xl6OutputPuts(") {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenDeclEmitXValueExprAssign(ctx, typeNode, cName, thenExpr);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenDeclEmitXValueExprAssign(ctx, typeNode, cName, elseExpr);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenDeclStaticBoolLikeType(const XL6_AstNode* typeNode)
{
    const char* name;

    if ( typeNode == NULL || typeNode->text == NULL ) {
        return 0;
    }

    name = typeNode->text;
    return xl6CgenIsBoolTypeName(name) ||
           xl6CTextEquals(name, "int") ||
           xl6CTextEquals(name, "int8") ||
           xl6CTextEquals(name, "uint8") ||
           xl6CTextEquals(name, "int16") ||
           xl6CTextEquals(name, "uint16") ||
           xl6CTextEquals(name, "int32") ||
           xl6CTextEquals(name, "uint32") ||
           xl6CTextEquals(name, "int64") ||
           xl6CTextEquals(name, "uint64");
}

static int xl6CgenDeclTryStaticXValueCompareInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    int id;
    int equalMode;
    int negate;

    if ( ctx == NULL || typeNode == NULL || cType == NULL || cName == NULL ||
         initExpr == NULL || initExpr->kind != XL6_AST_BINARY || initExpr->childCount < 2 ||
         !xl6CgenDeclStaticBoolLikeType(typeNode) ||
         !xl6CgenBinaryCompareIsXValue(ctx, initExpr) ) {
        return 0;
    }

    left = initExpr->children[0];
    right = initExpr->children[1];
    id = ctx->tempId++;
    equalMode = xl6CTextEquals(initExpr->text, "==") || xl6CTextEquals(initExpr->text, "!=");
    negate = xl6CTextEquals(initExpr->text, "!=");

    xl6OutputPrintf(ctx->out, "%s %s = 0;\n", cType, cName);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_cmp_left_%d = ", id);
    xl6CgenXValueFromExpr(ctx, left);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_cmp_right_%d = ", id);
    xl6CgenXValueFromExpr(ctx, right);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    if ( equalMode ) {
        xl6OutputPrintf(ctx->out,
                        "int __xl6_cmp_result_%d = xl6_xrt_basic_equal(__xl6_cmp_left_%d, __xl6_cmp_right_%d);\n",
                        id,
                        id,
                        id);
    } else {
        xl6OutputPrintf(ctx->out,
                        "int __xl6_cmp_result_%d = xl6_xrt_basic_order(__xl6_cmp_left_%d, __xl6_cmp_right_%d, %d);\n",
                        id,
                        id,
                        id,
                        xl6CgenXValueOrderMode(initExpr->text));
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_cmp_left_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_cmp_left_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_cmp_right_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_cmp_right_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    if ( negate ) {
        xl6OutputPrintf(ctx->out, "%s = (%s)(!__xl6_cmp_result_%d);\n", cName, cType, id);
    } else {
        xl6OutputPrintf(ctx->out, "%s = (%s)(__xl6_cmp_result_%d);\n", cName, cType, id);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenDeclTryStaticXValueApproxInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    int id;

    if ( ctx == NULL || typeNode == NULL || cType == NULL || cName == NULL ||
         initExpr == NULL || initExpr->kind != XL6_AST_BINARY || initExpr->childCount < 2 ||
         !xl6CgenDeclStaticBoolLikeType(typeNode) ||
         !xl6CgenBinaryApproxIsXValue(ctx, initExpr) ) {
        return 0;
    }

    left = initExpr->children[0];
    right = initExpr->children[1];
    id = ctx->tempId++;

    xl6OutputPrintf(ctx->out, "%s %s = 0;\n", cType, cName);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_approx_left_%d = ", id);
    xl6CgenXValueFromExpr(ctx, left);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_approx_right_%d = ", id);
    xl6CgenXValueFromExpr(ctx, right);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
                    "int __xl6_approx_result_%d = xl6_xrt_basic_approx(__xl6_approx_left_%d, __xl6_approx_right_%d);\n",
                    id,
                    id,
                    id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_approx_left_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_approx_left_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_approx_right_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_approx_right_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = (%s)(__xl6_approx_result_%d);\n", cName, cType, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenDeclTryStaticNullCoalesceInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    int id;

    if ( ctx == NULL || typeNode == NULL || cType == NULL || cName == NULL ||
         !xl6CgenDeclIsNullCoalesceExpr(initExpr) ||
         !xl6CgenDeclCanLowerStaticNullCoalesceType(ctx, typeNode) ) {
        return 0;
    }

    left = initExpr->children[0];
    right = initExpr->children[1];
    if ( left == NULL || right == NULL || left->kind == XL6_AST_NULL ) {
        return 0;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, left);
    rightType = xl6CgenCloneExprTypeForInference(ctx, right);
    if ( leftType == NULL || rightType == NULL || !xl6CgenTypeNodeEqual(leftType, rightType) ) {
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return 0;
    }
    xl6AstFree(leftType);
    xl6AstFree(rightType);

    id = ctx->tempId++;
    xl6OutputPrintf(ctx->out, "%s %s = ", cType, cName);
    xl6CgenDefaultValue(ctx, typeNode);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s __xl6_nc_%d = ", cType, id);
    xl6CgenExpr(ctx, left);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_nc_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = __xl6_nc_%d;\n", cName, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", cName);
    xl6CgenExpr(ctx, right);
    xl6OutputPuts(";\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static void xl6CgenDiscardTemporaryResultByType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* tempName)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    if ( ctx == NULL || typeNode == NULL || tempName == NULL || tempName[0] == '\0' ) {
        return;
    }

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( concreteType == NULL || typeName == NULL ) {
        return;
    }

    if ( xl6CgenIsTypedContainerType(ctx, concreteType) ) {
        const XL6_AstNode* containerType = xl6CgenTypedContainerConcreteType(ctx, concreteType);
        const char* containerName = containerType != NULL ? containerType->text : NULL;

        xl6CgenIndent(ctx);
        if ( xl6CTextEquals(containerName, "array") ) {
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtTypedArrayDestroy(%s); %s = 0; }\n", tempName, tempName, tempName);
        } else if ( xl6CTextEquals(containerName, "list") ) {
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtTypedListDestroy(%s); %s = 0; }\n", tempName, tempName, tempName);
        } else if ( xl6CTextEquals(containerName, "set") ) {
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtTypedSetDestroy(%s); %s = 0; }\n", tempName, tempName, tempName);
        } else if ( xl6CTextEquals(containerName, "dict") ) {
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtTypedDictDestroy(%s); %s = 0; }\n", tempName, tempName, tempName);
        }
        return;
    }

    if ( xl6CgenIsXValueType(concreteType) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xvoUnref(%s); %s = 0; }\n", tempName, tempName, tempName);
        return;
    }

    if ( xl6CgenIsFunctionType(concreteType) ) {
        xl6CgenReleaseFunctionPath(ctx, tempName);
        return;
    }

    if ( xl6CgenIsCustomType(ctx, concreteType) ) {
        const XL6_AstNode* customNode = xl6CgenFindCustom(ctx, typeName);
        const XL6_AstNode* destroyNode = customNode != NULL ? xl6CgenFindCustomSpecial(customNode, "destroy") : NULL;

        if ( destroyNode != NULL ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s.handle != 0) { ", tempName);
            xl6CgenCStringSymbol(ctx->out, destroyNode->text);
            xl6OutputPrintf(ctx->out, "(%s.handle); %s.handle = 0; }\n", tempName, tempName);
        }
        return;
    }

    if ( xl6CgenIsClassType(ctx, concreteType) ) {
        XL6_AstNode* classNode = xl6CgenClassNodeForType(ctx, concreteType);

        xl6CgenDeclDestroyTemporaryClassValue(ctx, classNode, concreteType, tempName);
        return;
    }
}

static void xl6CgenDiscardTemporaryMultiResultByTypes(XL6_CgenContext* ctx, const XL6_AstNode* returns, const char* tempName)
{
    size_t i;

    if ( ctx == NULL || returns == NULL || tempName == NULL || tempName[0] == '\0' ) {
        return;
    }

    for ( i = 0; i < returns->childCount; ++i ) {
        char valuePath[128];

        snprintf(valuePath, sizeof(valuePath), "%s.value%u", tempName, (unsigned)i);
        xl6CgenDiscardTemporaryResultByType(ctx, returns->children[i], valuePath);
    }
}

static int xl6CgenTryMemberFunctionStatementInvoke(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* returns;
    const XL6_AstNode* returnType;
    XL6_AstNode* functionType;
    char tempName[64];
    char resultName[64];
    int id;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }
    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER ) {
        return 0;
    }

    functionType = xl6CgenCloneMemberFieldTypeForInference(ctx, callee);
    returns = xl6CgenFunctionReturnTypes(functionType);
    if ( !xl6CgenIsFunctionType(functionType) ||
         returns == NULL ||
         xl6CgenFunctionTypeSupportsErasedCall(ctx, functionType) ) {
        xl6AstFree(functionType);
        return 0;
    }

    id = ctx->tempId++;
    snprintf(tempName, sizeof(tempName), "__xl6_member_fn_stmt_%d", id);
    xl6CgenEmitMemberFunctionStatementTemp(ctx, expr, tempName);
    xl6CgenIndent(ctx);
    if ( returns->childCount == 0 ) {
        xl6CgenMemberFunctionStatementInvokeExpr(ctx, expr, functionType, tempName);
        xl6OutputPuts(";\n", ctx->out);
    } else if ( returns->childCount == 1 ) {
        returnType = returns->children[0];
        snprintf(resultName, sizeof(resultName), "__xl6_member_result_%d", id);
        xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypeName(ctx, returnType), resultName);
        xl6CgenMemberFunctionStatementInvokeExpr(ctx, expr, functionType, tempName);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenDiscardTemporaryResultByType(ctx, returnType, resultName);
    } else {
        snprintf(resultName, sizeof(resultName), "__xl6_member_result_%d", id);
        xl6CgenFunctionReturnStructName(ctx,
                                        xl6CgenFunctionParamTypes(functionType),
                                        returns);
        xl6OutputPrintf(ctx->out, " %s = ", resultName);
        xl6CgenMemberFunctionStatementInvokeExpr(ctx, expr, functionType, tempName);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenDiscardTemporaryMultiResultByTypes(ctx, returns, resultName);
    }
    xl6AstFree(functionType);
    return 1;
}

static int xl6CgenDeclTryTemporaryClassFieldInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode* fieldType;
    XL6_AstNode* classNode;
    XL6_AstNode* memberFunctionType;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    int id;
    char receiverTempName[64];
    char memberTempName[64];

    if ( ctx == NULL || node == NULL || typeNode == NULL || cType == NULL || cName == NULL ||
         initExpr == NULL || initExpr->kind != XL6_AST_MEMBER || initExpr->childCount == 0 ) {
        return 0;
    }

    receiver = initExpr->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_CALL ) {
        return 0;
    }

    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    classNode = xl6CgenClassNodeForType(ctx, receiverType);
    if ( classNode == NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }

    fieldType = xl6CgenCloneMemberFieldTypeForInference(ctx, initExpr);
    if ( fieldType == NULL || !xl6CgenDeclTemporaryClassFieldTypeIsPlain(ctx, fieldType) ) {
        xl6AstFree(fieldType);
        xl6AstFree(receiverType);
        return 0;
    }

    id = ctx->tempId++;
    snprintf(receiverTempName, sizeof(receiverTempName), "__xl6_class_field_recv_%d", id);
    snprintf(memberTempName, sizeof(memberTempName), "__xl6_member_fn_stmt_%d", id);

    xl6OutputPrintf(ctx->out, "%s %s = {0};\n", xl6CgenTypeName(ctx, receiverType), receiverTempName);

    memberFunctionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, receiver);
    if ( memberFunctionType != NULL ) {
        xl6CgenEmitMemberFunctionStatementTemp(ctx, receiver, memberTempName);
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", receiverTempName);
    if ( memberFunctionType != NULL ) {
        xl6CgenMemberFunctionStatementInvokeExpr(ctx, receiver, memberFunctionType, memberTempName);
    } else {
        xl6CgenExpr(ctx, receiver);
    }
    xl6OutputPuts(";\n", ctx->out);

    xl6CgenAddVarType(ctx, node->text, typeNode);
    xl6CgenAddFunctionCleanup(ctx, node->text, typeNode);
    xl6CgenAddFutureCleanup(ctx, node->text, typeNode);
    xl6CgenAddChannelCleanup(ctx, node->text, typeNode);
    xl6CgenAddThreadCleanup(ctx, node->text, typeNode);
    xl6CgenAddGeneratorCleanup(ctx, node->text, typeNode);
    xl6CgenAddSyncCleanup(ctx, node->text, typeNode);

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( receiverType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = receiverType->children[0];
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = %s", cType, cName, receiverTempName);
    if ( !xl6CgenEmitClassFieldSuffix(ctx, classNode, initExpr->text) ) {
        xl6CgenError(ctx, initExpr, "class field not found");
    }
    xl6OutputPuts(";\n", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;

    xl6CgenDeclDestroyTemporaryClassValue(ctx, classNode, receiverType, receiverTempName);

    xl6AstFree(memberFunctionType);
    xl6AstFree(fieldType);
    xl6AstFree(receiverType);
    return 1;
}
