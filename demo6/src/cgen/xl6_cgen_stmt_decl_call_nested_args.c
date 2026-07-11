static int xl6CgenDeclCallArgTempAppend(
    XL6_CgenContext* ctx,
    XL6_CgenDeclCallArgTemp** temps,
    int* count,
    int* capacity,
    const XL6_AstNode* expr,
    XL6_AstNode* typeNode,
    const char* prefix,
    const char* label)
{
    XL6_CgenDeclCallArgTemp* newTemps;
    int newCapacity;

    if ( ctx == NULL || temps == NULL || count == NULL || capacity == NULL ||
         expr == NULL || typeNode == NULL || prefix == NULL ) {
        xl6AstFree(typeNode);
        return 0;
    }

    if ( *count == *capacity ) {
        newCapacity = *capacity == 0 ? 4 : (*capacity * 2);
        newTemps = (XL6_CgenDeclCallArgTemp*)xrtRealloc(*temps, (size_t)newCapacity * sizeof(XL6_CgenDeclCallArgTemp));
        if ( newTemps == NULL ) {
            xl6AstFree(typeNode);
            xl6CgenError(ctx, expr, label != NULL ? label : "out of memory while preparing call argument temporaries");
            return 0;
        }
        memset(newTemps + *capacity, 0, (size_t)(newCapacity - *capacity) * sizeof(XL6_CgenDeclCallArgTemp));
        *temps = newTemps;
        *capacity = newCapacity;
    }

    (*temps)[*count].active = 1;
    (*temps)[*count].expr = expr;
    (*temps)[*count].typeNode = typeNode;
    snprintf((*temps)[*count].name, sizeof((*temps)[*count].name), "%s%d", prefix, ctx->tempId++);
    *count += 1;
    return 1;
}

static void xl6CgenDeclCollectCallExprArgTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    XL6_CgenDeclCallArgTemp** temps,
    int* count,
    int* capacity);

static int xl6CgenDeclExprHasReplacement(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    size_t i;

    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->exprReplacementCount; ++i ) {
        if ( ctx->exprReplacements[i].expr == expr && ctx->exprReplacements[i].cName != NULL ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenDeclCallArgumentNeedsTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* expr)
{
    if ( xl6CgenDeclExprHasReplacement(ctx, expr) ) {
        return 0;
    }
    if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        /*
            typed container 参数传递的是 XRT 对象句柄。具名变量、成员和索引都是借用值，
            直接传递即可让被调函数的就地修改对调用方可见；只有字面量和拥有型返回值
            需要先物化，并在调用结束后释放临时句柄。
        */
        return xl6CgenTypedContainerInitLiteral(ctx, typeNode, expr) != NULL ||
               xl6CgenTypedContainerCanMoveFromExpr(ctx, typeNode, expr) ||
               xl6CgenTypedContainerCanImportDynamicExpr(ctx, typeNode, expr);
    }
    return xl6CgenDeclCanEmitCallArgTemp(ctx, typeNode, expr);
}

/*
    一个标量表达式中可以嵌套任意层调用。调用节点由上面的收集器完整处理，
    其他节点按源码顺序递归，保证实参物化顺序和表达式求值顺序一致。
*/
static void xl6CgenDeclCollectExprCallArgTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    XL6_CgenDeclCallArgTemp** temps,
    int* count,
    int* capacity)
{
    size_t i;

    if ( ctx == NULL || expr == NULL || xl6CgenDeclExprHasReplacement(ctx, expr) ) {
        return;
    }
    if ( expr->kind == XL6_AST_CALL ) {
        xl6CgenDeclCollectCallExprArgTemps(ctx, expr, temps, count, capacity);
        return;
    }
    for ( i = 0; i < expr->childCount; ++i ) {
        xl6CgenDeclCollectExprCallArgTemps(ctx, expr->children[i], temps, count, capacity);
    }
}

static int xl6CgenDeclExprNeedsCallArgTemps(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_CgenDeclCallArgTemp* temps;
    int tempCount;
    int tempCapacity;
    int savedTempId;

    if ( ctx == NULL || expr == NULL || xl6CgenDeclExprHasReplacement(ctx, expr) ) {
        return 0;
    }
    temps = NULL;
    tempCount = 0;
    tempCapacity = 0;
    savedTempId = ctx->tempId;
    xl6CgenDeclCollectExprCallArgTemps(ctx, expr, &temps, &tempCount, &tempCapacity);
    ctx->tempId = savedTempId;
    xl6CgenDeclFreeCallArgTemps(temps, tempCapacity);
    return tempCount > 0;
}

