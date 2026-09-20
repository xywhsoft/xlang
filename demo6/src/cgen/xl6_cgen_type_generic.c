static int xl6CgenExprStaticTypeIs(XL6_CgenContext* ctx, const XL6_AstNode* expr, const char* typeName)
{
    XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;
    int result;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    result = concreteType != NULL && concreteType->text != NULL && xl6CTextEquals(concreteType->text, typeName);
    xl6AstFree(typeNode);
    return result;
}
static int xl6CgenBinaryPlusIsString(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL || expr->kind != XL6_AST_BINARY || !xl6CTextEquals(expr->text, "+") || expr->childCount < 2 ) {
        return 0;
    }
    return xl6CgenExprStaticTypeIs(ctx, expr->children[0], "string") &&
           xl6CgenExprStaticTypeIs(ctx, expr->children[1], "string");
}

static int xl6CgenBinaryOpIsOrderedCompare(const char* op)
{
    return xl6CTextEquals(op, "==") || xl6CTextEquals(op, "!=") ||
           xl6CTextEquals(op, "<") || xl6CTextEquals(op, "<=") ||
           xl6CTextEquals(op, ">") || xl6CTextEquals(op, ">=");
}

static int xl6CgenBinaryCompareIsString(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL || expr->kind != XL6_AST_BINARY || expr->childCount < 2 ||
         !xl6CgenBinaryOpIsOrderedCompare(expr->text) ) {
        return 0;
    }
    return xl6CgenExprStaticTypeIs(ctx, expr->children[0], "string") &&
           xl6CgenExprStaticTypeIs(ctx, expr->children[1], "string");
}

static int xl6CgenBinaryCompareIsTypeValue(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL || expr->kind != XL6_AST_BINARY || expr->childCount < 2 ||
         (!xl6CTextEquals(expr->text, "==") && !xl6CTextEquals(expr->text, "!=")) ) {
        return 0;
    }
    return xl6CgenExprStaticTypeIs(ctx, expr->children[0], "type") ||
           xl6CgenExprStaticTypeIs(ctx, expr->children[1], "type");
}

static void xl6CgenTypeCompareOperand(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr != NULL && expr->kind == XL6_AST_NULL ) {
        xl6CgenEmitTypeIdForName(ctx, "null", expr);
        return;
    }
    xl6CgenExpr(ctx, expr);
}

static void xl6CgenTypeCompareExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    xl6OutputPutc('(', ctx->out);
    xl6CgenTypeCompareOperand(ctx, expr->childCount > 0 ? expr->children[0] : NULL);
    xl6OutputPrintf(ctx->out, " %s ", expr->text != NULL ? expr->text : "==");
    xl6CgenTypeCompareOperand(ctx, expr->childCount > 1 ? expr->children[1] : NULL);
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenBinaryApproxIsString(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL || expr->kind != XL6_AST_BINARY || expr->childCount < 2 ||
         !xl6CTextEquals(expr->text, "~=") ) {
        return 0;
    }
    return xl6CgenExprStaticTypeIs(ctx, expr->children[0], "string") &&
           xl6CgenExprStaticTypeIs(ctx, expr->children[1], "string");
}

static int xl6CgenBinaryInIsString(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL || expr->kind != XL6_AST_BINARY || expr->childCount < 2 ||
         !xl6CTextEquals(expr->text, "in") ) {
        return 0;
    }
    return xl6CgenExprStaticTypeIs(ctx, expr->children[0], "string") &&
           xl6CgenExprStaticTypeIs(ctx, expr->children[1], "string");
}

static int xl6CgenTypeParamIndex(const XL6_AstNode* typeParams, const char* name)
{
    size_t i;

    if ( typeParams == NULL || name == NULL ) {
        return -1;
    }
    for ( i = 0; i < typeParams->childCount; ++i ) {
        const XL6_AstNode* param = typeParams->children[i];
        if ( param != NULL && xl6CTextEquals(param->text, name) ) {
            return (int)i;
        }
    }
    return -1;
}

