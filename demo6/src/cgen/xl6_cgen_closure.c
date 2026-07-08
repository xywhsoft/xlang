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
    (*vars)[*count].flags = 0u;
    *count += 1;
    return 1;
}

static int xl6CgenNameInParamList(const XL6_AstNode* params, const char* name)
{
    size_t i;

    if ( params == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( param != NULL && xl6CTextEquals(param->text, name) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenCollectAnonCaptures(XL6_CgenContext* ctx, XL6_CgenAnonFunc* info, const XL6_AstNode* node, const XL6_AstNode* params, XL6_CgenVar* outerVars, size_t outerCount)
{
    size_t i;

    if ( node == NULL ) {
        return;
    }
    if ( node != info->node && node->kind == XL6_AST_ANON_FUNC ) {
        return;
    }
    if ( node->kind == XL6_AST_NAME && !xl6CgenNameInParamList(params, node->text) ) {
        int index = xl6CgenScopeVarFind(outerVars, outerCount, node->text);
        if ( index >= 0 ) {
            xl6CgenAnonAddCapture(ctx, info, node->text, outerVars[index].type);
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenCollectAnonCaptures(ctx, info, node->children[i], params, outerVars, outerCount);
    }
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
        if ( info != NULL ) {
            xl6CgenCollectAnonCaptures(ctx, info, node, params, *scopeVars, *scopeCount);
        }
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
