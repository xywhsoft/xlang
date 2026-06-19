static XL6_AstNode* xl6CgenResolveCallFuncInternal(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call,
    const char* name,
    XL6_AstNode** outTypeArgs,
    int report)
{
    XL6_AstNode* fixedMatch;
    XL6_AstNode* varargMatch;
    XL6_AstNode* fixedArgs;
    XL6_AstNode* varargArgs;
    const XL6_AstNode* args;
    size_t i;
    int fixedBestCount;
    int varargBestCount;
    int fixedBestCost;
    int varargBestCost;
    int fixedBestIsGeneric;
    int varargBestIsGeneric;

    if ( outTypeArgs != NULL ) {
        *outTypeArgs = NULL;
    }
    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    fixedMatch = NULL;
    varargMatch = NULL;
    fixedArgs = NULL;
    varargArgs = NULL;
    fixedBestCount = 0;
    varargBestCount = 0;
    fixedBestCost = 0;
    varargBestCost = 0;
    fixedBestIsGeneric = 0;
    varargBestIsGeneric = 0;

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        XL6_AstNode* nameNode;
        int cost;
        XL6_AstNode* candidateArgs;

        if ( node == NULL || node->kind != XL6_AST_FUNC || node->childCount == 0 ) {
            continue;
        }
        nameNode = node->children[0];
        if ( nameNode == NULL || !xl6CTextEquals(nameNode->text, name) ) {
            continue;
        }

        candidateArgs = NULL;
        if ( !xl6CgenFuncMatchesCallEx(ctx, node, args, &cost, &candidateArgs) ) {
            xl6AstFree(candidateArgs);
            continue;
        }

        if ( xl6CgenFuncHasVararg(node) ) {
            xl6CgenSelectCallCandidate(&varargMatch, &varargArgs,
                                       &varargBestCount, &varargBestCost, &varargBestIsGeneric,
                                       node, candidateArgs, cost);
        } else {
            xl6CgenSelectCallCandidate(&fixedMatch, &fixedArgs,
                                       &fixedBestCount, &fixedBestCost, &fixedBestIsGeneric,
                                       node, candidateArgs, cost);
        }
    }

    if ( fixedBestCount == 1 ) {
        xl6AstFree(varargArgs);
        if ( outTypeArgs != NULL ) {
            *outTypeArgs = fixedArgs;
        } else {
            xl6AstFree(fixedArgs);
        }
        return fixedMatch;
    }
    if ( fixedBestCount > 1 ) {
        xl6AstFree(fixedArgs);
        xl6AstFree(varargArgs);
        if ( report ) {
            xl6CgenError(ctx, call, "重载调用存在歧义，当�?C 生成无法选择唯一函数");
        }
        return NULL;
    }

    if ( varargBestCount == 1 ) {
        if ( outTypeArgs != NULL ) {
            *outTypeArgs = varargArgs;
        } else {
            xl6AstFree(varargArgs);
        }
        return varargMatch;
    }
    xl6AstFree(fixedArgs);
    xl6AstFree(varargArgs);
    if ( varargBestCount > 1 ) {
        if ( report ) {
            xl6CgenError(ctx, call, "重载调用存在歧义，当�?C 生成无法选择唯一函数");
        }
    } else if ( report ) {
        xl6CgenError(ctx, call, "找不到匹配的重载函数");
    }
    return NULL;
}

static XL6_AstNode* xl6CgenResolveCallFuncWithTypeArgs(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name, XL6_AstNode** outTypeArgs)
{
    return xl6CgenResolveCallFuncInternal(ctx, call, name, outTypeArgs, 1);
}