static int xl6CgenTypeContainsTypeParam(const XL6_AstNode* typeParams, const XL6_AstNode* typeNode)
{
    size_t i;

    if ( typeNode == NULL ) {
        return 0;
    }
    if ( xl6CgenTypeParamIndex(typeParams, typeNode->text) >= 0 && typeNode->childCount == 0 ) {
        return 1;
    }
    for ( i = 0; i < typeNode->childCount; ++i ) {
        if ( xl6CgenTypeContainsTypeParam(typeParams, typeNode->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenInferenceTypeEqual(XL6_CgenContext* ctx, const XL6_AstNode* left, const XL6_AstNode* right)
{
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    int equal;

    leftType = xl6CgenCloneCanonicalTypeNode(ctx, left);
    rightType = xl6CgenCloneCanonicalTypeNode(ctx, right);
    equal = leftType != NULL && rightType != NULL && xl6CgenTypeNodeEqual(leftType, rightType);
    xl6AstFree(leftType);
    xl6AstFree(rightType);
    return equal;
}

static int xl6CgenRememberInferredType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferred,
    int index,
    const XL6_AstNode* argType,
    int report,
    const XL6_AstNode* at)
{
    XL6_AstNode* canonicalType;

    (void)typeParams;
    canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, argType);
    if ( canonicalType == NULL ) {
        if ( report ) {
            xl6CgenError(ctx, at, "cannot infer generic class type from argument");
        }
        return 0;
    }

    if ( inferred[index] == NULL ) {
        inferred[index] = canonicalType;
        return 1;
    }

    if ( xl6CgenTypeNodeEqual(inferred[index], canonicalType) ) {
        xl6AstFree(canonicalType);
        return 1;
    }

    xl6AstFree(canonicalType);
    if ( report ) {
        xl6CgenError(ctx, at, "generic type inference conflict");
    }
    return 0;
}

static int xl6CgenInferGenericTypeFromTypes(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferred,
    const XL6_AstNode* paramType,
    const XL6_AstNode* argType,
    int report,
    const XL6_AstNode* at)
{
    const XL6_AstNode* paramResolved;
    const XL6_AstNode* argResolved;
    int typeParamIndex;
    size_t i;

    if ( paramType == NULL || !xl6CgenTypeContainsTypeParam(typeParams, paramType) ) {
        return 1;
    }
    if ( argType == NULL ) {
        if ( report ) {
            xl6CgenError(ctx, at, "cannot infer generic class type from argument");
        }
        return 0;
    }

    paramResolved = xl6CgenResolveAliasType(ctx, paramType);
    argResolved = xl6CgenResolveAliasType(ctx, argType);
    if ( paramResolved == NULL || argResolved == NULL ) {
        if ( report ) {
            xl6CgenError(ctx, at, "cannot infer generic class type from argument");
        }
        return 0;
    }
    typeParamIndex = xl6CgenTypeParamIndex(typeParams, paramResolved != NULL ? paramResolved->text : NULL);

    if ( typeParamIndex >= 0 && paramResolved->childCount == 0 ) {
        return xl6CgenRememberInferredType(ctx, typeParams, inferred, typeParamIndex, argResolved, report, at);
    }

    if ( paramResolved->kind != argResolved->kind ||
         paramResolved->childCount != argResolved->childCount ||
         ((paramResolved->text == NULL) != (argResolved->text == NULL)) ||
         (paramResolved->text != NULL && !xl6CTextEquals(paramResolved->text, argResolved->text)) ) {
        if ( report ) {
            xl6CgenError(ctx, at, "cannot infer generic class type from argument");
        }
        return 0;
    }

    for ( i = 0; i < paramResolved->childCount; ++i ) {
        const XL6_AstNode* paramChild = paramResolved->children[i];
        const XL6_AstNode* argChild = argResolved->children[i];

        if ( xl6CgenTypeContainsTypeParam(typeParams, paramChild) ) {
            if ( !xl6CgenInferGenericTypeFromTypes(ctx, typeParams, inferred, paramChild, argChild, report, at) ) {
                return 0;
            }
        } else if ( !xl6CgenInferenceTypeEqual(ctx, paramChild, argChild) ) {
            if ( report ) {
                xl6CgenError(ctx, at, "generic argument type structure mismatch");
            }
            return 0;
        }
    }

    return 1;
}

static XL6_AstNode* xl6CgenInferGenericTypeArgs(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* func, int report)
{
    const XL6_AstNode* typeParams;
    const XL6_AstNode* params;
    const XL6_AstNode* args;
    XL6_AstNode** inferred;
    XL6_AstNode* result;
    size_t i;
    int ok;

    typeParams = xl6CgenFuncTypeParams(func);
    if ( typeParams == NULL || typeParams->childCount == 0 ) {
        return NULL;
    }

    inferred = (XL6_AstNode**)xrtCalloc(typeParams->childCount, sizeof(XL6_AstNode*));
    if ( inferred == NULL ) {
        if ( report ) {
            xl6CgenError(ctx, call, "generic type inference out of memory");
        }
        return NULL;
    }

    params = func->childCount > 1 ? func->children[1] : NULL;
    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    ok = 1;

    if ( params != NULL ) {
        int fixedCount = xl6CgenFixedParamCount(params);
        for ( i = 0; i < (size_t)fixedCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            const XL6_AstNode* paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
            const XL6_AstNode* expr = xl6CgenFindArgForParam(args, param != NULL ? param->text : NULL, (int)i);

            if ( expr == NULL || paramType == NULL || !xl6CgenTypeContainsTypeParam(typeParams, paramType) ) {
                continue;
            }

            {
                XL6_AstNode* exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
                if ( exprType == NULL ) {
                    ok = 0;
                    if ( report ) {
                        xl6CgenError(ctx, expr, "cannot infer generic class type from argument");
                    }
                    break;
                }
                if ( !xl6CgenInferGenericTypeFromTypes(ctx, typeParams, inferred, paramType, exprType, report, expr) ) {
                    xl6AstFree(exprType);
                    ok = 0;
                    break;
                }
                xl6AstFree(exprType);
            }
        }
    }

    for ( i = 0; ok && i < typeParams->childCount; ++i ) {
        if ( inferred[i] == NULL ) {
            ok = 0;
            if ( report ) {
                xl6CgenError(ctx, call, "not all generic type parameters can be inferred");
            }
        }
    }

    if ( !ok ) {
        for ( i = 0; i < typeParams->childCount; ++i ) {
            xl6AstFree(inferred[i]);
        }
        xrtFree(inferred);
        return NULL;
    }

    result = xl6AstNew(XL6_AST_LIST, NULL);
    if ( result == NULL ) {
        for ( i = 0; i < typeParams->childCount; ++i ) {
            xl6AstFree(inferred[i]);
        }
        xrtFree(inferred);
        if ( report ) {
            xl6CgenError(ctx, call, "generic type inference out of memory");
        }
        return NULL;
    }
    for ( i = 0; i < typeParams->childCount; ++i ) {
        xl6AstAddChild(result, inferred[i]);
        inferred[i] = NULL;
    }
    xrtFree(inferred);
    return result;
}

static XL6_AstNode* xl6CgenFindInferredGenericFunc(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name, XL6_AstNode** outTypeArgs, int report)
{
    XL6_AstNode* selected;
    XL6_AstNode* selectedArgs;
    size_t i;
    int candidateCount;
    int matchCount;

    if ( outTypeArgs != NULL ) {
        *outTypeArgs = NULL;
    }
    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }

    selected = NULL;
    selectedArgs = NULL;
    candidateCount = 0;
    matchCount = 0;
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        XL6_AstNode* typeArgs;
        if ( node == NULL || (node->kind != XL6_AST_FUNC && node->kind != XL6_AST_ASYNC_FUNC) || !xl6CgenFuncIsGeneric(node) ||
             node->childCount == 0 || node->children[0] == NULL ||
             !xl6CTextEquals(node->children[0]->text, name) ) {
            continue;
        }
        candidateCount += 1;
        typeArgs = xl6CgenInferGenericTypeArgs(ctx, call, node, 0);
        if ( typeArgs != NULL ) {
            matchCount += 1;
            if ( selectedArgs != NULL ) {
                xl6AstFree(selectedArgs);
            }
            selected = node;
            selectedArgs = typeArgs;
        }
    }

    if ( matchCount == 1 ) {
        if ( outTypeArgs != NULL ) {
            *outTypeArgs = selectedArgs;
        } else {
            xl6AstFree(selectedArgs);
        }
        return selected;
    }

    if ( selectedArgs != NULL ) {
        xl6AstFree(selectedArgs);
    }
    if ( matchCount > 1 && report ) {
        xl6CgenError(ctx, call, "generic function inference is ambiguous");
    } else if ( candidateCount > 0 && report ) {
        for ( i = 0; i < ctx->root->childCount; ++i ) {
            XL6_AstNode* node = ctx->root->children[i];
            if ( node != NULL && (node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC) && xl6CgenFuncIsGeneric(node) &&
                 node->childCount > 0 && node->children[0] != NULL &&
                 xl6CTextEquals(node->children[0]->text, name) ) {
                (void)xl6CgenInferGenericTypeArgs(ctx, call, node, 1);
                break;
            }
        }
    }
    return NULL;
}

