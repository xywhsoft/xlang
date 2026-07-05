static int xl6CgenLiteralMatchesType(const XL6_AstNode* expr, const XL6_AstNode* typeNode)
{
    if ( expr == NULL || typeNode == NULL || typeNode->text == NULL ) {
        return 1;
    }
    switch ( expr->kind ) {
    case XL6_AST_INT:
        return xl6CTextEquals(typeNode->text, "int") || xl6CTextEquals(typeNode->text, "int64") ||
               xl6CTextEquals(typeNode->text, "int32");
    case XL6_AST_FLOAT:
        return xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float64") ||
               xl6CTextEquals(typeNode->text, "float32");
    case XL6_AST_STRING:
    case XL6_AST_INTERP_STRING:
        return xl6CTextEquals(typeNode->text, "string");
    case XL6_AST_TIME:
        return xl6CTextEquals(typeNode->text, "time");
    case XL6_AST_BOOL:
        return xl6CTextEquals(typeNode->text, "bool");
    default:
        return 1;
    }
}

static int xl6CgenExprParamMatchCost(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode);

static int xl6CgenExprMatchesParamType(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode)
{
    return xl6CgenExprParamMatchCost(ctx, expr, typeNode) >= 0;
}

static int xl6CgenExprParamMatchCost(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode)
{
    XL6_AstNode* exprType;
    const XL6_AstNode* paramType;
    const char* exprTypeName;
    const char* targetType;
    int result;

    if ( expr == NULL || typeNode == NULL ) {
        return 0;
    }

    paramType = xl6CgenSubstituteGenericType(ctx, typeNode);
    targetType = paramType != NULL ? paramType->text : NULL;
    if ( targetType == NULL ) {
        return 0;
    }

    exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( exprType != NULL ) {
        exprTypeName = exprType->text;
        if ( xl6CgenTypeNodeEqual(exprType, paramType) ) {
            xl6AstFree(exprType);
            return 0;
        }
        if ( exprTypeName != NULL && xl6CTextEquals(exprTypeName, targetType) &&
             exprType->childCount == 0 && paramType->childCount == 0 ) {
            xl6AstFree(exprType);
            return 0;
        }
        if ( exprTypeName != NULL && xl6CTextEquals(exprTypeName, targetType) &&
             paramType->childCount == 0 &&
             ((xl6CgenIsClassType(ctx, exprType) && xl6CgenIsClassType(ctx, paramType)) ||
              (xl6CgenIsNativeBackedClassType(ctx, exprType) && xl6CgenIsNativeBackedClassType(ctx, paramType)) ||
              (xl6CgenIsUnionType(ctx, exprType) && xl6CgenIsUnionType(ctx, paramType))) ) {
            xl6AstFree(exprType);
            return 0;
        }
        if ( exprTypeName != NULL &&
             exprType->childCount > 0 &&
             paramType->childCount == 0 &&
             ((xl6CTextEquals(exprTypeName, "array") && xl6CgenIsArrayXValueType(paramType)) ||
              (xl6CTextEquals(exprTypeName, "list") && xl6CgenIsListXValueType(paramType)) ||
              (xl6CTextEquals(exprTypeName, "set") && xl6CgenIsSetXValueType(paramType)) ||
              (xl6CTextEquals(exprTypeName, "dict") && xl6CgenIsDictXValueType(paramType))) ) {
            xl6AstFree(exprType);
            return 20;
        }
        if ( xl6CTextEquals(targetType, "var") ) {
            xl6AstFree(exprType);
            return 80;
        }
        if ( xl6CgenCanSafeWidenTypeName(exprTypeName, targetType) ) {
            result = (xl6CgenIsFloatTypeName(targetType) &&
                      (xl6CgenIsIntegerTypeName(exprTypeName) || xl6CgenIsBoolTypeName(exprTypeName))) ? 20 : 10;
            xl6AstFree(exprType);
            return result;
        }
        xl6AstFree(exprType);
        return -1;
    }

    if ( xl6CTextEquals(targetType, "var") ) {
        return 80;
    }
    if ( xl6CgenExprIntegerLiteralFitsType(expr, targetType) ) {
        return xl6CTextEquals(targetType, "int") ? 0 : 1;
    }
    if ( expr->kind == XL6_AST_FLOAT && xl6CgenIsFloatTypeName(targetType) ) {
        return xl6CTextEquals(targetType, "float") ? 0 : 2;
    }
    return xl6CgenLiteralMatchesType(expr, paramType) ? 0 : -1;
}

