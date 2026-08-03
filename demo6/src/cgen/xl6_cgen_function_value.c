static int xl6CgenDigitValue(char ch)
{
    if ( ch >= '0' && ch <= '9' ) {
        return ch - '0';
    }
    if ( ch >= 'a' && ch <= 'f' ) {
        return ch - 'a' + 10;
    }
    if ( ch >= 'A' && ch <= 'F' ) {
        return ch - 'A' + 10;
    }
    return -1;
}

static void xl6CgenIntLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const char* p;
    unsigned long long value;
    int base;

    if ( node == NULL || node->text == NULL ) {
        xl6OutputPuts("0", ctx->out);
        return;
    }

    p = node->text;
    base = 10;
    if ( p[0] == '0' && p[1] == 'x' ) {
        base = 16;
        p += 2;
    } else if ( p[0] == '0' && p[1] == 'o' ) {
        base = 8;
        p += 2;
    } else if ( p[0] == '0' && p[1] == 'b' ) {
        base = 2;
        p += 2;
    }

    value = 0;
    while ( *p != '\0' ) {
        int digit;

        if ( *p == '_' ) {
            ++p;
            continue;
        }
        digit = xl6CgenDigitValue(*p);
        if ( digit < 0 || digit >= base ||
             value > (ULLONG_MAX - (unsigned long long)digit) / (unsigned long long)base ) {
            xl6CgenError(ctx, node, "integer literal is too large or invalid");
            xl6OutputPuts("0", ctx->out);
            return;
        }
        value = value * (unsigned long long)base + (unsigned long long)digit;
        ++p;
    }

    xl6OutputPrintf(ctx->out, value > LLONG_MAX ? "%lluULL" : "%lluLL", value);
}

static void xl6CgenFloatLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const char* p;

    (void)ctx;
    if ( node == NULL || node->text == NULL ) {
        xl6OutputPuts("0.0", ctx->out);
        return;
    }
    for ( p = node->text; *p != '\0'; ++p ) {
        if ( *p != '_' ) {
            xl6OutputPutc((int)*p, ctx->out);
        }
    }
}

static int xl6CgenIntLiteralLevel(const XL6_AstNode* node)
{
    const char* p;
    int base;
    int value;

    if ( node == NULL || node->kind != XL6_AST_INT || node->text == NULL ) {
        return 1;
    }

    p = node->text;
    base = 10;
    if ( p[0] == '0' && p[1] == 'x' ) {
        base = 16;
        p += 2;
    } else if ( p[0] == '0' && p[1] == 'o' ) {
        base = 8;
        p += 2;
    } else if ( p[0] == '0' && p[1] == 'b' ) {
        base = 2;
        p += 2;
    }

    value = 0;
    while ( *p != '\0' ) {
        int digit;

        if ( *p == '_' ) {
            ++p;
            continue;
        }
        digit = xl6CgenDigitValue(*p);
        if ( digit < 0 || digit >= base || value > (INT_MAX - digit) / base ) {
            return INT_MAX;
        }
        value = value * base + digit;
        ++p;
    }
    return value;
}

static void xl6CgenLoopJump(XL6_CgenContext* ctx, const XL6_AstNode* node, int isContinue)
{
    int level;
    XL6_CgenLoopTarget* target;
    XL6_CgenFinallyTarget* finallyTarget;
    const XL6_HirNode* hirNode;
    size_t targetIndex;

    level = node != NULL && node->childCount > 0 ? xl6CgenIntLiteralLevel(node->children[0]) : 1;
    target = xl6CgenLoopTargetAtLevel(ctx, level);
    if ( target == NULL ) {
        xl6CgenError(ctx, node, isContinue ? "continue 层数超过当前循环深度" : "break 层数超过当前循环深度");
        return;
    }
    hirNode = xl6CgenHirNode(ctx, node);
    if ( hirNode == NULL ||
         hirNode->exitKind != (isContinue
             ? XL6_SEM_EXIT_CONTINUE
             : XL6_SEM_EXIT_BREAK) ) {
        xl6CgenError(ctx, node, "typed HIR 缺少循环退出计划");
        return;
    }
    if ( hirNode->targetRegion == 0 ||
         target->region == 0 ||
         hirNode->targetRegion != target->region ) {
        xl6CgenError(ctx, node, "typed HIR loop target region does not match cgen loop stack");
        return;
    }

    finallyTarget = xl6CgenCurrentFinallyTarget(ctx);
    targetIndex = (size_t)(target - ctx->loopTargets);
    if ( finallyTarget != NULL && targetIndex < finallyTarget->loopTargetCount ) {
        xl6CgenEmitCleanupsSince(
            ctx,
            finallyTarget->cleanupMark);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (__xl6_pending_kind_%d == 1) {\n", finallyTarget->id);
        ctx->indent += 1;
        xl6CgenDiscardPendingFinallyReturn(ctx, finallyTarget->id);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "__xl6_pending_kind_%d = %d;\n",
            finallyTarget->id,
            isContinue ? 3 : 2);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "__xl6_pending_target_%d = %d;\n",
            finallyTarget->id,
            isContinue ? target->continueId : target->breakId);
        xl6CgenIndent(ctx);
        if ( finallyTarget->inFinally ) {
            xl6OutputPrintf(ctx->out, "goto __xl6_try_finally_dispatch_%d;\n", finallyTarget->id);
        } else {
            xl6OutputPrintf(ctx->out, "goto __xl6_try_finally_%d;\n", finallyTarget->id);
        }
        return;
    }

    xl6CgenEmitCleanupsSince(
        ctx,
        isContinue ? target->continueCleanupMark : target->breakCleanupMark
    );
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "goto __xl6_%s_%d;\n", isContinue ? "continue" : "break", isContinue ? target->continueId : target->breakId);
}