static void xl6CgenCollectGenericCall(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArgs;
    XL6_AstNode* func;

    if ( call == NULL || call->childCount == 0 ) {
        return;
    }
    callee = call->children[0];
    if ( callee == NULL ) {
        return;
    }
    if ( callee->kind == XL6_AST_NAME ) {
        XL6_AstNode* inferredArgs = NULL;
        func = xl6CgenResolveCallFuncInternal(ctx, call, callee->text, &inferredArgs, 0);
        if ( func != NULL && inferredArgs != NULL ) {
            xl6CgenAddOwnedGenericInstance(ctx, func, inferredArgs);
        } else {
            xl6AstFree(inferredArgs);
        }
        return;
    }
    if ( callee->kind != XL6_AST_GENERIC_INSTANT || callee->childCount < 2 ) {
        return;
    }

    nameNode = callee->children[0];
    typeArgs = callee->children[1];
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME ) {
        xl6CgenError(ctx, callee, "generic instantiation currently requires function name in C generation");
        return;
    }

    if ( xl6CgenFindClass(ctx, nameNode->text) != NULL ) {
        return;
    }
    if ( xl6CTextEquals(nameNode->text, "channel") ||
         xl6CgenIsSpecialCollectionTypeName(nameNode->text) ) {
        return;
    }
    if ( xl6CgenIsPersistenceGenericCall(call) ) {
        return;
    }
    func = xl6CgenResolveExplicitGenericCallFunc(ctx, call, nameNode->text, typeArgs, 0);
    if ( func == NULL ) {
        xl6CgenError(ctx, callee, "no matching generic function declaration");
        return;
    }
    xl6CgenAddGenericInstance(ctx, func, typeArgs);
}

