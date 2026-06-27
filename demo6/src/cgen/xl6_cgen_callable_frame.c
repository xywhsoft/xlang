static void xl6CgenEmitAnonInvokeProto(XL6_CgenContext* ctx, const XL6_CgenAnonFunc* info)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    params = info->node->childCount > 0 ? info->node->children[0] : NULL;
    returns = info->node->childCount > 1 ? info->node->children[1] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFunctionReturnStructName(ctx, params, returns);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPrintf(ctx->out, " xl6_anon_%d_invoke(void* __xl6_env", info->id);
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            xl6OutputPuts(", ", ctx->out);
            xl6OutputPuts(xl6CgenValueStorageCType(ctx, param->children[0]), ctx->out);
            xl6OutputPutc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, param->text);
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenEmitVarInvokeValueExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* valueExpr)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CTextEquals(typeName, "var") || xl6CgenIsArrayXValueType(concreteType) ||
         xl6CgenIsListXValueType(concreteType) || xl6CgenIsSetXValueType(concreteType) ||
         xl6CgenIsDictXValueType(concreteType) ) {
        xl6OutputPuts(valueExpr != NULL ? valueExpr : "0", ctx->out);
    } else if ( xl6CgenIsIntegerTypeName(typeName) ) {
        xl6OutputPrintf(ctx->out, "((%s)xvoGetInt(%s))", xl6CgenTypeName(ctx, concreteType), valueExpr != NULL ? valueExpr : "0");
    } else if ( xl6CgenIsFloatTypeName(typeName) ) {
        xl6OutputPrintf(ctx->out, "((%s)xvoGetFloat(%s))", xl6CgenTypeName(ctx, concreteType), valueExpr != NULL ? valueExpr : "0");
    } else if ( xl6CgenIsBoolTypeName(typeName) ) {
        xl6OutputPrintf(ctx->out, "xvoGetBool(%s)", valueExpr != NULL ? valueExpr : "0");
    } else if ( xl6CTextEquals(typeName, "string") ) {
        xl6OutputPrintf(ctx->out, "xvoGetText(%s)", valueExpr != NULL ? valueExpr : "0");
    } else if ( xl6CTextEquals(typeName, "time") ) {
        xl6OutputPrintf(ctx->out, "xvoGetTime(%s)", valueExpr != NULL ? valueExpr : "0");
    } else if ( xl6CTextEquals(typeName, "point") ) {
        xl6OutputPrintf(ctx->out, "xvoGetPoint(%s)", valueExpr != NULL ? valueExpr : "0");
    } else {
        xl6OutputPuts("0", ctx->out);
    }
}

static void xl6CgenEmitVarInvokeArgExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, unsigned index)
{
    char valueExpr[64];

    snprintf(valueExpr, sizeof(valueExpr), "__xl6_argv[%u]", index);
    xl6CgenEmitVarInvokeValueExpr(ctx, typeNode, valueExpr);
}

static int xl6CgenParamListHasArrayVararg(const XL6_AstNode* params, int fixedCount)
{
    return params != NULL && params->childCount > (size_t)fixedCount &&
           xl6CgenParamIsArrayVararg(params->children[fixedCount]);
}

static int xl6CgenParamListHasDictVararg(const XL6_AstNode* params, int fixedCount)
{
    return params != NULL && params->childCount > (size_t)fixedCount &&
           xl6CgenParamIsDictVararg(params->children[fixedCount]);
}

