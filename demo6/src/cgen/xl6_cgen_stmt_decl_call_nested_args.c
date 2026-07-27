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
            直接传递可让被调函数的就地修改对调用方可见。只有字面量和拥有型返回值
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

/*
    task.all/race/whenAny 会直接把数组字面量展开为可变参数。
    数组节点本身不会经过 xl6CgenExpr，不能为它建立普通实参替换，否则会先求值一次
    临时数组，再把元素重新求值一次。这里只继续收集元素内部真正会被消费的临时量。
*/
static int xl6CgenDeclCollectTaskLiteralElementTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call,
    XL6_CgenDeclCallArgTemp** temps,
    int* count,
    int* capacity)
{
    const char* memberName;
    const XL6_AstNode* arrayExpr;
    size_t i;

    if ( ctx == NULL || call == NULL ||
         !xl6CgenIsTaskNamespaceMemberCall(call, &memberName) ||
         (!xl6CTextEquals(memberName, "all") &&
          !xl6CTextEquals(memberName, "race") &&
          !xl6CTextEquals(memberName, "whenAny")) ) {
        return 0;
    }

    arrayExpr = xl6CgenTaskCallArgExpr(call);
    if ( arrayExpr == NULL || arrayExpr->kind != XL6_AST_ARRAY_LITERAL ) {
        return 0;
    }

    for ( i = 0; i < arrayExpr->childCount; ++i ) {
        xl6CgenDeclCollectExprCallArgTemps(ctx, arrayExpr->children[i], temps, count, capacity);
    }
    return 1;
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

static void xl6CgenDeclCollectOwnedClassReceiverTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    XL6_CgenDeclCallArgTemp** temps,
    int* count,
    int* capacity)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode* tempType;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return;
    }
    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER || callee->childCount == 0 ) {
        return;
    }
    receiver = callee->children[0];
    if ( receiver == NULL || xl6CgenDeclExprHasReplacement(ctx, receiver) ||
         xl6CgenExprIsXValueBorrow(ctx, receiver) ) {
        return;
    }

    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( receiverType == NULL || xl6CgenClassNodeForType(ctx, receiverType) == NULL ) {
        xl6AstFree(receiverType);
        return;
    }

    /* 先处理接收者内部依赖，再保存接收者本身，严格维持从内到外的求值顺序。 */
    xl6CgenDeclCollectExprCallArgTemps(ctx, receiver, temps, count, capacity);
    tempType = xl6CgenCloneCanonicalTypeNode(ctx, receiverType);
    if ( tempType != NULL && xl6CgenClassHandleTargetType(ctx, tempType) == NULL ) {
        xl6AstFree(tempType);
        tempType = xl6CgenClassHandleTypeForClassType(ctx, receiverType);
    }
    if ( tempType != NULL &&
         xl6CgenDeclCallArgTempAppend(
             ctx,
             temps,
             count,
             capacity,
             receiver,
             tempType,
             "__xl6_call_receiver_",
             "out of memory while preparing class call receiver temporary") ) {
        (*temps)[*count - 1].releaseAfterUse = 1;
    }
    xl6AstFree(receiverType);
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
    const XL6_AstNode* receiverClassType;
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
    receiverClassType = NULL;
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
                receiverClassType = xl6CgenClassHandleTargetType(ctx, receiverType);
                if ( receiverClassType == NULL ) {
                    receiverClassType = xl6CgenResolveAliasType(ctx, receiverType);
                }
                classNode = xl6CgenClassNodeForType(ctx, receiverType);
            }
            if ( classNode != NULL ) {
                const XL6_AstNode* receiverTypeArgs = xl6CgenTypeNodeGenericArgs(receiverClassType);
                if ( receiverTypeArgs != NULL && xl6CgenClassHasTypeParams(classNode) ) {
                    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
                    ctx->currentGenericArgs = receiverTypeArgs;
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
            显式或推断泛型调用收集实参临时量时，参数类型必须先替换 T。
            否则 array<T> 与字面量会被当作未定型容器，错误退回动态 xvalue 路径。
        */
        tempType = xl6CgenCloneCanonicalTypeNode(ctx, xl6CgenSubstituteGenericType(ctx, paramType));
        if ( tempType != NULL && xl6CgenDeclCallArgumentNeedsTemp(ctx, tempType, argExpr) ) {
            /*
                先收集当前实参内部的临时值，再物化当前实参本身。
                这样 array[call(array[...])] 会严格按由内到外的依赖顺序生成。
            */
            xl6CgenDeclCollectExprCallArgTemps(ctx, argExpr, temps, count, capacity);
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

    xl6CgenDeclCollectOwnedClassReceiverTemp(ctx, expr, temps, count, capacity);
    if ( xl6CgenDeclCollectTaskLiteralElementTemps(ctx, expr, temps, count, capacity) ) {
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

        tempType = xl6CgenCloneExprTypeForInference(ctx, argExpr);
        if ( tempType == NULL ) {
            tempType = xl6CgenNewSimpleType("var", argExpr);
        }
        if ( tempType != NULL && xl6CgenDeclCanEmitCallArgTemp(ctx, tempType, argExpr) ) {
            /*
                先物化当前实参内部的调用，再物化实参本身。
                同时保留表达式的真实类型，避免标量结果被错误地再次装箱。
            */
            xl6CgenDeclCollectExprCallArgTemps(ctx, argExpr, temps, count, capacity);
            if ( !xl6CgenDeclCallArgTempAppend(ctx, temps, count, capacity, argExpr, tempType, "__xl6_call_arg_", "out of memory while preparing nested call argument temporaries") ) {
                return;
            }
            continue;
        }
        xl6AstFree(tempType);

        /*
            当前实参本身不需要临时量时，仍继续扫描内部 callable 调用，
            避免嵌套 kwargs 绕过 replacement。
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
        } else if ( temps[i].active && xl6CgenIsStringTypeNode(ctx, temps[i].typeNode) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtFree((void*)%s); %s = 0; }\n",
                            temps[i].name, temps[i].name, temps[i].name);
        } else if ( temps[i].active && temps[i].releaseAfterUse &&
                    xl6CgenClassHandleTargetType(ctx, temps[i].typeNode) != NULL ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xvoUnref(%s); %s = 0; }\n",
                            temps[i].name, temps[i].name, temps[i].name);
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
    memcpy(
        merged,
        ctx->exprReplacements,
        previousCount * sizeof(XL6_CgenExprReplacement));
    memcpy(
        merged + previousCount,
        own,
        ownCount * sizeof(XL6_CgenExprReplacement));
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

    prevExprReplacements = ctx->exprReplacements;
    prevExprReplacementCount = ctx->exprReplacementCount;
    ctx->exprReplacements = replacements;
    ctx->exprReplacementCount = replacementCount;

    for ( i = 0; i < tempCount; ++i ) {
        const char* tempCType;

        ctx->exprReplacementCount = prevExprReplacementCount + (size_t)i;
        tempCType = xl6CgenValueStorageCType(ctx, temps[i].typeNode);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", tempCType, temps[i].name);
        xl6CgenDeclCallArgTempDefaultValue(ctx, temps[i].typeNode);
        xl6OutputPuts(";\n", ctx->out);
        if ( !xl6CgenDeclEmitCallArgTemp(ctx, temps[i].typeNode, tempCType, temps[i].name, temps[i].expr) ) {
            xl6CgenError(ctx, temps[i].expr, "nested call argument lowering failed");
        }
    }
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

    prevExprReplacements = ctx->exprReplacements;
    prevExprReplacementCount = ctx->exprReplacementCount;
    ctx->exprReplacements = replacements;
    ctx->exprReplacementCount = replacementCount;

    for ( i = 0; i < tempCount; ++i ) {
        const char* tempCType = xl6CgenValueStorageCType(ctx, temps[i].typeNode);
        ctx->exprReplacementCount = prevExprReplacementCount + (size_t)i;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", tempCType, temps[i].name);
        xl6CgenDeclCallArgTempDefaultValue(ctx, temps[i].typeNode);
        xl6OutputPuts(";\n", ctx->out);
        if ( !xl6CgenDeclEmitCallArgTemp(ctx, temps[i].typeNode, tempCType, temps[i].name, temps[i].expr) ) {
            xl6CgenError(ctx, temps[i].expr, "nested typed container call argument lowering failed");
        }
    }
    ctx->exprReplacementCount = replacementCount;

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", valueName);
    if ( xl6CgenIsStringTypeNode(ctx, targetType) ) {
        xl6OutputPuts("xrtCopyStr(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts(", 0u)", ctx->out);
    } else {
        xl6CgenExpr(ctx, expr);
    }
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


    prevExprReplacements = ctx->exprReplacements;
    prevExprReplacementCount = ctx->exprReplacementCount;
    ctx->exprReplacements = replacements;
    ctx->exprReplacementCount = replacementCount;

    for ( i = 0; i < tempCount; ++i ) {
        const char* tempCType = xl6CgenValueStorageCType(ctx, temps[i].typeNode);

        ctx->exprReplacementCount = prevExprReplacementCount + (size_t)i;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", tempCType, temps[i].name);
        xl6CgenDeclCallArgTempDefaultValue(ctx, temps[i].typeNode);
        xl6OutputPuts(";\n", ctx->out);
        if ( !xl6CgenDeclEmitCallArgTemp(ctx, temps[i].typeNode, tempCType, temps[i].name, temps[i].expr) ) {
            xl6CgenError(ctx, temps[i].expr, "nested scalar call argument lowering failed");
        }
    }
    ctx->exprReplacementCount = replacementCount;

    /*
        调用实参先按形参类型落到临时量。string 返回值还必须复制到当前
        变量所有权中，因为表达式结果可能由临时接收者或临时作用域持有。
    */
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", valueName);
    if ( xl6CgenIsStringTypeNode(ctx, targetType) ) {
        xl6OutputPuts("xrtCopyStr(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts(", 0u)", ctx->out);
    } else {
        xl6CgenExpr(ctx, expr);
    }
    xl6OutputPuts(";\n", ctx->out);

    ctx->exprReplacements = prevExprReplacements;
    ctx->exprReplacementCount = prevExprReplacementCount;
    xl6CgenDeclDestroyCallArgTemps(ctx, temps, tempCount);
    xrtFree(replacements);
    xl6CgenDeclFreeCallArgTemps(temps, tempCount);
    return 1;
}