static int xl6CgenDeclCollectKnownCallExprArgTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    XL6_CgenDeclCallArgTemp** temps,
    int* count,
    int* capacity)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    const XL6_AstNode* params;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* ownerClass;
    XL6_AstNode* inferredArgs;
    XL6_AstNode* receiverType;
    XL6_AstNode* classNode;
    XL6_AstNode* func;
    int fixedCount;
    int i;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }

    callee = expr->children[0];
    inferredArgs = NULL;
    func = NULL;
    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    prevClass = ctx->currentClass;
    receiverType = NULL;
    classNode = NULL;
    ownerClass = NULL;

    if ( callee != NULL && callee->kind == XL6_AST_GENERIC_INSTANT ) {
        const XL6_AstNode* nameNode = callee->childCount > 0 ? callee->children[0] : NULL;
        const XL6_AstNode* typeArgs = callee->childCount > 1 ? callee->children[1] : NULL;
        if ( nameNode != NULL && nameNode->kind == XL6_AST_NAME ) {
            func = xl6CgenResolveExplicitGenericCallFunc(ctx, expr, nameNode->text, typeArgs, 0);
            if ( func != NULL ) {
                ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
                ctx->currentGenericArgs = typeArgs;
            }
        }
    } else if ( callee != NULL && callee->kind == XL6_AST_NAME ) {
        func = xl6CgenFindExternFunc(ctx, callee->text);
        if ( func == NULL ) {
            XL6_AstNode* exportFunc = xl6CgenFindExportFunc(ctx, callee->text);
            if ( exportFunc != NULL && xl6CgenExportFuncOverloadCount(ctx, callee->text) <= 1 ) {
                func = exportFunc;
            }
        }
        if ( func == NULL ) {
            func = xl6CgenResolveCallFuncInternal(ctx, expr, callee->text, &inferredArgs, 0);
            if ( func != NULL && inferredArgs != NULL ) {
                ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
                ctx->currentGenericArgs = inferredArgs;
            }
        }
    } else if ( callee != NULL && callee->kind == XL6_AST_MEMBER &&
                callee->text != NULL && callee->childCount > 0 ) {
        const XL6_AstNode* receiver = callee->children[0];

        classNode = xl6CgenClassDeclFromTypeMemberReceiver(ctx, callee);
        if ( classNode != NULL ) {
            func = (XL6_AstNode*)xl6CgenResolveStaticClassFunc(
                ctx,
                classNode,
                expr,
                callee->text,
                &ownerClass);
        } else {
            if ( receiver != NULL && receiver->kind == XL6_AST_NAME &&
                 xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
                classNode = (XL6_AstNode*)ctx->currentClass;
            } else {
                receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
                classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
            }
            if ( classNode != NULL ) {
                if ( receiverType != NULL && receiverType->childCount > 0 &&
                     xl6CgenClassHasTypeParams(classNode) ) {
                    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
                    ctx->currentGenericArgs = receiverType->children[0];
                }
                ctx->currentClass = classNode;
                func = (XL6_AstNode*)xl6CgenResolveClassFunc(
                    ctx,
                    classNode,
                    expr,
                    callee->text,
                    &ownerClass);
                if ( ownerClass != NULL ) {
                    ctx->currentClass = ownerClass;
                }
            }
        }
    }

    if ( func == NULL ) {
        xl6AstFree(inferredArgs);
        xl6AstFree(receiverType);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        ctx->currentClass = prevClass;
        return 0;
    }

    params = func->childCount > 1 ? func->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    if ( params == NULL || params->childCount > (size_t)fixedCount ) {
        xl6AstFree(inferredArgs);
        xl6AstFree(receiverType);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        ctx->currentClass = prevClass;
        return 0;
    }

    args = expr->childCount > 1 ? expr->children[1] : NULL;
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
        const XL6_AstNode* argExpr = param != NULL ? xl6CgenFindArgForParam(args, param->text, i) : NULL;
        XL6_AstNode* tempType;

        if ( argExpr == NULL ) {
            continue;
        }

        /*
            显式/推断泛型调用收集实参临时量时，参数类型必须先替换 T。
            否则 array<T> + [1, 2] 会被当成未定型容器，调用点就会退回
            动态 xvalue 字面量，传给 xtarray 形参后在运行期崩溃。
        */
        tempType = xl6CgenCloneCanonicalTypeNode(ctx, xl6CgenSubstituteGenericType(ctx, paramType));
        if ( tempType != NULL && xl6CgenDeclCallArgumentNeedsTemp(ctx, tempType, argExpr) ) {
            if ( !xl6CgenDeclCallArgTempAppend(ctx, temps, count, capacity, argExpr, tempType, "__xl6_call_arg_", "out of memory while preparing nested call argument temporaries") ) {
                xl6AstFree(inferredArgs);
                xl6AstFree(receiverType);
                ctx->currentGenericParams = prevParams;
                ctx->currentGenericArgs = prevArgs;
                ctx->currentClass = prevClass;
                return 1;
            }
            continue;
        }
        xl6AstFree(tempType);
        xl6CgenDeclCollectCallExprArgTemps(ctx, argExpr, temps, count, capacity);
    }

    xl6AstFree(inferredArgs);
    xl6AstFree(receiverType);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    ctx->currentClass = prevClass;
    return 1;
}

static void xl6CgenDeclCollectCallExprArgTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    XL6_CgenDeclCallArgTemp** temps,
    int* count,
    int* capacity)
{
    const XL6_AstNode* args;
    size_t i;

    if ( ctx == NULL || expr == NULL ) {
        return;
    }
    if ( expr->kind != XL6_AST_CALL || expr->childCount < 2 ) {
        return;
    }

    if ( xl6CgenDeclCollectKnownCallExprArgTemps(ctx, expr, temps, count, capacity) ) {
        return;
    }

    args = expr->children[1];
    for ( i = 0; args != NULL && i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        const XL6_AstNode* argExpr = xl6CgenArgExpr(arg);
        XL6_AstNode* tempType;

        if ( argExpr == NULL ) {
            continue;
        }
        if ( xl6CgenDeclExprHasReplacement(ctx, argExpr) ) {
            continue;
        }

        /*
            未通过普通函数签名解析的内建成员调用通常沿用动态 var 路径；
            ref(...) 是例外，它必须保留 ref<Derived> 等静态类型，才能选择
            class 移动装箱路径，不能丢失明确的所有权语义。
        */
        if ( xl6CgenRefCtorValueExpr(argExpr) != NULL ) {
            XL6_AstNode* inferredType = xl6CgenCloneExprTypeForInference(ctx, argExpr);
            tempType = xl6CgenCloneCanonicalTypeNode(ctx, inferredType);
            xl6AstFree(inferredType);
        } else {
            tempType = NULL;
        }
        if ( tempType == NULL ) {
            tempType = xl6CgenNewSimpleType("var", argExpr);
        }
        if ( tempType != NULL && xl6CgenDeclCanEmitCallArgTemp(ctx, tempType, argExpr) ) {
            if ( !xl6CgenDeclCallArgTempAppend(ctx, temps, count, capacity, argExpr, tempType, "__xl6_call_arg_", "out of memory while preparing nested call argument temporaries") ) {
                return;
            }
            continue;
        }
        xl6AstFree(tempType);

        /*
            如果当前实参本身不需要临时量，但内部还有 callable 调用，
            继续向下扫描，避免嵌套 kwargs 绕过 replacement。
        */
        xl6CgenDeclCollectCallExprArgTemps(ctx, argExpr, temps, count, capacity);
    }
}

static int xl6CgenDeclCallExprArgsNeedCallArgTemps(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL ) {
        return 0;
    }
    return xl6CgenDeclExprNeedsCallArgTemps(ctx, expr);
}

static void xl6CgenDeclDestroyCallArgTemps(XL6_CgenContext* ctx, XL6_CgenDeclCallArgTemp* temps, int count)
{
    int i;

    if ( ctx == NULL || temps == NULL ) {
        return;
    }
    for ( i = count - 1; i >= 0; --i ) {
        if ( temps[i].active && xl6CgenIsTypedContainerType(ctx, temps[i].typeNode) ) {
            xl6CgenIndent(ctx);
            xl6CgenTypedContainerDestroyValue(ctx, temps[i].typeNode, temps[i].name);
        }
    }
}

/*
    replacement 表允许语句级 lowering 互相嵌套。当前层放在前面，保证更精确的
    局部替换优先；外层 task/class 等临时值继续保留，不能被当前层静默覆盖。
*/
static XL6_CgenExprReplacement* xl6CgenDeclMergeExprReplacements(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    XL6_CgenExprReplacement* own,
    size_t ownCount,
    size_t* totalCount)
{
    XL6_CgenExprReplacement* merged;
    size_t previousCount;

    if ( totalCount != NULL ) {
        *totalCount = 0;
    }
    if ( ctx == NULL || own == NULL || ownCount == 0 ) {
        xrtFree(own);
        return NULL;
    }
    previousCount = ctx->exprReplacementCount;
    if ( previousCount == 0 ) {
        if ( totalCount != NULL ) {
            *totalCount = ownCount;
        }
        return own;
    }

    merged = (XL6_CgenExprReplacement*)xrtMalloc(
        (ownCount + previousCount) * sizeof(XL6_CgenExprReplacement));
    if ( merged == NULL ) {
        xrtFree(own);
        xl6CgenError(ctx, expr, "out of memory while nesting expression temporaries");
        return NULL;
    }
    memcpy(merged, own, ownCount * sizeof(XL6_CgenExprReplacement));
    memcpy(
        merged + ownCount,
        ctx->exprReplacements,
        previousCount * sizeof(XL6_CgenExprReplacement));
    xrtFree(own);
    if ( totalCount != NULL ) {
        *totalCount = ownCount + previousCount;
    }
    return merged;
}