static XL6_AstNode* xl6CgenResolveExplicitGenericCallFunc(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call,
    const char* name,
    const XL6_AstNode* typeArgs,
    int report)
{
    XL6_AstNode* fixedMatch;
    XL6_AstNode* varargMatch;
    const XL6_AstNode* args;
    size_t i;
    int fixedBestCount;
    int varargBestCount;
    int fixedBestCost;
    int varargBestCost;

    if ( ctx == NULL || ctx->root == NULL || name == NULL || typeArgs == NULL ) {
        return NULL;
    }

    fixedMatch = NULL;
    varargMatch = NULL;
    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    fixedBestCount = 0;
    varargBestCount = 0;
    fixedBestCost = 0;
    varargBestCost = 0;

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        XL6_AstNode* nameNode;
        int cost;

        if ( node == NULL || node->kind != XL6_AST_FUNC || !xl6CgenFuncIsGeneric(node) || node->childCount == 0 ) {
            continue;
        }
        nameNode = node->children[0];
        if ( nameNode == NULL || !xl6CTextEquals(nameNode->text, name) ) {
            continue;
        }
        if ( !xl6CgenFuncMatchesExplicitGenericCall(ctx, node, args, typeArgs, &cost) ) {
            continue;
        }

        if ( xl6CgenFuncHasVararg(node) ) {
            if ( varargBestCount == 0 || cost < varargBestCost ) {
                varargMatch = node;
                varargBestCost = cost;
                varargBestCount = 1;
            } else if ( cost == varargBestCost ) {
                varargBestCount += 1;
            }
        } else {
            if ( fixedBestCount == 0 || cost < fixedBestCost ) {
                fixedMatch = node;
                fixedBestCost = cost;
                fixedBestCount = 1;
            } else if ( cost == fixedBestCost ) {
                fixedBestCount += 1;
            }
        }
    }

    if ( fixedBestCount == 1 ) {
        return fixedMatch;
    }
    if ( fixedBestCount > 1 ) {
        if ( report ) {
            xl6CgenError(ctx, call, "显式泛型函数调用存在重载歧义");
        }
        return NULL;
    }
    if ( varargBestCount == 1 ) {
        return varargMatch;
    }
    if ( report ) {
        if ( varargBestCount > 1 ) {
            xl6CgenError(ctx, call, "显式泛型函数调用存在重载歧义");
        } else {
            xl6CgenError(ctx, call, "找不到匹配的显式泛型函数重载");
        }
    }
    return NULL;
}