static void xl6CgenFreeInferredTypes(XL6_AstNode** inferredTypes, size_t count)
{
    size_t i;

    if ( inferredTypes == NULL ) {
        return;
    }
    for ( i = 0; i < count; ++i ) {
        xl6AstFree(inferredTypes[i]);
    }
    xrtFree(inferredTypes);
}

static int xl6CgenExprGenericParamMatchCost(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferredTypes)
{
    XL6_AstNode* exprType;
    int index;
    int ok;

    if ( typeNode == NULL ) {
        return 0;
    }
    if ( typeParams != NULL &&
         typeParams->childCount > 0 &&
         xl6CgenTypeContainsTypeParam(typeParams, typeNode) ) {
        exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
        if ( exprType == NULL ) {
            return xl6CgenExprParamMatchCost(ctx, expr, typeNode);
        }
        ok = xl6CgenInferGenericTypeFromTypes(ctx, typeParams, inferredTypes, typeNode, exprType, 0, expr);
        xl6AstFree(exprType);
        if ( ok ) {
            return 40;
        }

        index = xl6CgenTypeParamIndex(typeParams, typeNode->text);
        if ( index < 0 && !xl6CTextEquals(typeNode->text, "function") ) {
            return xl6CgenExprParamMatchCost(ctx, expr, typeNode);
        }
        return -1;
    }
    return xl6CgenExprParamMatchCost(ctx, expr, typeNode);
}

static XL6_AstNode* xl6CgenTakeInferredTypeArgs(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeParams,
    XL6_AstNode** inferredTypes,
    const XL6_AstNode* at)
{
    XL6_AstNode* result;
    size_t i;

    if ( typeParams == NULL || typeParams->childCount == 0 || inferredTypes == NULL ) {
        return NULL;
    }
    result = xl6AstNew(XL6_AST_LIST, NULL);
    if ( result == NULL ) {
        xl6CgenError(ctx, at, "泛型类型推断内存不足");
        return NULL;
    }
    for ( i = 0; i < typeParams->childCount; ++i ) {
        if ( inferredTypes[i] == NULL ) {
            xl6AstFree(result);
            return NULL;
        }
        xl6AstAddChild(result, inferredTypes[i]);
        inferredTypes[i] = NULL;
    }
    return result;
}

