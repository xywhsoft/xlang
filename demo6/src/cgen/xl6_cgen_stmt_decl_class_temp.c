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
    int isConstructor;
    int isUpcast;
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
    temp->isConstructor = 0;
    temp->isUpcast = 0;
    temp->expr = NULL;
    temp->valueType = NULL;
    temp->functionType = NULL;
    temp->classNode = NULL;
    temp->valueName[0] = '\0';
    temp->functionName[0] = '\0';

    functionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, expr);
    returnCanonical = NULL;
    classNode = NULL;
    if ( functionType != NULL ) {
        returns = xl6CgenFunctionReturnTypes(functionType);
        returnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
        returnCanonical = xl6CgenCloneCanonicalTypeNode(ctx, returnType);
        classNode = xl6CgenClassNodeForType(ctx, returnCanonical);
    } else {
        XL6_AstNode* ctorClass = NULL;
        XL6_AstNode* genericType = NULL;

        if ( xl6CgenCallIsClassCtor(ctx, expr, &ctorClass) ) {
            returnCanonical = xl6CgenCloneExprTypeForInference(ctx, expr);
            classNode = ctorClass;
        } else {
            genericType = xl6CgenGenericClassCtorType(ctx, expr, &ctorClass);
            if ( genericType != NULL ) {
                returnCanonical = xl6CgenCloneCanonicalTypeNode(ctx, genericType);
                classNode = ctorClass;
            }
            xl6AstFree(genericType);
        }
        if ( returnCanonical == NULL || classNode == NULL ) {
            xl6AstFree(returnCanonical);
            return 0;
        }
        temp->isConstructor = 1;
    }

    paramCanonical = paramType != NULL ? xl6CgenCloneCanonicalTypeNode(ctx, paramType) : NULL;
    matches = returnCanonical != NULL &&
              classNode != NULL &&
              (paramCanonical == NULL || xl6CgenTypeNodeEqual(paramCanonical, returnCanonical));
    if ( !matches && paramCanonical != NULL && returnCanonical != NULL &&
         xl6CgenExprNeedsClassUpcast(ctx, paramCanonical, expr, NULL, 0, NULL, NULL) ) {
        XL6_AstNode* targetClass = xl6CgenClassNodeForType(ctx, paramCanonical);
        if ( targetClass != NULL ) {
            xl6AstFree(returnCanonical);
            returnCanonical = paramCanonical;
            paramCanonical = NULL;
            classNode = targetClass;
            temp->isUpcast = 1;
            matches = 1;
        }
    }
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

static void xl6CgenDeclCollectClassArgTempsInExprImpl(XL6_CgenContext* ctx,
                                                      const XL6_AstNode* expr,
                                                      int includeCurrent,
                                                      XL6_CgenDeclClassArgTemp** temps,
                                                      int* count,
                                                      int* capacity)
{
    size_t i;

    if ( ctx == NULL || expr == NULL ) {
        return;
    }

    /*
        匿名函数拥有独立的求值作用域。
        外层扫描器不能提前物化函数体里的 class 临时值，否则会引用尚不存在的形参，
        也会把调用时副作用错误地提升到闭包创建时。
    */
    if ( expr->kind == XL6_AST_ANON_FUNC ) {
        return;
    }

    /*
        条件、短路逻辑和空合并只会求值部分分支，不能把分支中的构造提前到外层。
        这些节点必须由各自的语句级 lowering 保持惰性求值。
    */
    if ( expr->kind == XL6_AST_CONDITIONAL ||
         (expr->kind == XL6_AST_BINARY &&
          (xl6CTextEquals(expr->text, "and") ||
           xl6CTextEquals(expr->text, "or") ||
           xl6CTextEquals(expr->text, "??"))) ) {
        return;
    }

    /*
        子表达式先于父表达式物化，确保多层嵌套构造按源码求值顺序完成。
        根表达式由它所在的声明、return 或调用语句接管，这里只收集内部 class 值。
    */
    for ( i = 0; i < expr->childCount; ++i ) {
        xl6CgenDeclCollectClassArgTempsInExprImpl(ctx, expr->children[i], 1, temps, count, capacity);
    }
    if ( includeCurrent ) {
        (void)xl6CgenDeclAppendClassArgTemp(ctx, temps, count, capacity, NULL, expr);
    }
}