static XL6_AstNode* xl6CgenResolveCallFunc(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name)
{
    XL6_AstNode* typeArgs;
    XL6_AstNode* func;
    XL6_AstNode* fixedMatch;
    XL6_AstNode* varargMatch;
    const XL6_AstNode* args;
    size_t i;
    int fixedBestCount;
    int varargBestCount;
    int fixedBestCost;
    int varargBestCost;

    typeArgs = NULL;
    func = xl6CgenResolveCallFuncInternal(ctx, call, name, &typeArgs, 1);
    if ( typeArgs != NULL ) {
        xl6AstFree(typeArgs);
        return NULL;
    }
    return func;

    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    if ( xl6CgenFuncOverloadCount(ctx, name) <= 1 ) {
        return xl6CgenFindFunc(ctx, name);
    }

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    fixedMatch = NULL;
    varargMatch = NULL;
    fixedBestCount = 0;
    varargBestCount = 0;
    fixedBestCost = 0;
    varargBestCost = 0;
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        XL6_AstNode* nameNode;
        if ( node == NULL || node->kind != XL6_AST_FUNC || xl6CgenFuncIsGeneric(node) || node->childCount == 0 ) {
            continue;
        }
        nameNode = node->children[0];
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, name) ) {
            int cost;

            if ( !xl6CgenFuncMatchesCall(ctx, node, args, &cost) ) {
                continue;
            }
            if ( xl6CgenFuncHasVararg(node) ) {
                if ( varargBestCount == 0 || cost < varargBestCost ) {
                    varargMatch = node;
                    varargBestCost = cost;
                    varargBestCount = 1;
                } else if ( cost == varargBestCost ) {
                    varargBestCount += 1;
                }
            } else {
                if ( fixedBestCount == 0 || cost < fixedBestCost ) {
                    fixedMatch = node;
                    fixedBestCost = cost;
                    fixedBestCount = 1;
                } else if ( cost == fixedBestCost ) {
                    fixedBestCount += 1;
                }
            }
        }
    }
    if ( fixedBestCount == 1 ) {
        return fixedMatch;
    }
    if ( fixedBestCount > 1 ) {
        xl6CgenError(ctx, call, "重载调用存在歧义，当�?C 生成无法选择唯一函数");
        return NULL;
    }
    if ( varargBestCount == 1 ) {
        return varargMatch;
    }
    if ( varargBestCount > 1 ) {
        xl6CgenError(ctx, call, "重载调用存在歧义，当�?C 生成无法选择唯一函数");
    } else {
        xl6CgenError(ctx, call, "找不到匹配的重载函数");
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenResolveCallableInBody(
    XL6_CgenContext* ctx,
    const XL6_AstNode* body,
    const XL6_AstNode* call,
    XL6_AstKind kind,
    const char* name,
    int reportNoMatch)
{
    const XL6_AstNode* fixedMatch;
    const XL6_AstNode* varargMatch;
    const XL6_AstNode* args;
    size_t i;
    int candidateCount;
    int fixedBestCount;
    int varargBestCount;
    int fixedBestCost;
    int varargBestCost;

    fixedMatch = NULL;
    varargMatch = NULL;
    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    candidateCount = 0;
    fixedBestCount = 0;
    varargBestCount = 0;
    fixedBestCost = 0;
    varargBestCost = 0;

    if ( body == NULL || name == NULL ) {
        return NULL;
    }

    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* params;
        int fixedCount;
        int hasVararg;

        if ( member == NULL || member->kind != kind || !xl6CTextEquals(xl6CgenCallableName(member), name) ) {
            continue;
        }
        candidateCount += 1;
        {
            int cost;

            if ( !xl6CgenFuncMatchesCall(ctx, member, args, &cost) ) {
                continue;
            }

            params = xl6CgenCallableParams(member);
            fixedCount = xl6CgenFixedParamCount(params);
            hasVararg = params != NULL && params->childCount > (size_t)fixedCount;
            if ( hasVararg ) {
                if ( varargBestCount == 0 || cost < varargBestCost ) {
                    varargMatch = member;
                    varargBestCost = cost;
                    varargBestCount = 1;
                } else if ( cost == varargBestCost ) {
                    varargBestCount += 1;
                }
            } else {
                if ( fixedBestCount == 0 || cost < fixedBestCost ) {
                    fixedMatch = member;
                    fixedBestCost = cost;
                    fixedBestCount = 1;
                } else if ( cost == fixedBestCost ) {
                    fixedBestCount += 1;
                }
            }
        }
    }

    if ( fixedBestCount == 1 ) {
        return fixedMatch;
    }
    if ( fixedBestCount > 1 ) {
        xl6CgenError(ctx, call, "成员调用存在重载歧义，当�?C 生成无法选择唯一成员");
        return NULL;
    }
    if ( varargBestCount == 1 ) {
        return varargMatch;
    }
    if ( varargBestCount > 1 ) {
        xl6CgenError(ctx, call, "成员调用存在重载歧义，当�?C 生成无法选择唯一成员");
    } else if ( candidateCount > 0 && reportNoMatch ) {
        xl6CgenError(ctx, call, "找不到匹配的成员重载");
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenResolveClassCreate(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* call)
{
    return xl6CgenResolveCallableInBody(ctx, xl6CgenClassBody(classNode), call, XL6_AST_CREATE, "create", 1);
}

static const XL6_AstNode* xl6CgenResolveClassFunc(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* call,
    const char* name,
    const XL6_AstNode** outOwnerClass)
{
    const XL6_AstNode* method;
    XL6_AstNode* baseClass;

    if ( outOwnerClass != NULL ) {
        *outOwnerClass = NULL;
    }
    if ( classNode == NULL || name == NULL ) {
        return NULL;
    }

    method = xl6CgenResolveCallableInBody(ctx, xl6CgenClassBody(classNode), call, XL6_AST_FUNC, name, 0);
    if ( method != NULL ) {
        if ( outOwnerClass != NULL ) {
            *outOwnerClass = classNode;
        }
        return method;
    }

    baseClass = xl6CgenBaseClass(ctx, classNode);
    if ( baseClass == NULL ) {
        return NULL;
    }
    return xl6CgenResolveClassFunc(ctx, baseClass, call, name, outOwnerClass);
}

static const XL6_AstNode* xl6CgenResolveCustomFunc(
    XL6_CgenContext* ctx,
    const XL6_AstNode* customNode,
    const XL6_AstNode* call,
    const char* name,
    int reportNoMatch)
{
    const XL6_AstNode* body;

    body = customNode != NULL && customNode->childCount > 0 ? customNode->children[0] : NULL;
    return xl6CgenResolveCallableInBody(ctx, body, call, XL6_AST_FUNC, name, reportNoMatch);
}

static const XL6_AstNode* xl6CgenResolveCustomExternFuncWithReport(
    XL6_CgenContext* ctx,
    const XL6_AstNode* customNode,
    const XL6_AstNode* call,
    const char* name,
    int reportNoMatch)
{
    const XL6_AstNode* body;

    body = customNode != NULL && customNode->childCount > 0 ? customNode->children[0] : NULL;
    return xl6CgenResolveCallableInBody(ctx, body, call, XL6_AST_CUSTOM_EXTERN_FUNC, name, reportNoMatch);
}

static const XL6_AstNode* xl6CgenResolveCustomExternFunc(
    XL6_CgenContext* ctx,
    const XL6_AstNode* customNode,
    const XL6_AstNode* call,
    const char* name)
{
    return xl6CgenResolveCustomExternFuncWithReport(ctx, customNode, call, name, 1);
}

static const XL6_AstNode* xl6CgenResolveCustomCreate(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* call)
{
    const XL6_AstNode* body;

    body = customNode != NULL && customNode->childCount > 0 ? customNode->children[0] : NULL;
    return xl6CgenResolveCallableInBody(ctx, body, call, XL6_AST_CUSTOM_EXTERN_SPECIAL, "create", 1);
}

static int xl6CgenCallIsCustomCtor(XL6_CgenContext* ctx, const XL6_AstNode* expr, XL6_AstNode** outCustom)
{
    const XL6_AstNode* callee;
    XL6_AstNode* customNode;

    if ( outCustom != NULL ) {
        *outCustom = NULL;
    }
    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }
    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    customNode = xl6CgenFindCustom(ctx, callee->text);
    if ( customNode == NULL ) {
        return 0;
    }
    if ( outCustom != NULL ) {
        *outCustom = customNode;
    }
    return 1;
}

static int xl6CgenCallIsClassCtor(XL6_CgenContext* ctx, const XL6_AstNode* expr, XL6_AstNode** outClass)
{
    const XL6_AstNode* callee;
    XL6_AstNode* classNode;

    if ( outClass != NULL ) {
        *outClass = NULL;
    }
    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }
    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    classNode = xl6CgenFindClass(ctx, callee->text);
    if ( classNode == NULL ) {
        return 0;
    }
    if ( outClass != NULL ) {
        *outClass = classNode;
    }
    return 1;
}

