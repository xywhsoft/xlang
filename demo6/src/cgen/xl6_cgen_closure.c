static int xl6CgenScopeVarFind(XL6_CgenVar* vars, size_t count, const char* name)
{
    size_t i;

    if ( vars == NULL || name == NULL ) {
        return -1;
    }
    for ( i = count; i > 0; --i ) {
        if ( xl6CTextEquals(vars[i - 1].name, name) ) {
            return (int)(i - 1);
        }
    }
    return -1;
}

static int xl6CgenScopeVarAdd(XL6_CgenContext* ctx, XL6_CgenVar** vars, size_t* count, size_t* capacity, const char* name, const XL6_AstNode* type)
{
    XL6_CgenVar* newItems;
    size_t newCapacity;

    if ( vars == NULL || count == NULL || capacity == NULL || name == NULL || type == NULL ) {
        return 1;
    }
    if ( *count == *capacity ) {
        newCapacity = *capacity == 0 ? 8 : *capacity * 2;
        newItems = (XL6_CgenVar*)xrtRealloc(*vars, newCapacity * sizeof(XL6_CgenVar));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, type, "匿名函数作用域表内存不足");
            return 0;
        }
        *vars = newItems;
        *capacity = newCapacity;
    }
    (*vars)[*count].name = (char*)name;
    (*vars)[*count].type = type;
    (*vars)[*count].narrowType = NULL;
    (*vars)[*count].flags = 0u;
    *count += 1;
    return 1;
}

static int xl6CgenCopyScopeVars(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    XL6_CgenVar* source,
    size_t sourceCount,
    XL6_CgenVar** target,
    size_t* targetCount,
    size_t* targetCapacity)
{
    size_t capacity;

    if ( target == NULL || targetCount == NULL || targetCapacity == NULL ) {
        return 0;
    }
    capacity = sourceCount > 0 ? sourceCount : 8u;
    *target = (XL6_CgenVar*)xrtMalloc(capacity * sizeof(XL6_CgenVar));
    if ( *target == NULL ) {
        xl6CgenError(ctx, node, "out of memory while copying anonymous function scope");
        return 0;
    }
    if ( sourceCount > 0 ) {
        memcpy(*target, source, sourceCount * sizeof(XL6_CgenVar));
    }
    *targetCount = sourceCount;
    *targetCapacity = capacity;
    return 1;
}

static void xl6CgenCollectAnonCapturesInScope(
    XL6_CgenContext* ctx,
    XL6_CgenAnonFunc* info,
    const XL6_AstNode* node,
    XL6_CgenVar* outerVars,
    size_t outerCount,
    XL6_CgenVar** localVars,
    size_t* localCount,
    size_t* localCapacity)
{
    size_t i;

    if ( node == NULL ) {
        return;
    }

    /*
     * 每个块复制一份绑定表。块内声明按出现顺序生效，离开块后不会污染
     * 外层；这也让嵌套匿名函数能够区分同名参数、局部量和真正的外部捕获。
     */
    if ( node->kind == XL6_AST_BLOCK ) {
        XL6_CgenVar* blockVars;
        size_t blockCount;
        size_t blockCapacity;

        if ( !xl6CgenCopyScopeVars(
                ctx,
                node,
                *localVars,
                *localCount,
                &blockVars,
                &blockCount,
                &blockCapacity) ) {
            return;
        }
        for ( i = 0; i < node->childCount; ++i ) {
            xl6CgenCollectAnonCapturesInScope(
                ctx,
                info,
                node->children[i],
                outerVars,
                outerCount,
                &blockVars,
                &blockCount,
                &blockCapacity);
        }
        xrtFree(blockVars);
        return;
    }

    if ( node->kind == XL6_AST_ANON_FUNC ) {
        const XL6_AstNode* nestedParams =
            node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* nestedBody =
            node->childCount > 2 ? node->children[2] : NULL;
        XL6_CgenVar* nestedVars;
        size_t nestedCount;
        size_t nestedCapacity;

        if ( !xl6CgenCopyScopeVars(
                ctx,
                node,
                *localVars,
                *localCount,
                &nestedVars,
                &nestedCount,
                &nestedCapacity) ) {
            return;
        }
        if ( nestedParams != NULL ) {
            for ( i = 0; i < nestedParams->childCount; ++i ) {
                const XL6_AstNode* param = nestedParams->children[i];
                if ( param != NULL && param->childCount > 0 ) {
                    (void)xl6CgenScopeVarAdd(
                        ctx,
                        &nestedVars,
                        &nestedCount,
                        &nestedCapacity,
                        param->text,
                        param->children[0]);
                }
            }
        }
        xl6CgenCollectAnonCapturesInScope(
            ctx,
            info,
            nestedBody,
            outerVars,
            outerCount,
            &nestedVars,
            &nestedCount,
            &nestedCapacity);
        xrtFree(nestedVars);
        return;
    }

    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        if ( node->childCount > 1 ) {
            xl6CgenCollectAnonCapturesInScope(
                ctx,
                info,
                node->children[1],
                outerVars,
                outerCount,
                localVars,
                localCount,
                localCapacity);
        }
        if ( node->childCount > 0 ) {
            (void)xl6CgenScopeVarAdd(
                ctx,
                localVars,
                localCount,
                localCapacity,
                node->text,
                node->children[0]);
        }
        return;
    }

    if ( node->kind == XL6_AST_FOR ) {
        const XL6_AstNode* targets =
            node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* iterable =
            node->childCount > 1 ? node->children[1] : NULL;
        const XL6_AstNode* body =
            node->childCount > 2 ? node->children[2] : NULL;
        XL6_CgenVar* loopVars;
        size_t loopCount;
        size_t loopCapacity;

        xl6CgenCollectAnonCapturesInScope(
            ctx,
            info,
            iterable,
            outerVars,
            outerCount,
            localVars,
            localCount,
            localCapacity);
        if ( !xl6CgenCopyScopeVars(
                ctx,
                node,
                *localVars,
                *localCount,
                &loopVars,
                &loopCount,
                &loopCapacity) ) {
            return;
        }
        for ( i = 0; targets != NULL && i < targets->childCount; ++i ) {
            const XL6_AstNode* target = targets->children[i];
            const XL6_AstNode* targetType =
                xl6CgenForTargetStorageType(ctx, node, i);
            if ( target != NULL && targetType != NULL ) {
                (void)xl6CgenScopeVarAdd(
                    ctx,
                    &loopVars,
                    &loopCount,
                    &loopCapacity,
                    target->text,
                    targetType);
            }
        }
        xl6CgenCollectAnonCapturesInScope(
            ctx,
            info,
            body,
            outerVars,
            outerCount,
            &loopVars,
            &loopCount,
            &loopCapacity);
        xrtFree(loopVars);
        return;
    }

    if ( node->kind == XL6_AST_NAME &&
         xl6CgenScopeVarFind(*localVars, *localCount, node->text) < 0 ) {
        int index = xl6CgenScopeVarFind(outerVars, outerCount, node->text);
        if ( index >= 0 ) {
            xl6CgenAnonAddCapture(ctx, info, node->text, outerVars[index].type);
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenCollectAnonCapturesInScope(
            ctx,
            info,
            node->children[i],
            outerVars,
            outerCount,
            localVars,
            localCount,
            localCapacity);
    }
}