static void xl6CgenDeclCollectClassArgTempsInExpr(XL6_CgenContext* ctx,
                                                  const XL6_AstNode* expr,
                                                  XL6_CgenDeclClassArgTemp** temps,
                                                  int* count,
                                                  int* capacity)
{
    xl6CgenDeclCollectClassArgTempsInExprImpl(ctx, expr, 0, temps, count, capacity);
}

static void xl6CgenDeclEmitClassArgTemps(XL6_CgenContext* ctx,
                                         XL6_CgenDeclClassArgTemp* temps,
                                         int count,
                                         int firstLineAlreadyIndented)
{
    XL6_CgenExprReplacement* dependencies;
    const XL6_CgenExprReplacement* savedReplacements;
    size_t savedReplacementCount;
    size_t dependencyCapacity;
    size_t emittedCount;
    int i;
    int firstOutput;

    savedReplacements = ctx != NULL ? ctx->exprReplacements : NULL;
    savedReplacementCount = ctx != NULL ? ctx->exprReplacementCount : 0;
    dependencyCapacity = savedReplacementCount + (count > 0 ? (size_t)count : 0);
    dependencies = dependencyCapacity > 0 ?
                   (XL6_CgenExprReplacement*)xrtMalloc(dependencyCapacity * sizeof(XL6_CgenExprReplacement)) : NULL;
    if ( dependencyCapacity > 0 && dependencies == NULL ) {
        xl6CgenError(ctx, NULL, "out of memory while preparing nested class dependencies");
        return;
    }
    for ( i = 0; i < (int)savedReplacementCount; ++i ) {
        dependencies[i] = savedReplacements[i];
    }

    firstOutput = 1;
    emittedCount = 0;
    for ( i = 0; i < count; ++i ) {
        if ( !temps[i].active ) {
            continue;
        }
        ctx->exprReplacements = dependencies;
        ctx->exprReplacementCount = savedReplacementCount + emittedCount;
        if ( firstOutput ) {
            if ( !firstLineAlreadyIndented ) {
                xl6CgenIndent(ctx);
            }
            firstOutput = 0;
        } else {
            xl6CgenIndent(ctx);
        }
        xl6OutputPrintf(ctx->out, "%s %s = {0};\n", xl6CgenTypeName(ctx, temps[i].valueType), temps[i].valueName);
        if ( temps[i].isConstructor ) {
            if ( !xl6CgenEmitClassCtorInto(ctx, temps[i].expr, temps[i].valueName) ) {
                xl6CgenError(ctx, temps[i].expr, "nested class constructor lowering failed");
            }
        } else if ( temps[i].isUpcast ) {
            if ( !xl6CgenClassUpcastStmt(ctx, temps[i].valueType, temps[i].expr, temps[i].valueName) ) {
                xl6CgenError(ctx, temps[i].expr, "nested class upcast lowering failed");
            }
        } else {
            xl6CgenEmitMemberFunctionStatementTemp(ctx, temps[i].expr, temps[i].functionName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = ", temps[i].valueName);
            xl6CgenMemberFunctionStatementInvokeExpr(ctx, temps[i].expr, temps[i].functionType, temps[i].functionName);
            xl6OutputPuts(";\n", ctx->out);
        }
        dependencies[savedReplacementCount + emittedCount].expr = temps[i].expr;
        dependencies[savedReplacementCount + emittedCount].cName = temps[i].valueName;
        emittedCount += 1;
    }
    ctx->exprReplacements = savedReplacements;
    ctx->exprReplacementCount = savedReplacementCount;
    xrtFree(dependencies);
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

typedef struct XL6_CgenClassExprTempScope {
    XL6_CgenDeclClassArgTemp* temps;
    XL6_CgenExprReplacement* replacements;
    const XL6_CgenExprReplacement* savedReplacements;
    size_t savedReplacementCount;
    int count;
    int capacity;
    int active;
} XL6_CgenClassExprTempScope;

static int xl6CgenClassExprTempScopeBegin(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    XL6_CgenClassExprTempScope* scope)
{
    size_t totalCount;
    int i;

    if ( scope == NULL ) {
        return 0;
    }
    memset(scope, 0, sizeof(*scope));
    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }

    xl6CgenDeclCollectClassArgTempsInExpr(ctx, expr, &scope->temps, &scope->count, &scope->capacity);
    if ( scope->count <= 0 ) {
        xl6CgenDeclFreeClassArgTemps(scope->temps, scope->count);
        memset(scope, 0, sizeof(*scope));
        return 0;
    }

    scope->savedReplacements = ctx->exprReplacements;
    scope->savedReplacementCount = ctx->exprReplacementCount;
    totalCount = scope->savedReplacementCount + (size_t)scope->count;
    scope->replacements = (XL6_CgenExprReplacement*)xrtMalloc(totalCount * sizeof(XL6_CgenExprReplacement));
    if ( scope->replacements == NULL ) {
        xl6CgenError(ctx, expr, "out of memory while preparing class expression temporaries");
        xl6CgenDeclFreeClassArgTemps(scope->temps, scope->count);
        memset(scope, 0, sizeof(*scope));
        return 0;
    }
    for ( i = 0; i < (int)scope->savedReplacementCount; ++i ) {
        scope->replacements[i] = scope->savedReplacements[i];
    }
    for ( i = 0; i < scope->count; ++i ) {
        scope->replacements[scope->savedReplacementCount + (size_t)i].expr = scope->temps[i].expr;
        scope->replacements[scope->savedReplacementCount + (size_t)i].cName = scope->temps[i].valueName;
    }

    xl6CgenDeclEmitClassArgTemps(ctx, scope->temps, scope->count, 0);
    ctx->exprReplacements = scope->replacements;
    ctx->exprReplacementCount = totalCount;
    scope->active = 1;
    return 1;
}

static void xl6CgenClassExprTempScopeEnd(XL6_CgenContext* ctx, XL6_CgenClassExprTempScope* scope)
{
    if ( ctx == NULL || scope == NULL || !scope->active ) {
        return;
    }

    ctx->exprReplacements = scope->savedReplacements;
    ctx->exprReplacementCount = scope->savedReplacementCount;
    xl6CgenDeclDestroyClassArgTemps(ctx, scope->temps, scope->count);
    xrtFree(scope->replacements);
    xl6CgenDeclFreeClassArgTemps(scope->temps, scope->count);
    memset(scope, 0, sizeof(*scope));
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
        if ( xl6CgenExternCallNeedsWrapper(ctx, func) ) {
            xl6CgenExternCallWrapperSymbol(ctx, func);
        } else {
            xl6CgenCStringSymbol(ctx->out, func->text);
        }
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
        if ( temps[i].isConstructor ) {
            if ( !xl6CgenEmitClassCtorInto(ctx, expr, temps[i].valueName) ) {
                xl6CgenError(ctx, expr, "class argument constructor lowering failed");
            }
        } else if ( temps[i].isUpcast ) {
            if ( !xl6CgenClassUpcastStmt(ctx, temps[i].valueType, expr, temps[i].valueName) ) {
                xl6CgenError(ctx, expr, "class argument upcast lowering failed");
            }
        } else {
            xl6CgenEmitMemberFunctionStatementTemp(ctx, expr, temps[i].functionName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = ", temps[i].valueName);
            xl6CgenMemberFunctionStatementInvokeExpr(ctx, expr, temps[i].functionType, temps[i].functionName);
            xl6OutputPuts(";\n", ctx->out);
        }
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