static int xl6CgenCallableSupportsXCall(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* returns)
{
    int fixedCount;
    size_t i;

    fixedCount = xl6CgenFixedParamCount(params);
    if ( params != NULL && params->childCount > (size_t)fixedCount ) {
        const XL6_AstNode* vararg = params->children[fixedCount];
        if ( !xl6CgenParamIsArrayVararg(vararg) && !xl6CgenParamIsDictVararg(vararg) ) {
            return 0;
        }
    }
    for ( i = 0; returns != NULL && i < returns->childCount; ++i ) {
        if ( !xl6CgenTypeSupportsVarInvokeValue(ctx, returns->children[i]) ) {
            return 0;
        }
    }
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( param == NULL || param->childCount == 0 ||
             !xl6CgenTypeSupportsVarInvokeValue(ctx, param->children[0]) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6CgenFuncSupportsXCall(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    if ( ctx == NULL || func == NULL ) {
        return 0;
    }
    return xl6CgenCallableSupportsXCall(ctx,
                                        func->childCount > 1 ? func->children[1] : NULL,
                                        func->childCount > 2 ? func->children[2] : NULL);
}

static int xl6CgenAnonSupportsXCall(XL6_CgenContext* ctx, const XL6_CgenAnonFunc* info)
{
    if ( ctx == NULL || info == NULL || info->node == NULL ) {
        return 0;
    }
    return xl6CgenCallableSupportsXCall(ctx,
                                        info->node->childCount > 0 ? info->node->children[0] : NULL,
                                        info->node->childCount > 1 ? info->node->children[1] : NULL);
}

static void xl6CgenEmitVarInvokeLocals(XL6_CgenContext* ctx, int hasArrayVararg)
{
    if ( hasArrayVararg ) {
        xl6OutputPuts("    xvalue __xl6_vararg;\n", ctx->out);
        xl6OutputPuts("    int __xl6_i;\n", ctx->out);
    }
}

static void xl6CgenEmitXCallLocals(XL6_CgenContext* ctx, const XL6_AstNode* params, int fixedCount, int hasArrayVararg, int hasDictVararg)
{
    int i;

    xl6OutputPuts("    int __xl6_argc;\n", ctx->out);
    xl6OutputPuts("    xvalue* __xl6_argv;\n", ctx->out);
    xl6OutputPuts("    xvalue __xl6_kwargs;\n", ctx->out);
    xl6OutputPuts("    xvalue __xl6_kwrest;\n", ctx->out);
    xl6OutputPuts("    xvalue __xl6_value;\n", ctx->out);
    for ( i = 0; params != NULL && i < fixedCount; ++i ) {
        xl6OutputPrintf(ctx->out, "    xvalue __xl6_arg_%d;\n", i);
    }
    if ( hasArrayVararg || hasDictVararg ) {
        xl6OutputPuts("    xvalue __xl6_vararg;\n", ctx->out);
    }
    if ( hasArrayVararg ) {
        xl6OutputPuts("    int __xl6_i;\n", ctx->out);
    }
}

static void xl6CgenEmitXCallSetup(XL6_CgenContext* ctx)
{
    xl6OutputPuts("    if (__xl6_frame == 0 || __xl6_result == 0) {\n", ctx->out);
    xl6OutputPuts("        return false;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    __xl6_argc = (int)__xl6_frame->Argc;\n", ctx->out);
    xl6OutputPuts("    __xl6_argv = __xl6_frame->Argv;\n", ctx->out);
    xl6OutputPuts("    if (__xl6_argc > 0 && __xl6_argv == 0) {\n", ctx->out);
    xl6OutputPuts("        return false;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    __xl6_kwargs = __xl6_frame->Kwargs;\n", ctx->out);
    xl6OutputPuts("    if (__xl6_kwargs != 0 && xvoIsNull(__xl6_kwargs)) {\n", ctx->out);
    xl6OutputPuts("        __xl6_kwargs = 0;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    if (__xl6_kwargs != 0 && xvoType(__xl6_kwargs) != XL6_XVO_DT_TABLE) {\n", ctx->out);
    xl6OutputPuts("        return false;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
}

static void xl6CgenEmitXCallParamSetup(XL6_CgenContext* ctx, const XL6_AstNode* params, int fixedCount)
{
    int i;

    for ( i = 0; params != NULL && i < fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const char* name = param != NULL ? param->text : NULL;
        xl6OutputPrintf(ctx->out, "    if (__xl6_argc > %d && __xl6_kwargs != 0 && xvoTableExists(__xl6_kwargs, ", i);
        xl6CgenCStringLiteral(ctx->out, name != NULL ? name : "");
        xl6OutputPuts(", 0)) {\n", ctx->out);
        xl6OutputPuts("        return false;\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
        xl6OutputPrintf(ctx->out, "    __xl6_arg_%d = (__xl6_argc > %d) ? __xl6_argv[%d] : ((__xl6_kwargs != 0 && xvoTableExists(__xl6_kwargs, ", i, i, i);
        xl6CgenCStringLiteral(ctx->out, name != NULL ? name : "");
        xl6OutputPuts(", 0)) ? xvoTableGetValue(__xl6_kwargs, ", ctx->out);
        xl6CgenCStringLiteral(ctx->out, name != NULL ? name : "");
        xl6OutputPuts(", 0) : 0);\n", ctx->out);
    }
}

static void xl6CgenEmitXCallRemoveFixedKwargs(XL6_CgenContext* ctx, const XL6_AstNode* params, int fixedCount, const char* dictName)
{
    int i;

    for ( i = 0; params != NULL && i < fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const char* name = param != NULL ? param->text : NULL;
        xl6OutputPrintf(ctx->out, "    (void)xvoTableRemove(%s, ", dictName != NULL ? dictName : "__xl6_kwrest");
        xl6CgenCStringLiteral(ctx->out, name != NULL ? name : "");
        xl6OutputPuts(", 0);\n", ctx->out);
    }
}

static void xl6CgenEmitXCallArityCheck(XL6_CgenContext* ctx, const XL6_AstNode* params, int fixedCount, int hasArrayVararg, int hasDictVararg)
{
    int requiredCount;
    int i;

    requiredCount = xl6CgenParamRequiredCount(params);
    if ( !hasArrayVararg ) {
        xl6OutputPrintf(ctx->out, "    if (__xl6_argc > %u) {\n", (unsigned)fixedCount);
        xl6OutputPuts("        return false;\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
    }
    for ( i = 0; i < requiredCount; ++i ) {
        xl6OutputPrintf(ctx->out, "    if (__xl6_arg_%d == 0) {\n", i);
        xl6OutputPuts("        return false;\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
    }
    if ( !hasDictVararg ) {
        xl6OutputPuts("    if (__xl6_kwargs != 0) {\n", ctx->out);
        xl6OutputPuts("        __xl6_kwrest = xvoCopy(__xl6_kwargs);\n", ctx->out);
        xl6OutputPuts("        if (__xl6_kwrest == 0) {\n", ctx->out);
        xl6OutputPuts("            return false;\n", ctx->out);
        xl6OutputPuts("        }\n", ctx->out);
        xl6CgenEmitXCallRemoveFixedKwargs(ctx, params, fixedCount, "__xl6_kwrest");
        xl6OutputPuts("        if (xvoTableItemCount(__xl6_kwrest) != 0) {\n", ctx->out);
        xl6OutputPuts("            xvoUnref(__xl6_kwrest);\n", ctx->out);
        xl6OutputPuts("            return false;\n", ctx->out);
        xl6OutputPuts("        }\n", ctx->out);
        xl6OutputPuts("        xvoUnref(__xl6_kwrest);\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
    }
}

static void xl6CgenEmitXCallTypeChecks(XL6_CgenContext* ctx, const XL6_AstNode* params, int fixedCount)
{
    size_t i;

    for ( i = 0; params != NULL && i < (size_t)fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
        const char* runtimeType = xl6CgenXValueRuntimeTypeConst(ctx, paramType);
        if ( runtimeType != NULL ) {
            xl6OutputPrintf(ctx->out, "    if (__xl6_arg_%u != 0 && xvoType(__xl6_arg_%u) != %s) {\n", (unsigned)i, (unsigned)i, runtimeType);
            xl6OutputPuts("        return false;\n", ctx->out);
            xl6OutputPuts("    }\n", ctx->out);
        }
    }
}

static void xl6CgenEmitXCallVarargPack(XL6_CgenContext* ctx, const XL6_AstNode* params, int fixedCount, int hasArrayVararg, int hasDictVararg)
{
    if ( !hasArrayVararg && !hasDictVararg ) {
        return;
    }
    if ( hasDictVararg ) {
        xl6OutputPuts("    if (__xl6_kwargs != 0) {\n", ctx->out);
        xl6OutputPuts("        __xl6_vararg = xvoCopy(__xl6_kwargs);\n", ctx->out);
        xl6OutputPuts("    } else {\n", ctx->out);
        xl6OutputPuts("        __xl6_vararg = xvoCreateTable();\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
        xl6OutputPuts("    if (__xl6_vararg == 0 || xvoType(__xl6_vararg) != XL6_XVO_DT_TABLE) {\n", ctx->out);
        xl6OutputPuts("        if (__xl6_vararg != 0) {\n", ctx->out);
        xl6OutputPuts("            xvoUnref(__xl6_vararg);\n", ctx->out);
        xl6OutputPuts("        }\n", ctx->out);
        xl6OutputPuts("        return false;\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
        xl6CgenEmitXCallRemoveFixedKwargs(ctx, params, fixedCount, "__xl6_vararg");
        return;
    }
    xl6OutputPuts("    __xl6_vararg = xvoCreateArray();\n", ctx->out);
    xl6OutputPuts("    if (__xl6_vararg == 0) {\n", ctx->out);
    xl6OutputPuts("        return false;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    for (__xl6_i = %u; __xl6_i < __xl6_argc; ++__xl6_i) {\n", (unsigned)fixedCount);
    xl6OutputPuts("        if (!xvoArrayAppendValue(__xl6_vararg, __xl6_argv[__xl6_i], false)) {\n", ctx->out);
    xl6OutputPuts("            xvoUnref(__xl6_vararg);\n", ctx->out);
    xl6OutputPuts("            return false;\n", ctx->out);
    xl6OutputPuts("        }\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
}

static void xl6CgenEmitXCallVarargCleanup(XL6_CgenContext* ctx, int hasArrayVararg, int hasDictVararg)
{
    if ( hasArrayVararg || hasDictVararg ) {
        xl6OutputPuts("    xvoUnref(__xl6_vararg);\n", ctx->out);
    }
}

static void xl6CgenEmitXCallSetResult(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, unsigned index, const char* exprText, int hasArrayVararg, int hasDictVararg)
{
    xl6OutputPrintf(ctx->out, "    __xl6_value = ");
    xl6CgenXValueFromTypedCExpr(ctx, typeNode, exprText);
    xl6OutputPuts(";\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    if (__xl6_value == 0 || !xrtCallResultSetValue(__xl6_result, %uu, __xl6_value, true)) {\n", index);
    xl6OutputPuts("        if (__xl6_value != 0) {\n", ctx->out);
    xl6OutputPuts("            xvoUnref(__xl6_value);\n", ctx->out);
    xl6OutputPuts("        }\n", ctx->out);
    (void)hasArrayVararg;
    (void)hasDictVararg;
    xl6OutputPuts("        return false;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
}

static void xl6CgenEmitVarInvokeArityCheck(XL6_CgenContext* ctx, const XL6_AstNode* params, int fixedCount, int hasArrayVararg)
{
    int requiredCount;

    requiredCount = xl6CgenParamRequiredCount(params);
    if ( hasArrayVararg ) {
        xl6OutputPrintf(ctx->out, "    if (__xl6_argc < %u) {\n", (unsigned)requiredCount);
    } else {
        xl6OutputPrintf(ctx->out, "    if (__xl6_argc < %u || __xl6_argc > %u) {\n", (unsigned)requiredCount, (unsigned)fixedCount);
    }
    xl6OutputPuts("        xl6_function_fatal(3);\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
}

static void xl6CgenEmitVarInvokeTypeChecks(XL6_CgenContext* ctx, const XL6_AstNode* params, int fixedCount, int hasArrayVararg)
{
    size_t i;
    size_t checkCount;
    int requiredCount;

    (void)hasArrayVararg;
    checkCount = params != NULL ? (size_t)fixedCount : 0;
    requiredCount = xl6CgenParamRequiredCount(params);
    for ( i = 0; params != NULL && i < checkCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
        const char* runtimeType = xl6CgenXValueRuntimeTypeConst(ctx, paramType);
        if ( runtimeType != NULL ) {
            if ( (int)i >= requiredCount ) {
                xl6OutputPrintf(ctx->out, "    if (__xl6_argc > %u && xvoType(__xl6_argv[%u]) != %s) {\n", (unsigned)i, (unsigned)i, runtimeType);
            } else {
                xl6OutputPrintf(ctx->out, "    if (xvoType(__xl6_argv[%u]) != %s) {\n", (unsigned)i, runtimeType);
            }
            xl6OutputPuts("        xl6_function_fatal(3);\n", ctx->out);
            xl6OutputPuts("    }\n", ctx->out);
        }
    }
}

static void xl6CgenEmitVarInvokeArrayPack(XL6_CgenContext* ctx, int fixedCount, int hasArrayVararg)
{
    if ( !hasArrayVararg ) {
        return;
    }
    xl6OutputPuts("    __xl6_vararg = xvoCreateArray();\n", ctx->out);
    xl6OutputPuts("    if (__xl6_vararg == 0) {\n", ctx->out);
    xl6OutputPuts("        xl6_function_fatal(2);\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    for (__xl6_i = %u; __xl6_i < __xl6_argc; ++__xl6_i) {\n", (unsigned)fixedCount);
    /* xl6_function_call_var owns __xl6_argv; append by reference here. */
    xl6OutputPuts("        if (!xvoArrayAppendValue(__xl6_vararg, __xl6_argv[__xl6_i], false)) {\n", ctx->out);
    xl6OutputPuts("            xl6_function_fatal(2);\n", ctx->out);
    xl6OutputPuts("        }\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
}

static void xl6CgenEmitVarInvokeArgOrDefault(XL6_CgenContext* ctx, const XL6_AstNode* param, unsigned index)
{
    const XL6_AstNode* paramType;

    paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
    if ( param != NULL && param->childCount > 1 ) {
        xl6OutputPrintf(ctx->out, "((__xl6_argc > %u) ? (", index);
        xl6CgenEmitVarInvokeArgExpr(ctx, paramType, index);
        xl6OutputPuts(") : (", ctx->out);
        xl6CgenExpr(ctx, param->children[1]);
        xl6OutputPuts("))", ctx->out);
    } else {
        xl6CgenEmitVarInvokeArgExpr(ctx, paramType, index);
    }
}

static void xl6CgenEmitXCallArgOrDefault(XL6_CgenContext* ctx, const XL6_AstNode* param, unsigned index)
{
    const XL6_AstNode* paramType;
    char valueExpr[64];

    paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
    snprintf(valueExpr, sizeof(valueExpr), "__xl6_arg_%u", index);
    if ( param != NULL && param->childCount > 1 ) {
        xl6OutputPrintf(ctx->out, "((%s != 0) ? (", valueExpr);
        xl6CgenEmitVarInvokeValueExpr(ctx, paramType, valueExpr);
        xl6OutputPuts(") : (", ctx->out);
        xl6CgenExpr(ctx, param->children[1]);
        xl6OutputPuts("))", ctx->out);
    } else {
        xl6CgenEmitVarInvokeValueExpr(ctx, paramType, valueExpr);
    }
}

static void xl6CgenEmitVarInvokeCallArgs(XL6_CgenContext* ctx, const XL6_AstNode* params, int fixedCount, int hasArrayVararg)
{
    size_t i;

    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        xl6OutputPuts(", ", ctx->out);
        if ( hasArrayVararg && i == (size_t)fixedCount ) {
            xl6OutputPuts("__xl6_vararg", ctx->out);
        } else {
            xl6CgenEmitVarInvokeArgOrDefault(ctx, param, (unsigned)i);
        }
    }
}

static void xl6CgenEmitXCallCallArgs(XL6_CgenContext* ctx, const XL6_AstNode* params, int fixedCount, int hasArrayVararg, int hasDictVararg)
{
    size_t i;

    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        xl6OutputPuts(", ", ctx->out);
        if ( (hasArrayVararg || hasDictVararg) && i == (size_t)fixedCount ) {
            xl6OutputPuts("__xl6_vararg", ctx->out);
        } else {
            xl6CgenEmitXCallArgOrDefault(ctx, param, (unsigned)i);
        }
    }
}

static void xl6CgenEmitAnonVarInvoke(XL6_CgenContext* ctx, const XL6_CgenAnonFunc* info)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int fixedCount;
    int hasArrayVararg;

    if ( !ctx->needsFunctionVarRuntime || !xl6CgenAnonSupportsVarInvoke(ctx, info) ) {
        return;
    }

    params = info->node->childCount > 0 ? info->node->children[0] : NULL;
    returns = info->node->childCount > 1 ? info->node->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    hasArrayVararg = xl6CgenParamListHasArrayVararg(params, fixedCount);

    xl6OutputPrintf(ctx->out, "static xvalue xl6_anon_%d_invoke_var(void* __xl6_env, int __xl6_argc, xvalue* __xl6_argv)\n{\n", info->id);
    xl6CgenEmitVarInvokeLocals(ctx, hasArrayVararg);
    xl6CgenEmitVarInvokeArityCheck(ctx, params, fixedCount, hasArrayVararg);
    xl6CgenEmitVarInvokeTypeChecks(ctx, params, fixedCount, hasArrayVararg);
    xl6CgenEmitVarInvokeArrayPack(ctx, fixedCount, hasArrayVararg);

    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[0]), ctx->out);
        xl6OutputPuts(" __xl6_result = ", ctx->out);
    } else {
        xl6OutputPuts("    ", ctx->out);
    }
    xl6OutputPrintf(ctx->out, "xl6_anon_%d_invoke(__xl6_env", info->id);
    xl6CgenEmitVarInvokeCallArgs(ctx, params, fixedCount, hasArrayVararg);
    xl6OutputPuts(");\n", ctx->out);

    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("    return ", ctx->out);
        xl6CgenXValueFromTypedCExpr(ctx, returns->children[0], "__xl6_result");
        xl6OutputPuts(";\n", ctx->out);
    } else {
        xl6OutputPuts("    return xvoCreateNull();\n", ctx->out);
    }
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenEmitAnonXCall(XL6_CgenContext* ctx, const XL6_CgenAnonFunc* info)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int fixedCount;
    int hasArrayVararg;
    int hasDictVararg;
    size_t i;

    if ( !ctx->needsFunctionVarRuntime || !xl6CgenAnonSupportsXCall(ctx, info) ) {
        return;
    }

    params = info->node->childCount > 0 ? info->node->children[0] : NULL;
    returns = info->node->childCount > 1 ? info->node->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    hasArrayVararg = xl6CgenParamListHasArrayVararg(params, fixedCount);
    hasDictVararg = xl6CgenParamListHasDictVararg(params, fixedCount);

    xl6OutputPrintf(ctx->out, "static bool xl6_anon_%d_xcall(void* __xl6_env, xrt_call_frame* __xl6_frame, xrt_call_result* __xl6_result)\n{\n", info->id);
    xl6CgenEmitXCallLocals(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);
    xl6CgenEmitXCallSetup(ctx);
    xl6CgenEmitXCallParamSetup(ctx, params, fixedCount);
    xl6CgenEmitXCallArityCheck(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);
    xl6CgenEmitXCallTypeChecks(ctx, params, fixedCount);
    xl6CgenEmitXCallVarargPack(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);

    if ( returns != NULL && returns->childCount > 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6CgenFunctionReturnStructName(ctx, params, returns);
        xl6OutputPrintf(ctx->out, " __xl6_multi = xl6_anon_%d_invoke(__xl6_env", info->id);
        xl6CgenEmitXCallCallArgs(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);
        xl6OutputPuts(");\n", ctx->out);
        for ( i = 0; i < returns->childCount; ++i ) {
            char exprText[96];
            snprintf(exprText, sizeof(exprText), "__xl6_multi.value%u", (unsigned)i);
            xl6CgenEmitXCallSetResult(ctx, returns->children[i], (unsigned)i, exprText, hasArrayVararg, hasDictVararg);
        }
    } else if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[0]), ctx->out);
        xl6OutputPrintf(ctx->out, " __xl6_single = xl6_anon_%d_invoke(__xl6_env", info->id);
        xl6CgenEmitXCallCallArgs(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenEmitXCallSetResult(ctx, returns->children[0], 0u, "__xl6_single", hasArrayVararg, hasDictVararg);
    } else {
        xl6OutputPrintf(ctx->out, "    xl6_anon_%d_invoke(__xl6_env", info->id);
        xl6CgenEmitXCallCallArgs(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);
        xl6OutputPuts(");\n", ctx->out);
    }

    (void)hasArrayVararg;
    (void)hasDictVararg;
    xl6OutputPuts("    return true;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}