static int xl6CgenClassBasePath(XL6_CgenContext* ctx, const XL6_AstNode* fromClass, const XL6_AstNode* targetClass, char* buffer, size_t capacity)
{
    XL6_AstNode* baseClass;

    if ( buffer == NULL || capacity == 0 || fromClass == NULL || targetClass == NULL ) {
        return 0;
    }
    if ( xl6CTextEquals(fromClass->text, targetClass->text) ) {
        return 1;
    }

    baseClass = xl6CgenBaseClass(ctx, fromClass);
    if ( baseClass == NULL ) {
        return 0;
    }
    xl6CgenAppendText(buffer, capacity, ".base");
    if ( xl6CTextEquals(baseClass->text, targetClass->text) ) {
        return 1;
    }
    return xl6CgenClassBasePath(ctx, baseClass, targetClass, buffer, capacity);
}

static void xl6CgenCustomCtorValue(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* call)
{
    const XL6_AstNode* createNode;
    const XL6_AstNode* args;

    args = call->childCount > 1 ? call->children[1] : NULL;
    createNode = xl6CgenResolveCustomCreate(ctx, customNode, call);

    if ( createNode != NULL ) {
        xl6CgenCStringSymbol(ctx->out, createNode->text);
        xl6OutputPutc('(', ctx->out);
        xl6CgenKnownCallArgs(ctx, createNode, args);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    if ( args != NULL && args->childCount == 1 && args->children[0] != NULL && args->children[0]->childCount > 0 ) {
        xl6CgenExpr(ctx, args->children[0]->children[0]);
        return;
    }

    xl6CgenError(ctx, call, "custom 没有 create 时只能用一�?point 参数绑定已有句柄");
    xl6OutputPuts("0", ctx->out);
}

static void xl6CgenCustomCtorAssign(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const char* varName, const XL6_AstNode* call)
{
    xl6CgenIndent(ctx);
    xl6CgenFputsCIdent(ctx->out, varName);
    xl6OutputPuts(".handle = ", ctx->out);
    xl6CgenCustomCtorValue(ctx, customNode, call);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenDefaultValue(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        xl6OutputPuts("0", ctx->out);
        return;
    }
    if ( xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float32") ||
         xl6CTextEquals(typeNode->text, "float64") ) {
        xl6OutputPuts("0.0", ctx->out);
        return;
    }
    if ( xl6CTextEquals(typeNode->text, "string") ) {
        xl6OutputPuts("\"\"", ctx->out);
        return;
    }
    if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6OutputPuts("{0}", ctx->out);
        return;
    }
    if ( xl6CTextEquals(typeNode->text, "array") ) {
        xl6OutputPuts("xvoCreateArray()", ctx->out);
        return;
    }
    if ( xl6CTextEquals(typeNode->text, "list") ) {
        xl6OutputPuts("xvoCreateList()", ctx->out);
        return;
    }
    if ( xl6CTextEquals(typeNode->text, "set") ) {
        xl6OutputPuts("xvoCreateSet()", ctx->out);
        return;
    }
    if ( xl6CTextEquals(typeNode->text, "dict") ) {
        xl6OutputPuts("xvoCreateTable()", ctx->out);
        return;
    }
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        return;
    }
    xl6OutputPuts("0", ctx->out);
}