static void xl6CgenCollectAnonCaptures(
    XL6_CgenContext* ctx,
    XL6_CgenAnonFunc* info,
    const XL6_AstNode* node,
    const XL6_AstNode* params,
    XL6_CgenVar* outerVars,
    size_t outerCount)
{
    const XL6_AstNode* body;
    XL6_CgenVar* localVars;
    size_t localCount;
    size_t localCapacity;
    size_t i;

    if ( node == NULL || info == NULL ) {
        return;
    }
    localVars = NULL;
    localCount = 0;
    localCapacity = 0;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( param != NULL && param->childCount > 0 ) {
                (void)xl6CgenScopeVarAdd(
                    ctx,
                    &localVars,
                    &localCount,
                    &localCapacity,
                    param->text,
                    param->children[0]);
            }
        }
    }
    body = node->kind == XL6_AST_ANON_FUNC && node->childCount > 2 ?
        node->children[2] : node;
    xl6CgenCollectAnonCapturesInScope(
        ctx,
        info,
        body,
        outerVars,
        outerCount,
        &localVars,
        &localCount,
        &localCapacity);
    xrtFree(localVars);
}

static void xl6CgenCollectAnonFuncsInNode(XL6_CgenContext* ctx, const XL6_AstNode* node, XL6_CgenVar** scopeVars, size_t* scopeCount, size_t* scopeCapacity)
{
    size_t i;

    if ( node == NULL ) {
        return;
    }
    if ( node->kind == XL6_AST_ANON_FUNC ) {
        XL6_CgenAnonFunc* info = xl6CgenAddAnonFuncInfo(ctx, node);
        const XL6_AstNode* params = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* body = node->childCount > 2 ? node->children[2] : NULL;
        XL6_CgenVar* nestedVars;
        size_t nestedCount;
        size_t nestedCapacity;

        if ( info != NULL ) {
            xl6CgenCollectAnonCaptures(ctx, info, node, params, *scopeVars, *scopeCount);
        }

        /*
            匿名函数内部仍可声明匿名函数。内层作用域继承当前可见变量，
            再追加本层参数和按顺序出现的局部变量，保证捕获类型完整且支持遮蔽。
        */
        nestedCount = *scopeCount;
        nestedCapacity = nestedCount + (params != NULL ? params->childCount : 0u);
        nestedVars = nestedCapacity > 0 ?
            (XL6_CgenVar*)xrtMalloc(nestedCapacity * sizeof(XL6_CgenVar)) : NULL;
        if ( nestedCapacity > 0 && nestedVars == NULL ) {
            xl6CgenError(ctx, node, "out of memory while collecting nested anonymous functions");
            return;
        }
        if ( nestedCount > 0 ) {
            memcpy(nestedVars, *scopeVars, nestedCount * sizeof(XL6_CgenVar));
        }
        if ( params != NULL ) {
            for ( i = 0; i < params->childCount; ++i ) {
                const XL6_AstNode* param = params->children[i];
                if ( param != NULL && param->childCount > 0 ) {
                    (void)xl6CgenScopeVarAdd(
                        ctx,
                        &nestedVars,
                        &nestedCount,
                        &nestedCapacity,
                        param->text,
                        param->children[0]);
                }
            }
        }
        xl6CgenCollectAnonFuncsInNode(ctx, body, &nestedVars, &nestedCount, &nestedCapacity);
        xrtFree(nestedVars);
        return;
    }
    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        if ( node->childCount > 1 ) {
            xl6CgenCollectAnonFuncsInNode(ctx, node->children[1], scopeVars, scopeCount, scopeCapacity);
        }
        if ( node->childCount > 0 ) {
            const XL6_AstNode* typeNode = node->children[0];
            if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") && node->childCount > 1 ) {
                XL6_AstNode* inferredType = xl6CgenCloneExprTypeForInference(ctx, node->children[1]);
                if ( inferredType != NULL ) {
                    xl6CgenOwnTypeNode(ctx, inferredType);
                    typeNode = inferredType;
                }
            }
            (void)xl6CgenScopeVarAdd(ctx, scopeVars, scopeCount, scopeCapacity, node->text, typeNode);
        }
        return;
    }
    if ( node->kind == XL6_AST_FOR ) {
        const XL6_AstNode* targets =
            node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* iterable =
            node->childCount > 1 ? node->children[1] : NULL;
        const XL6_AstNode* body =
            node->childCount > 2 ? node->children[2] : NULL;
        XL6_CgenVar* loopVars;
        size_t loopCount;
        size_t loopCapacity;

        xl6CgenCollectAnonFuncsInNode(
            ctx,
            iterable,
            scopeVars,
            scopeCount,
            scopeCapacity);
        if ( !xl6CgenCopyScopeVars(
                ctx,
                node,
                *scopeVars,
                *scopeCount,
                &loopVars,
                &loopCount,
                &loopCapacity) ) {
            return;
        }
        for ( i = 0; targets != NULL && i < targets->childCount; ++i ) {
            const XL6_AstNode* target = targets->children[i];
            const XL6_AstNode* targetType =
                xl6CgenForTargetStorageType(ctx, node, i);
            if ( target != NULL && targetType != NULL ) {
                (void)xl6CgenScopeVarAdd(
                    ctx,
                    &loopVars,
                    &loopCount,
                    &loopCapacity,
                    target->text,
                    targetType);
            }
        }
        xl6CgenCollectAnonFuncsInNode(
            ctx,
            body,
            &loopVars,
            &loopCount,
            &loopCapacity);
        xrtFree(loopVars);
        return;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenCollectAnonFuncsInNode(ctx, node->children[i], scopeVars, scopeCount, scopeCapacity);
    }
}