static int xl6CgenDeclEmitXValueCallExprWithArgTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* expr,
    const char* valueName)
{
    XL6_CgenDeclCallArgTemp* temps;
    XL6_CgenExprReplacement* replacements;
    const XL6_CgenExprReplacement* prevExprReplacements;
    size_t prevExprReplacementCount;
    size_t replacementCount;
    int tempCount;
    int tempCapacity;
    int i;

    temps = NULL;
    replacements = NULL;
    tempCount = 0;
    tempCapacity = 0;
    xl6CgenDeclCollectExprCallArgTemps(ctx, expr, &temps, &tempCount, &tempCapacity);
    if ( tempCount <= 0 ) {
        xl6CgenDeclFreeCallArgTemps(temps, tempCount);
        return 0;
    }

    replacements = xl6CgenDeclCreateCallArgReplacements(ctx, temps, tempCount, tempCount);
    if ( replacements == NULL ) {
        xl6CgenDeclFreeCallArgTemps(temps, tempCount);
        return 0;
    }
    replacements = xl6CgenDeclMergeExprReplacements(
        ctx,
        expr,
        replacements,
        (size_t)tempCount,
        &replacementCount);
    if ( replacements == NULL ) {
        xl6CgenDeclFreeCallArgTemps(temps, tempCount);
        return 0;
    }

    for ( i = 0; i < tempCount; ++i ) {
        const char* tempCType;

        tempCType = xl6CgenValueStorageCType(ctx, temps[i].typeNode);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", tempCType, temps[i].name);
        xl6CgenDeclCallArgTempDefaultValue(ctx, temps[i].typeNode);
        xl6OutputPuts(";\n", ctx->out);
        if ( !xl6CgenDeclEmitCallArgTemp(ctx, temps[i].typeNode, tempCType, temps[i].name, temps[i].expr) ) {
            xl6CgenError(ctx, temps[i].expr, "nested call argument lowering failed");
        }
    }

    prevExprReplacements = ctx->exprReplacements;
    prevExprReplacementCount = ctx->exprReplacementCount;
    ctx->exprReplacements = replacements;
    ctx->exprReplacementCount = replacementCount;

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", valueName);
    xl6CgenXValueFromExprForTarget(ctx, targetType, expr);
    xl6OutputPuts(";\n", ctx->out);

    ctx->exprReplacements = prevExprReplacements;
    ctx->exprReplacementCount = prevExprReplacementCount;
    xl6CgenDeclDestroyCallArgTemps(ctx, temps, tempCount);
    xrtFree(replacements);
    xl6CgenDeclFreeCallArgTemps(temps, tempCount);
    return 1;
}