static int xl6CgenFuncMatchesCallEx(
    XL6_CgenContext* ctx,
    const XL6_AstNode* funcNode,
    const XL6_AstNode* args,
    int* outCost,
    XL6_AstNode** outTypeArgs)
{
    const XL6_AstNode* params;
    const XL6_AstNode* typeParams;
    XL6_AstNode** inferredTypes;
    int fixedCount;
    int hasArrayVararg;
    int hasDictVararg;
    int positionalIndex;
    int seenNamed;
    int* provided;
    size_t i;
    int ok;
    int cost;

    if ( outTypeArgs != NULL ) {
        *outTypeArgs = NULL;
    }
    params = xl6CgenCallableParams(funcNode);
    typeParams = xl6CgenFuncTypeParams(funcNode);
    fixedCount = xl6CgenFixedParamCount(params);
    hasArrayVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6CgenParamIsArrayVararg(params->children[fixedCount]);
    hasDictVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6CgenParamIsDictVararg(params->children[fixedCount]);
    provided = fixedCount > 0 ? (int*)xrtCalloc((size_t)fixedCount, sizeof(int)) : NULL;
    inferredTypes = typeParams != NULL && typeParams->childCount > 0 ?
                    (XL6_AstNode**)xrtCalloc(typeParams->childCount, sizeof(XL6_AstNode*)) : NULL;
    positionalIndex = 0;
    seenNamed = 0;
    ok = 1;
    cost = 0;

    if ( fixedCount > 0 && provided == NULL ) {
        xl6CgenFreeInferredTypes(inferredTypes, typeParams != NULL ? typeParams->childCount : 0);
        return 0;
    }
    if ( typeParams != NULL && typeParams->childCount > 0 && inferredTypes == NULL ) {
        xrtFree(provided);
        return 0;
    }

    if ( args != NULL ) {
        for ( i = 0; i < args->childCount && ok; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg == NULL ) {
                continue;
            }
            if ( arg->kind == XL6_AST_NAMED_ARG ) {
                int index;
                seenNamed = 1;
                index = xl6CgenFindParamIndex(params, fixedCount, arg->text);
                if ( index >= 0 ) {
                    const XL6_AstNode* param = params->children[index];
                    const XL6_AstNode* expr = xl6CgenArgExpr(arg);
                    int argCost;
                    if ( provided[index] ) {
                        ok = 0;
                    }
                    argCost = expr != NULL && param->childCount > 0 ?
                              xl6CgenExprGenericParamMatchCost(ctx, expr, param->children[0], typeParams, inferredTypes) : 0;
                    if ( argCost < 0 ) {
                        ok = 0;
                    } else {
                        cost += argCost;
                    }
                    provided[index] = 1;
                } else if ( !hasDictVararg ) {
                    ok = 0;
                } else {
                    cost += 200;
                }
            } else {
                if ( seenNamed ) {
                    ok = 0;
                } else if ( positionalIndex < fixedCount ) {
                    const XL6_AstNode* param = params->children[positionalIndex];
                    const XL6_AstNode* expr = xl6CgenArgExpr(arg);
                    int argCost;
                    if ( provided[positionalIndex] ) {
                        ok = 0;
                    }
                    argCost = expr != NULL && param->childCount > 0 ?
                              xl6CgenExprGenericParamMatchCost(ctx, expr, param->children[0], typeParams, inferredTypes) : 0;
                    if ( argCost < 0 ) {
                        ok = 0;
                    } else {
                        cost += argCost;
                    }
                    provided[positionalIndex] = 1;
                    positionalIndex += 1;
                } else if ( !hasArrayVararg ) {
                    ok = 0;
                } else {
                    cost += 200;
                }
            }
        }
    }

    for ( i = 0; ok && i < (size_t)fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( !provided[i] && param->childCount < 2 ) {
            ok = 0;
        } else if ( !provided[i] ) {
            cost += 100;
        }
    }

    if ( ok && typeParams != NULL && typeParams->childCount > 0 ) {
        for ( i = 0; i < typeParams->childCount; ++i ) {
            if ( inferredTypes[i] == NULL ) {
                ok = 0;
                break;
            }
        }
    }

    if ( ok && outTypeArgs != NULL && typeParams != NULL && typeParams->childCount > 0 ) {
        *outTypeArgs = xl6CgenTakeInferredTypeArgs(ctx, typeParams, inferredTypes, funcNode);
        if ( *outTypeArgs == NULL ) {
            ok = 0;
        }
    }

    xrtFree(provided);
    xl6CgenFreeInferredTypes(inferredTypes, typeParams != NULL ? typeParams->childCount : 0);
    if ( ok && outCost != NULL ) {
        *outCost = cost;
    }
    return ok;
}

static int xl6CgenFuncMatchesCall(XL6_CgenContext* ctx, const XL6_AstNode* funcNode, const XL6_AstNode* args, int* outCost)
{
    return xl6CgenFuncMatchesCallEx(ctx, funcNode, args, outCost, NULL);
}

static int xl6CgenExprExplicitGenericParamMatchCost(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* typeNode)
{
    XL6_AstNode* concreteType;
    int cost;

    concreteType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
    cost = xl6CgenExprParamMatchCost(ctx, expr, concreteType != NULL ? concreteType : typeNode);
    xl6AstFree(concreteType);
    return cost;
}