static void xl6CgenCollectAnonFuncsInFunc(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    XL6_CgenVar* scopeVars;
    size_t scopeCount;
    size_t scopeCapacity;
    const XL6_AstNode* params;
    const XL6_AstNode* body;
    size_t i;

    scopeVars = NULL;
    scopeCount = 0;
    scopeCapacity = 0;
    params = funcNode != NULL && funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    body = funcNode != NULL && funcNode->childCount > 3 ? funcNode->children[3] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( param != NULL && param->childCount > 0 ) {
                (void)xl6CgenScopeVarAdd(ctx, &scopeVars, &scopeCount, &scopeCapacity, param->text, param->children[0]);
            }
        }
    }
    xl6CgenCollectAnonFuncsInNode(ctx, body, &scopeVars, &scopeCount, &scopeCapacity);
    xrtFree(scopeVars);
}

static void xl6CgenCollectAnonFuncs(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    size_t i;

    if ( root == NULL ) {
        return;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        XL6_CgenVar* scopeVars;
        size_t scopeCount;
        size_t scopeCapacity;

        if ( node == NULL ) {
            continue;
        }
        if ( node->kind == XL6_AST_FUNC || node->kind == XL6_AST_EXPORT_FUNC ) {
            xl6CgenCollectAnonFuncsInFunc(ctx, node);
        } else {
            /*
             * 模块级变量是真正的全局变量，匿名函数应直接访问它们。
             * 这里为每个顶层节点使用独立的局部表，避免把前一个模块级声明误当作按值捕获。
             */
            scopeVars = NULL;
            scopeCount = 0;
            scopeCapacity = 0;
            xl6CgenCollectAnonFuncsInNode(ctx, node, &scopeVars, &scopeCount, &scopeCapacity);
            xrtFree(scopeVars);
        }
    }
}
