static int xl6CgenDeclTemporaryClassFieldTypeIsPlain(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( concreteType == NULL ) {
        return 0;
    }

    /*
        当前只接管普通按值字段。function、class、native-backed class、var、
        容器和并发句柄字段涉及借用/保留语义，必须由专门路径处理，
        避免从临时 class 中借出悬空值。
    */
    if ( xl6CgenIsXValueType(concreteType) ||
         xl6CgenIsFunctionType(concreteType) ||
         xl6CgenIsClassType(ctx, concreteType) ||
         xl6CgenIsNativeBackedClassType(ctx, concreteType) ||
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
        xl6CgenExportInternalSymbol(ctx, func);
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
    xl6CgenDeclEmitKnownStatementCallSymbol(ctx, func);
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