static void xl6CgenCollectGenericInstances(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return;
    }

    /* 泛型模板本身不直接扫描，只有被实例化后才进入代码生成。*/
    if ( (node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC) && xl6CgenFuncIsGeneric(node) ) {
        return;
    }
    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        const XL6_AstNode* typeNode = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* initExpr = node->childCount > 1 ? node->children[1] : NULL;
        XL6_AstNode* inferredType;

        if ( initExpr != NULL ) {
            xl6CgenCollectGenericInstances(ctx, initExpr);
        }
        if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") ) {
            inferredType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
            if ( inferredType != NULL ) {
                xl6CgenOwnTypeNode(ctx, inferredType);
                xl6CgenAddVarType(ctx, node->text, inferredType);
            }
        } else if ( typeNode != NULL ) {
            xl6CgenAddVarType(ctx, node->text, typeNode);
        }
        return;
    }
    if ( node->kind == XL6_AST_CALL ) {
        xl6CgenCollectGenericCall(ctx, node);
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenCollectGenericInstances(ctx, node->children[i]);
    }
}

static void xl6CgenCollectGenericInstancesToFixedPoint(XL6_CgenContext* ctx)
{
    size_t scanned;

    if ( ctx == NULL ) {
        return;
    }
    xl6CgenCollectGenericInstances(ctx, ctx->root);
    scanned = 0;
    while ( scanned < ctx->genericInstanceCount ) {
        const XL6_CgenGenericInstance* instance = &ctx->genericInstances[scanned];
        const XL6_AstNode* body = instance->func != NULL && instance->func->childCount > 3 ? instance->func->children[3] : NULL;
        xl6CgenCollectGenericInstances(ctx, body);
        scanned += 1;
    }
}