static int xl6CgenDeclEmitTypedContainerCallExprWithArgTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* expr,
    const char* valueName)
{
    XL6_CgenDeclCallArgTemp* temps;
    XL6_CgenExprReplacement* replacements;
    const XL6_CgenExprReplacement* prevExprReplacements;
    size_t prevExprReplacementCount;
    size_t replacementCount;
    int tempCount;
    int tempCapacity;
    int i;

    if ( ctx == NULL || targetType == NULL || expr == NULL || valueName == NULL ||
         !xl6CgenIsTypedContainerType(ctx, targetType) ) {
        return 0;
    }

    temps = NULL;
    replacements = NULL;
    tempCount = 0;
    tempCapacity = 0;
    xl6CgenDeclCollectCallExprArgTemps(ctx, expr, &temps, &tempCount, &tempCapacity);
    if ( tempCount <= 0 ) {
        xl6CgenDeclFreeCallArgTemps(temps, tempCount);
        return 0;
    }

    replacements = xl6CgenDeclCreateCallArgReplacements(ctx, temps, tempCount, tempCount);
    if ( replacements == NULL ) {
        xl6CgenDeclFreeCallArgTemps(temps, tempCount);
        return 0;
    }
    replacements = xl6CgenDeclMergeExprReplacements(
        ctx,
        expr,
        replacements,
        (size_t)tempCount,
        &replacementCount);
    if ( replacements == NULL ) {
        xl6CgenDeclFreeCallArgTemps(temps, tempCount);
        return 0;
    }

    for ( i = 0; i < tempCount; ++i ) {
        const char* tempCType = xl6CgenValueStorageCType(ctx, temps[i].typeNode);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", tempCType, temps[i].name);
        xl6CgenDeclCallArgTempDefaultValue(ctx, temps[i].typeNode);
        xl6OutputPuts(";\n", ctx->out);
        if ( !xl6CgenDeclEmitCallArgTemp(ctx, temps[i].typeNode, tempCType, temps[i].name, temps[i].expr) ) {
            xl6CgenError(ctx, temps[i].expr, "nested typed container call argument lowering failed");
        }
    }

    prevExprReplacements = ctx->exprReplacements;
    prevExprReplacementCount = ctx->exprReplacementCount;
    ctx->exprReplacements = replacements;
    ctx->exprReplacementCount = replacementCount;

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", valueName);
    xl6CgenExpr(ctx, expr);
    xl6OutputPuts(";\n", ctx->out);

    ctx->exprReplacements = prevExprReplacements;
    ctx->exprReplacementCount = prevExprReplacementCount;
    xl6CgenDeclDestroyCallArgTemps(ctx, temps, tempCount);
    xrtFree(replacements);
    xl6CgenDeclFreeCallArgTemps(temps, tempCount);
    return 1;
}

static int xl6CgenDeclEmitScalarCallExprWithArgTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* expr,
    const char* valueName)
{
    XL6_CgenDeclCallArgTemp* temps;
    XL6_CgenExprReplacement* replacements;
    const XL6_CgenExprReplacement* prevExprReplacements;
    size_t prevExprReplacementCount;
    size_t replacementCount;
    int tempCount;
    int tempCapacity;
    int i;

    if ( ctx == NULL || targetType == NULL || expr == NULL || valueName == NULL ||
         !xl6CgenDeclCallArgStaticScalarType(targetType) ) {
        return 0;
    }

    temps = NULL;
    replacements = NULL;
    tempCount = 0;
    tempCapacity = 0;
    xl6CgenDeclCollectExprCallArgTemps(ctx, expr, &temps, &tempCount, &tempCapacity);
    if ( tempCount <= 0 ) {
        xl6CgenDeclFreeCallArgTemps(temps, tempCount);
        return 0;
    }

    replacements = xl6CgenDeclCreateCallArgReplacements(ctx, temps, tempCount, tempCount);
    if ( replacements == NULL ) {
        xl6CgenDeclFreeCallArgTemps(temps, tempCount);
        return 0;
    }
    replacements = xl6CgenDeclMergeExprReplacements(
        ctx,
        expr,
        replacements,
        (size_t)tempCount,
        &replacementCount);
    if ( replacements == NULL ) {
        xl6CgenDeclFreeCallArgTemps(temps, tempCount);
        return 0;
    }

    for ( i = 0; i < tempCount; ++i ) {
        const char* tempCType = xl6CgenValueStorageCType(ctx, temps[i].typeNode);

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", tempCType, temps[i].name);
        xl6CgenDeclCallArgTempDefaultValue(ctx, temps[i].typeNode);
        xl6OutputPuts(";\n", ctx->out);
        if ( !xl6CgenDeclEmitCallArgTemp(ctx, temps[i].typeNode, tempCType, temps[i].name, temps[i].expr) ) {
            xl6CgenError(ctx, temps[i].expr, "nested scalar call argument lowering failed");
        }
    }

    /*
        标量返回值本身不需要特殊包装；真正需要语句级 lowering 的是调用实参。
        这里复用表达式替换表，确保 array<T> 字面量等参数按形参类型先落到临时量。
    */
    prevExprReplacements = ctx->exprReplacements;
    prevExprReplacementCount = ctx->exprReplacementCount;
    ctx->exprReplacements = replacements;
    ctx->exprReplacementCount = replacementCount;

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", valueName);
    xl6CgenExpr(ctx, expr);
    xl6OutputPuts(";\n", ctx->out);

    ctx->exprReplacements = prevExprReplacements;
    ctx->exprReplacementCount = prevExprReplacementCount;
    xl6CgenDeclDestroyCallArgTemps(ctx, temps, tempCount);
    xrtFree(replacements);
    xl6CgenDeclFreeCallArgTemps(temps, tempCount);
    return 1;
}