static XL6_AstNode* xl6CgenFindExternFunc(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( node->kind == XL6_AST_EXTERN_FUNC && node->childCount > 0 &&
             node->children[0] != NULL && xl6CTextEquals(node->children[0]->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static XL6_AstNode* xl6CgenFindExportFunc(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( node->kind == XL6_AST_EXPORT_FUNC && node->childCount > 0 &&
             node->children[0] != NULL && xl6CTextEquals(node->children[0]->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static void xl6CgenCallArgs(XL6_CgenContext* ctx, const XL6_AstNode* args)
{
    size_t i;

    if ( args == NULL ) {
        return;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        if ( arg->kind == XL6_AST_NAMED_ARG ) {
            xl6CgenError(ctx, arg, "当前 C 生成暂不支持命名参数调用");
            xl6OutputPuts("0", ctx->out);
        } else if ( arg->childCount > 0 ) {
            xl6CgenExpr(ctx, arg->children[0]);
        }
    }
}

static const XL6_AstNode* xl6CgenFunctionParamTypes(const XL6_AstNode* functionType)
{
    return functionType != NULL && functionType->childCount > 0 ? functionType->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenFunctionReturnTypes(const XL6_AstNode* functionType)
{
    return functionType != NULL && functionType->childCount > 1 ? functionType->children[1] : NULL;
}

static int xl6CgenFunctionTypeParamIsArrayVararg(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && xl6CTextEquals(typeNode->text, "array<var>");
}

static int xl6CgenFunctionTypeParamIsDictVararg(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && xl6CTextEquals(typeNode->text, "dict<var>");
}

static size_t xl6CgenFunctionTypeFixedParamCount(const XL6_AstNode* paramTypes)
{
    size_t count;
    const XL6_AstNode* last;

    count = paramTypes != NULL ? paramTypes->childCount : 0;
    if ( count == 0 ) {
        return 0;
    }
    last = paramTypes->children[count - 1];
    if ( xl6CgenFunctionTypeParamIsArrayVararg(last) ||
         xl6CgenFunctionTypeParamIsDictVararg(last) ) {
        return count - 1;
    }
    return count;
}

static size_t xl6CgenFunctionValuePositionalArgCount(const XL6_AstNode* args)
{
    size_t i;
    size_t count;

    count = 0;
    if ( args == NULL ) {
        return 0;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg != NULL && arg->kind != XL6_AST_NAMED_ARG ) {
            count += 1;
        }
    }
    return count;
}

static const XL6_AstNode* xl6CgenFunctionValuePositionalArg(const XL6_AstNode* args, size_t index)
{
    size_t i;
    size_t seen;

    seen = 0;
    if ( args == NULL ) {
        return NULL;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg == NULL || arg->kind == XL6_AST_NAMED_ARG ) {
            continue;
        }
        if ( seen == index ) {
            return arg;
        }
        seen += 1;
    }
    return NULL;
}

static int xl6CgenFunctionValueDictVarargCount(const XL6_AstNode* args)
{
    size_t i;
    int count;

    count = 0;
    if ( args == NULL ) {
        return 0;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG ) {
            count += 1;
        }
    }
    return count;
}

static void xl6CgenFunctionValueDictVarargExpr(XL6_CgenContext* ctx, const XL6_AstNode* args)
{
    size_t i;
    int count;

    count = xl6CgenFunctionValueDictVarargCount(args);
    xl6OutputPrintf(ctx->out, "xl6_xrt_dict_varargs(%d", count);
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg == NULL || arg->kind != XL6_AST_NAMED_ARG ) {
                continue;
            }
            xl6OutputPuts(", ", ctx->out);
            xl6CgenCStringLiteral(ctx->out, arg->text);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenXValueFromExpr(ctx, xl6CgenArgExpr(arg));
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static const XL6_AstNode* xl6CgenCallableItemType(const XL6_AstNode* item)
{
    if ( item != NULL && item->kind == XL6_AST_PARAM && item->childCount > 0 ) {
        return item->children[0];
    }
    return item;
}

static int xl6CgenCallableTypeListEqual(XL6_CgenContext* ctx, const XL6_AstNode* left, const XL6_AstNode* right)
{
    size_t i;

    if ( left == NULL || right == NULL ) {
        return left == right;
    }
    if ( left->childCount != right->childCount ) {
        return 0;
    }
    for ( i = 0; i < left->childCount; ++i ) {
        const XL6_AstNode* leftType = xl6CgenResolveAliasType(ctx, xl6CgenCallableItemType(left->children[i]));
        const XL6_AstNode* rightType = xl6CgenResolveAliasType(ctx, xl6CgenCallableItemType(right->children[i]));
        if ( !xl6CgenTypeNodeEqual(leftType, rightType) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6CgenCallableListLooksLikeParams(const XL6_AstNode* list)
{
    return list != NULL && list->childCount > 0 &&
           list->children[0] != NULL && list->children[0]->kind == XL6_AST_PARAM;
}

static int xl6CgenCallableParamPrefixMatches(XL6_CgenContext* ctx, const XL6_AstNode* expected, const XL6_AstNode* params, size_t count)
{
    size_t i;

    if ( (count > 0) && (expected == NULL || params == NULL) ) {
        return 0;
    }
    if ( expected != NULL && expected->childCount < count ) {
        return 0;
    }
    if ( params != NULL && params->childCount < count ) {
        return 0;
    }
    for ( i = 0; i < count; ++i ) {
        const XL6_AstNode* expectedType = xl6CgenResolveAliasType(ctx, xl6CgenCallableItemType(expected->children[i]));
        const XL6_AstNode* actualType = xl6CgenResolveAliasType(ctx, xl6CgenCallableItemType(params->children[i]));
        if ( !xl6CgenTypeNodeEqual(expectedType, actualType) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6CgenCallableParamListMatchesFunctionParams(XL6_CgenContext* ctx, const XL6_AstNode* expected, const XL6_AstNode* params)
{
    size_t expectedCount;
    size_t expectedFixed;
    int expectedHasArrayVararg;
    int expectedHasDictVararg;
    int actualHasVararg;
    int actualFixed;
    int actualRequired;

    if ( !xl6CgenCallableListLooksLikeParams(params) ) {
        return xl6CgenCallableTypeListEqual(ctx, expected, params);
    }

    expectedCount = expected != NULL ? expected->childCount : 0;
    expectedHasArrayVararg = expectedCount > 0 && xl6CgenFunctionTypeParamIsArrayVararg(expected->children[expectedCount - 1]);
    expectedHasDictVararg = expectedCount > 0 && xl6CgenFunctionTypeParamIsDictVararg(expected->children[expectedCount - 1]);
    expectedFixed = (expectedHasArrayVararg || expectedHasDictVararg) ? expectedCount - 1 : expectedCount;
    actualFixed = xl6CgenFixedParamCount(params);
    actualRequired = xl6CgenParamRequiredCount(params);
    actualHasVararg = params != NULL && params->childCount > (size_t)actualFixed;

    if ( expectedHasArrayVararg || expectedHasDictVararg ) {
        const XL6_AstNode* actualVararg;

        if ( !actualHasVararg || expectedFixed < (size_t)actualRequired || expectedFixed > (size_t)actualFixed ) {
            return 0;
        }
        actualVararg = params->children[actualFixed];
        if ( expectedHasArrayVararg && !xl6CgenParamIsArrayVararg(actualVararg) ) {
            return 0;
        }
        if ( expectedHasDictVararg && !xl6CgenParamIsDictVararg(actualVararg) ) {
            return 0;
        }
        return xl6CgenCallableParamPrefixMatches(ctx, expected, params, expectedFixed);
    }

    if ( actualHasVararg || expectedFixed < (size_t)actualRequired || expectedFixed > (size_t)actualFixed ) {
        return 0;
    }
    return xl6CgenCallableParamPrefixMatches(ctx, expected, params, expectedFixed);
}

static int xl6CgenFunctionTypeMatchesFunc(XL6_CgenContext* ctx, const XL6_AstNode* functionType, const XL6_AstNode* func)
{
    const XL6_AstNode* expectedParams;
    const XL6_AstNode* expectedReturns;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    if ( functionType == NULL || !xl6CTextEquals(functionType->text, "function") || func == NULL ) {
        return 0;
    }
    if ( functionType->childCount == 0 ) {
        return 1;
    }
    if ( functionType->childCount < 2 ) {
        return 0;
    }
    expectedParams = functionType->children[0];
    expectedReturns = functionType->children[1];
    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    return xl6CgenCallableParamListMatchesFunctionParams(ctx, expectedParams, params) &&
           xl6CgenCallableTypeListEqual(ctx, expectedReturns, returns);
}

static XL6_AstNode* xl6CgenFindFunctionValueFunc(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* functionType, const XL6_AstNode* atNode)
{
    XL6_AstNode* selected;
    int candidateCount;
    int matchCount;
    size_t i;

    selected = NULL;
    candidateCount = 0;
    matchCount = 0;
    if ( ctx == NULL || ctx->root == NULL || name == NULL || functionType == NULL ||
         !xl6CTextEquals(functionType->text, "function") ) {
        return NULL;
    }

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* func = ctx->root->children[i];
        XL6_AstNode* nameNode;
        if ( func == NULL ||
             (func->kind != XL6_AST_FUNC && func->kind != XL6_AST_EXTERN_FUNC && func->kind != XL6_AST_EXPORT_FUNC) ||
             xl6CgenFuncIsGeneric(func) ||
             func->childCount == 0 ) {
            continue;
        }
        nameNode = func->children[0];
        if ( nameNode == NULL || !xl6CTextEquals(nameNode->text, name) ) {
            continue;
        }
        candidateCount += 1;
        if ( xl6CgenFunctionTypeMatchesFunc(ctx, functionType, func) ) {
            selected = func;
            matchCount += 1;
        }
    }

    if ( matchCount == 1 ) {
        return selected;
    }
    if ( candidateCount == 0 ) {
        xl6CgenError(ctx, atNode, "找不到可作为 function 值的具名函数");
    } else if ( functionType->childCount == 0 ) {
        xl6CgenError(ctx, atNode, "bare function value target is ambiguous; add an explicit function signature");
    } else if ( matchCount == 0 ) {
        xl6CgenError(ctx, atNode, "function value signature does not match any function overload");
    } else {
        xl6CgenError(ctx, atNode, "function value signature matches multiple overloads");
    }
    return NULL;
}

static XL6_AstNode* xl6CgenFindUniqueBareFunctionValueFunc(
    XL6_CgenContext* ctx,
    const char* name,
    int* outCandidateCount)
{
    XL6_AstNode* selected;
    size_t i;
    int candidateCount;

    selected = NULL;
    candidateCount = 0;
    if ( ctx != NULL && ctx->root != NULL && name != NULL ) {
        for ( i = 0; i < ctx->root->childCount; ++i ) {
            XL6_AstNode* func = ctx->root->children[i];

            if ( func == NULL ||
                 (func->kind != XL6_AST_FUNC &&
                  func->kind != XL6_AST_EXTERN_FUNC &&
                  func->kind != XL6_AST_EXPORT_FUNC) ||
                 xl6CgenFuncIsGeneric(func) ||
                 func->childCount == 0 ||
                 func->children[0] == NULL ||
                 !xl6CTextEquals(func->children[0]->text, name) ) {
                continue;
            }
            selected = func;
            candidateCount += 1;
        }
    }
    if ( outCandidateCount != NULL ) {
        *outCandidateCount = candidateCount;
    }
    return candidateCount == 1 ? selected : NULL;
}

static void xl6CgenFunctionValueCName(XL6_CgenContext* ctx, const char* name, char* cName, size_t cNameSize)
{
    char baseName[512];

    if ( cName == NULL || cNameSize == 0 ) {
        return;
    }
    if ( xl6CgenAnonCaptureIndex(ctx->currentAnonFunc, name) >= 0 ) {
        xl6CgenCIdentText(baseName, sizeof(baseName), name);
        snprintf(cName, cNameSize, "__env->%s", baseName);
    } else {
        xl6CgenValueStorageNameText(ctx, cName, cNameSize, name);
    }
}

static void xl6CgenBareFunctionInvoke(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* args)
{
    size_t i;
    char cName[512];

    xl6CgenFunctionValueCName(ctx, name, cName, sizeof(cName));
    xl6OutputPrintf(ctx->out, "xl6_function_call_var(%s, %u", cName, args != NULL ? (unsigned)args->childCount : 0);
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            xl6OutputPuts(", ", ctx->out);
            if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG ) {
                xl6CgenError(ctx, arg, "bare function call does not support named arguments");
                xl6OutputPuts("xvoCreateNull()", ctx->out);
            } else if ( arg != NULL && arg->childCount > 0 ) {
                xl6CgenXValueFromExpr(ctx, arg->children[0]);
            } else {
                xl6OutputPuts("xvoCreateNull()", ctx->out);
            }
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenFunctionInvokeDirectCall(XL6_CgenContext* ctx,
                                            const char* cName,
                                            const XL6_AstNode* paramTypes,
                                            const XL6_AstNode* returns,
                                            const XL6_AstNode* args,
                                            size_t fixedCount,
                                            int hasArrayVararg,
                                            int hasDictVararg)
{
    size_t i;
    int returnCount = returns != NULL ? (int)returns->childCount : 0;

    xl6OutputPuts("((", ctx->out);
    if ( returnCount > 1 ) {
        xl6CgenFunctionReturnStructName(ctx, paramTypes, returns);
    } else if ( returnCount == 1 ) {
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPuts(" (*)(void*", ctx->out);
    for ( i = 0; paramTypes != NULL && i < paramTypes->childCount; ++i ) {
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, paramTypes->children[i]), ctx->out);
        if ( xl6CgenParamUsesReferenceAbi(paramTypes->children[i]) ) {
            xl6OutputPutc('*', ctx->out);
        }
    }
    xl6OutputPrintf(ctx->out, "))%s.invoke)(%s.env", cName, cName);
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* arg = xl6CgenFunctionValuePositionalArg(args, i);
        xl6OutputPuts(", ", ctx->out);
        if ( arg != NULL && arg->childCount > 0 ) {
            if ( xl6CgenParamUsesReferenceAbi(paramTypes->children[i]) ) {
                xl6OutputPuts("&(", ctx->out);
                xl6CgenExpr(ctx, arg->children[0]);
                xl6OutputPutc(')', ctx->out);
            } else {
                xl6CgenFixedTypeArgExpr(ctx, paramTypes->children[i], arg->children[0]);
            }
        } else {
            xl6OutputPuts("0", ctx->out);
        }
    }
    if ( hasArrayVararg ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenArrayVarargExpr(ctx, args, (int)fixedCount);
    } else if ( hasDictVararg ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenFunctionValueDictVarargExpr(ctx, args);
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenFunctionInvokeDirectCallExpr(XL6_CgenContext* ctx,
                                                const XL6_AstNode* fnExpr,
                                                const XL6_AstNode* paramTypes,
                                                const XL6_AstNode* returns,
                                                const XL6_AstNode* args,
                                                size_t fixedCount,
                                                int hasArrayVararg,
                                                int hasDictVararg)
{
    size_t i;
    int returnCount = returns != NULL ? (int)returns->childCount : 0;

    xl6OutputPuts("((", ctx->out);
    if ( returnCount > 1 ) {
        xl6CgenFunctionReturnStructName(ctx, paramTypes, returns);
    } else if ( returnCount == 1 ) {
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPuts(" (*)(void*", ctx->out);
    for ( i = 0; paramTypes != NULL && i < paramTypes->childCount; ++i ) {
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, paramTypes->children[i]), ctx->out);
        if ( xl6CgenParamUsesReferenceAbi(paramTypes->children[i]) ) {
            xl6OutputPutc('*', ctx->out);
        }
    }
    xl6OutputPuts("))", ctx->out);
    xl6CgenExpr(ctx, fnExpr);
    xl6OutputPuts(".invoke)(", ctx->out);
    xl6CgenExpr(ctx, fnExpr);
    xl6OutputPuts(".env", ctx->out);
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* arg = xl6CgenFunctionValuePositionalArg(args, i);
        xl6OutputPuts(", ", ctx->out);
        if ( arg != NULL && arg->childCount > 0 ) {
            if ( xl6CgenParamUsesReferenceAbi(paramTypes->children[i]) ) {
                xl6OutputPuts("&(", ctx->out);
                xl6CgenExpr(ctx, arg->children[0]);
                xl6OutputPutc(')', ctx->out);
            } else {
                xl6CgenFixedTypeArgExpr(ctx, paramTypes->children[i], arg->children[0]);
            }
        } else {
            xl6OutputPuts("0", ctx->out);
        }
    }
    if ( hasArrayVararg ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenArrayVarargExpr(ctx, args, (int)fixedCount);
    } else if ( hasDictVararg ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenFunctionValueDictVarargExpr(ctx, args);
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenFunctionInvokeFallbackCall(XL6_CgenContext* ctx, const char* cName, const XL6_AstNode* args)
{
    size_t i;
    int namedCount = xl6CgenFunctionValueDictVarargCount(args);
    size_t positionalCount = xl6CgenFunctionValuePositionalArgCount(args);

    if ( namedCount > 0 ) {
        xl6OutputPrintf(ctx->out, "xl6_function_call_kwargs(%s, ", cName);
        xl6CgenFunctionValueDictVarargExpr(ctx, args);
        xl6OutputPrintf(ctx->out, ", %u", (unsigned)positionalCount);
    } else {
        xl6OutputPrintf(ctx->out, "xl6_function_call_var(%s, %u", cName, (unsigned)positionalCount);
    }
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG ) {
                continue;
            }
            xl6OutputPuts(", ", ctx->out);
            if ( arg != NULL && arg->childCount > 0 ) {
                xl6CgenXValueFromExpr(ctx, arg->children[0]);
            } else {
                xl6OutputPuts("xvoCreateNull()", ctx->out);
            }
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenFunctionDefaultReturnExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( concreteType == NULL || typeName == NULL ) {
        xl6OutputPuts("0", ctx->out);
    } else if ( xl6CgenIsTypedContainerType(ctx, concreteType) ) {
        xl6OutputPrintf(ctx->out, "((%s)0)", xl6CgenTypedContainerCType(ctx, concreteType));
    } else if ( xl6CgenIsXValueType(concreteType) ) {
        xl6OutputPuts("xvoCreateNull()", ctx->out);
    } else if ( xl6CgenIsFunctionType(concreteType) ||
                xl6CgenIsClassType(ctx, concreteType) ||
                xl6CgenIsPlainRecordType(ctx, concreteType) ||
                xl6CgenIsNativeBackedClassType(ctx, concreteType) ) {
        xl6OutputPrintf(ctx->out, "(%s){0}", xl6CgenTypeName(ctx, concreteType));
    } else if ( xl6CTextEquals(typeName, "string") ) {
        xl6OutputPuts("\"\"", ctx->out);
    } else if ( xl6CgenIsFloatTypeName(typeName) ) {
        xl6OutputPrintf(ctx->out, "((%s)0.0)", xl6CgenTypeName(ctx, concreteType));
    } else {
        xl6OutputPrintf(ctx->out, "((%s)0)", xl6CgenTypeName(ctx, concreteType));
    }
}

static void xl6CgenFunctionOwnedValueExpr(XL6_CgenContext* ctx, const XL6_AstNode* fnExpr, const XL6_AstNode* functionType)
{
    if ( xl6CgenExprIsFunctionBorrow(ctx, fnExpr) ) {
        xl6OutputPuts("xl6_function_copy_value(", ctx->out);
        xl6CgenFunctionValueExpr(ctx, fnExpr, functionType);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    xl6CgenFunctionValueExpr(ctx, fnExpr, functionType);
}

static void xl6CgenFunctionInvokeFallbackCallExpr(XL6_CgenContext* ctx,
                                                  const XL6_AstNode* fnExpr,
                                                  const XL6_AstNode* functionType,
                                                  const XL6_AstNode* args,
                                                  int releaseOwned)
{
    size_t i;
    int namedCount = xl6CgenFunctionValueDictVarargCount(args);
    size_t positionalCount = xl6CgenFunctionValuePositionalArgCount(args);

    if ( namedCount > 0 ) {
        xl6OutputPuts(releaseOwned ? "xl6_function_call_kwargs_release(" : "xl6_function_call_kwargs(", ctx->out);
        if ( releaseOwned ) {
            xl6CgenFunctionOwnedValueExpr(ctx, fnExpr, functionType);
        } else {
            xl6CgenExpr(ctx, fnExpr);
        }
        xl6OutputPuts(", ", ctx->out);
        xl6CgenFunctionValueDictVarargExpr(ctx, args);
        xl6OutputPrintf(ctx->out, ", %u", (unsigned)positionalCount);
    } else {
        xl6OutputPuts(releaseOwned ? "xl6_function_call_var_release(" : "xl6_function_call_var(", ctx->out);
        if ( releaseOwned ) {
            xl6CgenFunctionOwnedValueExpr(ctx, fnExpr, functionType);
        } else {
            xl6CgenExpr(ctx, fnExpr);
        }
        xl6OutputPrintf(ctx->out, ", %u", (unsigned)positionalCount);
    }
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG ) {
                continue;
            }
            xl6OutputPuts(", ", ctx->out);
            if ( arg != NULL && arg->childCount > 0 ) {
                xl6CgenXValueFromExpr(ctx, arg->children[0]);
            } else {
                xl6OutputPuts("xvoCreateNull()", ctx->out);
            }
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenFunctionInvokeFallbackExpr(XL6_CgenContext* ctx,
                                              const XL6_AstNode* returnType,
                                              const char* cName,
                                              const XL6_AstNode* args)
{
    const XL6_AstNode* concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, returnType));
    const char* typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CgenIsTypedContainerType(ctx, concreteType) ) {
        xl6OutputPuts("(xl6_function_fatal(3), ", ctx->out);
        xl6CgenFunctionDefaultReturnExpr(ctx, concreteType);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CgenIsXValueType(concreteType) ) {
        xl6CgenFunctionInvokeFallbackCall(ctx, cName, args);
    } else if ( xl6CgenIsFunctionType(concreteType) ) {
        xl6OutputPuts("xl6_function_from_xvalue_release(", ctx->out);
        xl6CgenFunctionInvokeFallbackCall(ctx, cName, args);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CgenIsIntegerTypeName(typeName) ) {
        xl6OutputPrintf(ctx->out, "((%s)xl6_xvalue_to_i64_release(", xl6CgenTypeName(ctx, concreteType));
        xl6CgenFunctionInvokeFallbackCall(ctx, cName, args);
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsFloatTypeName(typeName) ) {
        xl6OutputPrintf(ctx->out, "((%s)xl6_xvalue_to_f64_release(", xl6CgenTypeName(ctx, concreteType));
        xl6CgenFunctionInvokeFallbackCall(ctx, cName, args);
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsBoolTypeName(typeName) ) {
        xl6OutputPuts("xl6_xvalue_to_bool_release(", ctx->out);
        xl6CgenFunctionInvokeFallbackCall(ctx, cName, args);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "string") ) {
        xl6OutputPuts("xl6_xvalue_to_string_release(", ctx->out);
        xl6CgenFunctionInvokeFallbackCall(ctx, cName, args);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "time") ) {
        xl6OutputPuts("xl6_xvalue_to_time_release(", ctx->out);
        xl6CgenFunctionInvokeFallbackCall(ctx, cName, args);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "point") ) {
        xl6OutputPuts("xl6_xvalue_to_point_release(", ctx->out);
        xl6CgenFunctionInvokeFallbackCall(ctx, cName, args);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "type") ) {
        xl6OutputPuts("((unsigned long long)xl6_xvalue_to_i64_release(", ctx->out);
        xl6CgenFunctionInvokeFallbackCall(ctx, cName, args);
        xl6OutputPuts("))", ctx->out);
    } else {
        xl6OutputPuts("(xl6_function_fatal(3), ", ctx->out);
        xl6CgenFunctionDefaultReturnExpr(ctx, concreteType);
        xl6OutputPutc(')', ctx->out);
    }
}

static void xl6CgenFunctionInvokeFallbackExprForExpr(XL6_CgenContext* ctx,
                                                     const XL6_AstNode* returnType,
                                                     const XL6_AstNode* fnExpr,
                                                     const XL6_AstNode* functionType,
                                                     const XL6_AstNode* args,
                                                     int releaseOwned)
{
    const XL6_AstNode* concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, returnType));
    const char* typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CgenIsTypedContainerType(ctx, concreteType) ) {
        xl6OutputPuts("(xl6_function_fatal(3), ", ctx->out);
        xl6CgenFunctionDefaultReturnExpr(ctx, concreteType);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CgenIsXValueType(concreteType) ) {
        xl6CgenFunctionInvokeFallbackCallExpr(ctx, fnExpr, functionType, args, releaseOwned);
    } else if ( xl6CgenIsFunctionType(concreteType) ) {
        xl6OutputPuts("xl6_function_from_xvalue_release(", ctx->out);
        xl6CgenFunctionInvokeFallbackCallExpr(ctx, fnExpr, functionType, args, releaseOwned);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CgenIsIntegerTypeName(typeName) ) {
        xl6OutputPrintf(ctx->out, "((%s)xl6_xvalue_to_i64_release(", xl6CgenTypeName(ctx, concreteType));
        xl6CgenFunctionInvokeFallbackCallExpr(ctx, fnExpr, functionType, args, releaseOwned);
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsFloatTypeName(typeName) ) {
        xl6OutputPrintf(ctx->out, "((%s)xl6_xvalue_to_f64_release(", xl6CgenTypeName(ctx, concreteType));
        xl6CgenFunctionInvokeFallbackCallExpr(ctx, fnExpr, functionType, args, releaseOwned);
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsBoolTypeName(typeName) ) {
        xl6OutputPuts("xl6_xvalue_to_bool_release(", ctx->out);
        xl6CgenFunctionInvokeFallbackCallExpr(ctx, fnExpr, functionType, args, releaseOwned);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "string") ) {
        xl6OutputPuts("xl6_xvalue_to_string_release(", ctx->out);
        xl6CgenFunctionInvokeFallbackCallExpr(ctx, fnExpr, functionType, args, releaseOwned);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "time") ) {
        xl6OutputPuts("xl6_xvalue_to_time_release(", ctx->out);
        xl6CgenFunctionInvokeFallbackCallExpr(ctx, fnExpr, functionType, args, releaseOwned);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "point") ) {
        xl6OutputPuts("xl6_xvalue_to_point_release(", ctx->out);
        xl6CgenFunctionInvokeFallbackCallExpr(ctx, fnExpr, functionType, args, releaseOwned);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "type") ) {
        xl6OutputPuts("((unsigned long long)xl6_xvalue_to_i64_release(", ctx->out);
        xl6CgenFunctionInvokeFallbackCallExpr(ctx, fnExpr, functionType, args, releaseOwned);
        xl6OutputPuts("))", ctx->out);
    } else {
        xl6OutputPuts("(xl6_function_fatal(3), ", ctx->out);
        xl6CgenFunctionDefaultReturnExpr(ctx, concreteType);
        xl6OutputPutc(')', ctx->out);
    }
}