static void xl6CgenDiscoverGenericClassInstancesInNode(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_HirNode* hirNode;
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return;
    }
    if ( node->kind == XL6_AST_CLASS && xl6CgenClassHasTypeParams(node) && ctx->currentGenericArgs == NULL ) {
        return;
    }
    if ( node->kind == XL6_AST_TYPE ) {
        XL6_AstNode* canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, node);
        const XL6_AstNode* classType = xl6CgenClassHandleTargetType(ctx, canonicalType);
        XL6_AstNode* classNode;

        if ( classType == NULL ) {
            classType = canonicalType;
        }
        classNode = classType != NULL && classType->text != NULL ? xl6CgenFindClass(ctx, classType->text) : NULL;
        if ( classNode != NULL && classType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
            xl6CgenAddGenericClassInstance(ctx, classNode, classType);
        }
        if ( canonicalType != NULL && canonicalType->text != NULL ) {
            XL6_AstNode* enumNode = xl6CgenFindEnum(ctx, canonicalType->text);

            if ( enumNode != NULL &&
                 xl6CgenEnumHasTypeParams(enumNode) &&
                 canonicalType->childCount > 0 ) {
                xl6CgenAddGenericEnumInstance(ctx, enumNode, canonicalType);
            }
        }
        xl6AstFree(canonicalType);
    }
    hirNode = xl6CgenHirNode(ctx, node);
    if ( hirNode != NULL && hirNode->type != XL6_HIR_TYPE_INVALID ) {
        XL6_AstNode* hirType = xl6HirTypeCloneAst(&ctx->hir->types, hirNode->type);

        if ( hirType == NULL ) {
            xl6CgenError(ctx, node, "cannot restore typed HIR generic class discovery type");
        } else {
            xl6CgenDiscoverGenericClassInstancesInNode(ctx, hirType);
            xl6AstFree(hirType);
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenDiscoverGenericClassInstancesInNode(ctx, node->children[i]);
    }
}

static void xl6CgenDiscoverGenericClassInstancesToFixedPoint(XL6_CgenContext* ctx)
{
    size_t scanned;

    if ( ctx == NULL ) {
        return;
    }
    xl6CgenDiscoverGenericClassInstancesInNode(ctx, ctx->root);
    scanned = 0;
    while ( scanned < ctx->genericClassInstanceCount ) {
        const XL6_CgenGenericClassInstance* instance = &ctx->genericClassInstances[scanned];
        const XL6_AstNode* classNode = instance->classNode;
        const XL6_AstNode* typeNode = instance->typeNode;
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
        size_t i;

        if ( classNode != NULL && typeNode != NULL && typeNode->childCount > 0 ) {
            ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
            ctx->currentGenericArgs = typeNode->children[0];
            for ( i = 0; i < classNode->childCount; ++i ) {
                xl6CgenDiscoverGenericClassInstancesInNode(ctx, classNode->children[i]);
            }
            ctx->currentGenericParams = prevParams;
            ctx->currentGenericArgs = prevArgs;
        }
        scanned += 1;
    }

    scanned = 0;
    while ( scanned < ctx->genericEnumInstanceCount ) {
        const XL6_CgenGenericEnumInstance* instance =
            &ctx->genericEnumInstances[scanned];
        const XL6_AstNode* previousParams = ctx->currentGenericParams;
        const XL6_AstNode* previousArgs = ctx->currentGenericArgs;

        if ( instance->enumNode != NULL &&
             instance->typeNode != NULL &&
             instance->typeNode->childCount > 0 ) {
            ctx->currentGenericParams =
                xl6CgenEnumTypeParams(instance->enumNode);
            ctx->currentGenericArgs = instance->typeNode->children[0];
            xl6CgenDiscoverGenericClassInstancesInNode(
                ctx,
                instance->enumNode);
            ctx->currentGenericParams = previousParams;
            ctx->currentGenericArgs = previousArgs;
        }
        scanned += 1;
    }
}