static int xl6CgenFuncMatchesExplicitGenericCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* funcNode,
    const XL6_AstNode* args,
    const XL6_AstNode* typeArgs,
    int* outCost)
{
    const XL6_AstNode* params;
    const XL6_AstNode* typeParams;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    int fixedCount;
    int hasArrayVararg;
    int hasDictVararg;
    int positionalIndex;
    int seenNamed;
    int* provided;
    size_t i;
    int ok;
    int cost;

    typeParams = xl6CgenFuncTypeParams(funcNode);
    if ( typeParams == NULL || typeParams->childCount == 0 ||
         typeArgs == NULL || typeParams->childCount != typeArgs->childCount ) {
        return 0;
    }

    params = xl6CgenCallableParams(funcNode);
    fixedCount = xl6CgenFixedParamCount(params);
    hasArrayVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6CgenParamIsArrayVararg(params->children[fixedCount]);
    hasDictVararg = params != NULL && params->childCount > (size_t)fixedCount && xl6CgenParamIsDictVararg(params->children[fixedCount]);
    provided = fixedCount > 0 ? (int*)xrtCalloc((size_t)fixedCount, sizeof(int)) : NULL;
    positionalIndex = 0;
    seenNamed = 0;
    ok = 1;
    cost = 40;

    if ( fixedCount > 0 && provided == NULL ) {
        return 0;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = typeParams;
    ctx->currentGenericArgs = typeArgs;

    if ( args != NULL ) {
        for ( i = 0; i < args->childCount && ok; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg == NULL ) {
                continue;
            }
            if ( arg->kind == XL6_AST_NAMED_ARG ) {
                int index;
                seenNamed = 1;
                index = xl6CgenFindParamIndex(params, fixedCount, arg->text);
                if ( index >= 0 ) {
                    const XL6_AstNode* param = params->children[index];
                    const XL6_AstNode* expr = xl6CgenArgExpr(arg);
                    int argCost;
                    if ( provided[index] ) {
                        ok = 0;
                    }
                    argCost = expr != NULL && param->childCount > 0 ?
                              xl6CgenExprExplicitGenericParamMatchCost(ctx, expr, param->children[0]) : 0;
                    if ( argCost < 0 ) {
                        ok = 0;
                    } else {
                        cost += argCost;
                    }
                    provided[index] = 1;
                } else if ( !hasDictVararg ) {
                    ok = 0;
                } else {
                    cost += 200;
                }
            } else {
                if ( seenNamed ) {
                    ok = 0;
                } else if ( positionalIndex < fixedCount ) {
                    const XL6_AstNode* param = params->children[positionalIndex];
                    const XL6_AstNode* expr = xl6CgenArgExpr(arg);
                    int argCost;
                    if ( provided[positionalIndex] ) {
                        ok = 0;
                    }
                    argCost = expr != NULL && param->childCount > 0 ?
                              xl6CgenExprExplicitGenericParamMatchCost(ctx, expr, param->children[0]) : 0;
                    if ( argCost < 0 ) {
                        ok = 0;
                    } else {
                        cost += argCost;
                    }
                    provided[positionalIndex] = 1;
                    positionalIndex += 1;
                } else if ( !hasArrayVararg ) {
                    ok = 0;
                } else {
                    cost += 200;
                }
            }
        }
    }

    for ( i = 0; ok && i < (size_t)fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( !provided[i] && param->childCount < 2 ) {
            ok = 0;
        } else if ( !provided[i] ) {
            cost += 100;
        }
    }

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    xrtFree(provided);
    if ( ok && outCost != NULL ) {
        *outCost = cost;
    }
    return ok;
}

static void xl6CgenSelectCallCandidate(
    XL6_AstNode** selected,
    XL6_AstNode** selectedArgs,
    int* bestCount,
    int* bestCost,
    int* bestIsGeneric,
    XL6_AstNode* candidate,
    XL6_AstNode* candidateArgs,
    int candidateCost)
{
    int candidateIsGeneric;

    candidateIsGeneric = xl6CgenFuncIsGeneric(candidate);
    if ( *bestCount == 0 ||
         candidateCost < *bestCost ||
         (candidateCost == *bestCost && *bestIsGeneric && !candidateIsGeneric) ) {
        xl6AstFree(*selectedArgs);
        *selected = candidate;
        *selectedArgs = candidateArgs;
        *bestCost = candidateCost;
        *bestCount = 1;
        *bestIsGeneric = candidateIsGeneric;
        return;
    }

    if ( candidateCost == *bestCost ) {
        if ( !*bestIsGeneric && candidateIsGeneric ) {
            xl6AstFree(candidateArgs);
            return;
        }
        *bestCount += 1;
    }
    xl6AstFree(candidateArgs);
}