static int xl6CgenFunctionExprIsRepeatableLvalue(const XL6_AstNode* expr)
{
    if ( expr == NULL ) {
        return 0;
    }
    if ( expr->kind == XL6_AST_NAME ) {
        return 1;
    }
    if ( expr->kind == XL6_AST_MEMBER ) {
        return expr->childCount > 0 && xl6CgenFunctionExprIsRepeatableLvalue(expr->children[0]);
    }
    return 0;
}

static void xl6CgenFunctionExprField(XL6_CgenContext* ctx, const XL6_AstNode* fnExpr, const char* fieldName)
{
    xl6CgenExpr(ctx, fnExpr);
    xl6OutputPutc('.', ctx->out);
    xl6OutputPuts(fieldName, ctx->out);
}

static void xl6CgenFunctionInvokeExpr(XL6_CgenContext* ctx, const XL6_AstNode* fnExpr, const XL6_AstNode* functionType, const XL6_AstNode* args)
{
    unsigned long long signatureId;
    size_t positionalCount;
    size_t fixedCount;
    const XL6_AstNode* paramTypes;
    const XL6_AstNode* returns;
    int hasArrayVararg;
    int hasDictVararg;
    int returnCount;

    paramTypes = xl6CgenFunctionParamTypes(functionType);
    returns = xl6CgenFunctionReturnTypes(functionType);
    if ( paramTypes == NULL || returns == NULL ) {
        xl6CgenError(ctx, fnExpr, "function expression signature is incomplete");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    hasArrayVararg = paramTypes->childCount > 0 &&
                     xl6CgenFunctionTypeParamIsArrayVararg(paramTypes->children[paramTypes->childCount - 1]);
    hasDictVararg = paramTypes->childCount > 0 &&
                    xl6CgenFunctionTypeParamIsDictVararg(paramTypes->children[paramTypes->childCount - 1]);
    fixedCount = xl6CgenFunctionTypeFixedParamCount(paramTypes);
    positionalCount = xl6CgenFunctionValuePositionalArgCount(args);
    if ( positionalCount < fixedCount ||
         (!hasArrayVararg && positionalCount > fixedCount) ||
         (!hasDictVararg && xl6CgenFunctionValueDictVarargCount(args) > 0) ) {
        xl6CgenError(ctx, functionType, "function call argument count does not match signature");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    returnCount = (int)returns->childCount;
    signatureId = xl6CgenFunctionSignatureId(ctx, paramTypes, returns);

    if ( !xl6CgenFunctionExprIsRepeatableLvalue(fnExpr) ) {
        if ( returnCount > 1 ) {
            xl6CgenError(ctx, fnExpr, "function expression with multiple return values requires a stable function value");
            xl6OutputPuts("(xl6_function_fatal(3), (", ctx->out);
            xl6CgenFunctionReturnStructName(ctx, paramTypes, returns);
            xl6OutputPuts("){0})", ctx->out);
            return;
        }
        if ( returnCount == 1 ) {
            xl6CgenFunctionInvokeFallbackExprForExpr(ctx, returns->children[0], fnExpr, functionType, args, 1);
            return;
        }
        xl6OutputPuts("xl6_xvalue_drop(", ctx->out);
        xl6CgenFunctionInvokeFallbackCallExpr(ctx, fnExpr, functionType, args, 1);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    xl6OutputPuts("((", ctx->out);
    xl6CgenFunctionExprField(ctx, fnExpr, "signature");
    xl6OutputPrintf(ctx->out, " == 0x%016llXULL) ? ", signatureId);
    if ( returnCount > 1 ) {
        xl6OutputPuts("((", ctx->out);
        xl6CgenFunctionExprField(ctx, fnExpr, "invoke");
        xl6OutputPuts(" != 0) ? ", ctx->out);
        xl6CgenFunctionInvokeDirectCallExpr(ctx, fnExpr, paramTypes, returns, args, fixedCount, hasArrayVararg, hasDictVararg);
        xl6OutputPuts(" : (xl6_function_fatal(3), (", ctx->out);
        xl6CgenFunctionReturnStructName(ctx, paramTypes, returns);
        xl6OutputPuts("){0})) : (xl6_function_fatal(3), (", ctx->out);
        xl6CgenFunctionReturnStructName(ctx, paramTypes, returns);
        xl6OutputPuts("){0}))", ctx->out);
    } else if ( returnCount == 1 ) {
        xl6OutputPuts("((", ctx->out);
        xl6CgenFunctionExprField(ctx, fnExpr, "invoke");
        xl6OutputPuts(" != 0) ? ", ctx->out);
        xl6CgenFunctionInvokeDirectCallExpr(ctx, fnExpr, paramTypes, returns, args, fixedCount, hasArrayVararg, hasDictVararg);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenFunctionInvokeFallbackExprForExpr(ctx, returns->children[0], fnExpr, functionType, args, 0);
        xl6OutputPuts(") : (xl6_function_fatal(3), ", ctx->out);
        xl6CgenFunctionDefaultReturnExpr(ctx, returns->children[0]);
        xl6OutputPuts("))", ctx->out);
    } else {
        xl6OutputPuts("((", ctx->out);
        xl6CgenFunctionExprField(ctx, fnExpr, "invoke");
        xl6OutputPuts(" != 0) ? ", ctx->out);
        xl6CgenFunctionInvokeDirectCallExpr(ctx, fnExpr, paramTypes, returns, args, fixedCount, hasArrayVararg, hasDictVararg);
        xl6OutputPuts(" : xl6_xvalue_drop(", ctx->out);
        xl6CgenFunctionInvokeFallbackCallExpr(ctx, fnExpr, functionType, args, 0);
        xl6OutputPuts(")) : xl6_function_fatal(3))", ctx->out);
    }
}

static void xl6CgenVarFunctionInvoke(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* args)
{
    size_t i;
    int namedCount;
    int positionalCount;
    char cName[512];

    xl6CgenValueStorageNameText(ctx, cName, sizeof(cName), name);
    namedCount = xl6CgenFunctionValueDictVarargCount(args);
    positionalCount = xl6CgenFunctionValuePositionalArgCount(args);
    if ( namedCount > 0 ) {
        xl6OutputPrintf(ctx->out, "xl6_var_call_kwargs(%s, ", cName);
        xl6CgenFunctionValueDictVarargExpr(ctx, args);
        xl6OutputPrintf(ctx->out, ", %u", (unsigned)positionalCount);
    } else {
        xl6OutputPrintf(ctx->out, "xl6_var_call(%s, %u", cName, (unsigned)positionalCount);
    }
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG ) {
                continue;
            }
            xl6OutputPuts(", ", ctx->out);
            if ( arg != NULL && arg->childCount > 0 ) {
                xl6CgenXValueFromExpr(ctx, arg->children[0]);
            } else {
                xl6OutputPuts("xvoCreateNull()", ctx->out);
            }
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenVarFunctionInvokeExpr(XL6_CgenContext* ctx, const XL6_AstNode* fnExpr, const XL6_AstNode* args)
{
    size_t i;
    int namedCount;
    int positionalCount;

    namedCount = xl6CgenFunctionValueDictVarargCount(args);
    positionalCount = xl6CgenFunctionValuePositionalArgCount(args);
    if ( namedCount > 0 ) {
        xl6OutputPuts("xl6_var_call_kwargs(", ctx->out);
        xl6CgenExpr(ctx, fnExpr);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenFunctionValueDictVarargExpr(ctx, args);
        xl6OutputPrintf(ctx->out, ", %u", (unsigned)positionalCount);
    } else {
        xl6OutputPuts("xl6_var_call(", ctx->out);
        xl6CgenExpr(ctx, fnExpr);
        xl6OutputPrintf(ctx->out, ", %u", (unsigned)positionalCount);
    }
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG ) {
                continue;
            }
            xl6OutputPuts(", ", ctx->out);
            if ( arg != NULL && arg->childCount > 0 ) {
                xl6CgenXValueFromExpr(ctx, arg->children[0]);
            } else {
                xl6OutputPuts("xvoCreateNull()", ctx->out);
            }
        }
    }
    xl6OutputPutc(')', ctx->out);
}