static void xl6CgenCollectRuntimeTypeDescsInNode(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return;
    }
    if ( (node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC) && xl6CgenFuncIsGeneric(node) && ctx->currentGenericArgs == NULL ) {
        return;
    }
    /* 开放泛型 class 没有可注册的运行时类型，只收集其具体实例。 */
    if ( node->kind == XL6_AST_CLASS && xl6CgenClassHasTypeParams(node) && ctx->currentGenericArgs == NULL ) {
        return;
    }
    if ( node->kind == XL6_AST_TYPE ) {
        xl6CgenAddRuntimeTypeDesc(ctx, node);
    }
    /*
        容器字面量可以嵌套在调用参数、条件和成员表达式中。
        字面量自身足以推导存储类型，不依赖尚未建立的局部变量表。
    */
    if ( node->kind == XL6_AST_ARRAY_LITERAL ||
         node->kind == XL6_AST_LIST_LITERAL ||
         node->kind == XL6_AST_SET_LITERAL ||
         node->kind == XL6_AST_DICT_LITERAL ||
         node->kind == XL6_AST_TYPED_LITERAL ) {
        XL6_AstNode* inferredType = xl6CgenCloneExprTypeForInference(ctx, node);

        xl6CgenAddRuntimeTypeDesc(ctx, inferredType);
        xl6AstFree(inferredType);
    }
    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        const XL6_AstNode* typeNode = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* initExpr = node->childCount > 1 ? node->children[1] : NULL;
        if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") && initExpr != NULL ) {
            XL6_AstNode* inferredType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
            xl6CgenAddRuntimeTypeDesc(ctx, inferredType);
            xl6AstFree(inferredType);
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenCollectRuntimeTypeDescsInNode(ctx, node->children[i]);
    }
}

static void xl6CgenCollectRuntimeTypeDescs(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    size_t i;

    if ( ctx == NULL ) {
        return;
    }

    xl6CgenCollectRuntimeTypeDescsInNode(ctx, root);
    for ( i = 0; i < ctx->genericInstanceCount; ++i ) {
        prevParams = ctx->currentGenericParams;
        prevArgs = ctx->currentGenericArgs;
        ctx->currentGenericParams = xl6CgenFuncTypeParams(ctx->genericInstances[i].func);
        ctx->currentGenericArgs = ctx->genericInstances[i].typeArgs;
        xl6CgenCollectRuntimeTypeDescsInNode(ctx, ctx->genericInstances[i].func);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
    }
    for ( i = 0; i < ctx->genericClassInstanceCount; ++i ) {
        const XL6_CgenGenericClassInstance* instance = &ctx->genericClassInstances[i];
        const XL6_AstNode* typeNode = instance->typeNode;

        if ( instance->classNode == NULL || typeNode == NULL || typeNode->childCount == 0 ) {
            continue;
        }
        prevParams = ctx->currentGenericParams;
        prevArgs = ctx->currentGenericArgs;
        ctx->currentGenericParams = xl6CgenClassTypeParams(instance->classNode);
        ctx->currentGenericArgs = typeNode->children[0];
        xl6CgenCollectRuntimeTypeDescsInNode(ctx, instance->classNode);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
    }
}
