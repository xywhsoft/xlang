static void xl6CgenClosureCapturePath(char* buffer, size_t capacity, const XL6_CgenAnonCapture* capture)
{
    char fieldName[512];

    xl6CgenCIdentText(fieldName, sizeof(fieldName), capture != NULL ? capture->name : NULL);
    snprintf(buffer, capacity, "__env->%s", fieldName);
}

static void xl6CgenReleaseClosureCapture(XL6_CgenContext* ctx, const XL6_CgenAnonCapture* capture)
{
    const XL6_AstNode* typeNode = capture != NULL ? capture->type : NULL;
    XL6_AstNode* classNode;
    char path[1024];

    if ( typeNode == NULL ) {
        return;
    }
    xl6CgenClosureCapturePath(path, sizeof(path), capture);
    if ( xl6CgenIsStringTypeNode(ctx, typeNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtFree((void*)%s); %s = 0; }\n", path, path, path);
    } else if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", path);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6CgenTypedContainerDestroyValue(ctx, typeNode, path);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    } else if ( xl6CgenIsXValueType(typeNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xvoUnref(%s); %s = 0; }\n", path, path, path);
    } else if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6CgenReleaseFunctionPath(ctx, path);
    } else {
        classNode = xl6CgenClassNodeForType(ctx, typeNode);
        if ( classNode != NULL ) {
            xl6CgenDeclDestroyTemporaryClassValue(ctx, classNode, typeNode, path);
        }
    }
}

static void xl6CgenCopyClosureCapture(XL6_CgenContext* ctx, const XL6_CgenAnonCapture* capture)
{
    const XL6_AstNode* typeNode = capture != NULL ? capture->type : NULL;
    XL6_AstNode* classNode;
    char path[1024];
    char sourceName[512];

    if ( typeNode == NULL ) {
        return;
    }
    xl6CgenClosureCapturePath(path, sizeof(path), capture);
    xl6CgenCIdentText(sourceName, sizeof(sourceName), capture->name);
    xl6CgenIndent(ctx);
    xl6OutputPuts(path, ctx->out);
    xl6OutputPuts(" = ", ctx->out);
    if ( xl6CgenIsStringTypeNode(ctx, typeNode) ) {
        xl6OutputPrintf(ctx->out, "xrtCopyStr(%s, 0u)", sourceName);
    } else if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        xl6CgenTypedContainerCopyCExprMode(ctx, typeNode, sourceName, "XRT_OBJMODE_SHARED");
    } else if ( xl6CgenIsXValueType(typeNode) ) {
        xl6OutputPrintf(ctx->out, "xl6_xrt_retain(%s)", sourceName);
    } else if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6OutputPuts(sourceName, ctx->out);
    } else {
        classNode = xl6CgenClassNodeForType(ctx, typeNode);
        if ( classNode != NULL && !xl6CgenClassHasTypeParams(classNode) ) {
            xl6CgenClassCopyBuiltinHelperSymbol(ctx->out, classNode, 0);
            xl6OutputPrintf(ctx->out, "(%s, 1)", sourceName);
        } else {
            xl6OutputPuts(sourceName, ctx->out);
        }
    }
    xl6OutputPuts(";\n", ctx->out);

    if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6CgenRetainFunctionPath(ctx, path);
    } else {
        classNode = xl6CgenClassNodeForType(ctx, typeNode);
        if ( classNode != NULL && xl6CgenClassHasTypeParams(classNode) ) {
            xl6CgenRetainClassFunctionFields(ctx, classNode, path);
        }
    }
}

static void xl6CgenEmitAnonFunction(XL6_CgenContext* ctx, const XL6_CgenAnonFunc* info)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    const XL6_AstNode* body;
    const XL6_CgenAnonFunc* prevAnon;
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevReturnParams;
    const XL6_AstNode* prevReturns;
    const XL6_AstNode* prevReturnFunc;
    size_t varMark;
    size_t cleanupMark;
    unsigned long long signatureId;
    size_t i;

    params = info->node->childCount > 0 ? info->node->children[0] : NULL;
    returns = info->node->childCount > 1 ? info->node->children[1] : NULL;
    body = info->node->childCount > 2 ? info->node->children[2] : NULL;
    signatureId = xl6CgenFunctionSignatureId(ctx, params, returns);

    xl6CgenFunctionReturnStructDecl(ctx, params, returns);
    if ( info->captureCount > 0 ) {
        xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", ctx->out);
        xl6OutputPuts("void xrtFree(void* pMem);\n\n", ctx->out);
        xl6OutputPrintf(ctx->out, "typedef struct XL6_Closure_%d {\n", info->id);
        xl6OutputPuts("    int ref_count;\n", ctx->out);
        for ( i = 0; i < info->captureCount; ++i ) {
            xl6OutputPuts("    ", ctx->out);
            xl6OutputPuts(xl6CgenParamCType(ctx, info->captures[i].type), ctx->out);
            xl6OutputPutc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
            xl6OutputPuts(";\n", ctx->out);
        }
        xl6OutputPrintf(ctx->out, "} XL6_Closure_%d;\n\n", info->id);
    }

    xl6OutputPuts("static ", ctx->out);
    xl6CgenEmitAnonInvokeProto(ctx, info);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    prevAnon = ctx->currentAnonFunc;
    prevReturnType = ctx->currentReturnType;
    prevReturnParams = ctx->currentReturnParams;
    prevReturns = ctx->currentReturns;
    prevReturnFunc = ctx->currentReturnFunc;
    ctx->currentAnonFunc = info;
    ctx->currentReturnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    ctx->currentReturnParams = params;
    ctx->currentReturns = returns;
    ctx->currentReturnFunc = NULL;
    xl6CgenRegisterParamTypes(ctx, params);
    xl6CgenEmitManagedParamOwners(ctx, params);
    xl6CgenBeginTempScope(ctx, info->node);
    xl6CgenRegisterAnonCaptureTypes(ctx, info);
    if ( info->captureCount > 0 ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "XL6_Closure_%d* __env = (XL6_Closure_%d*)__xl6_env;\n", info->id, info->id);
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPuts("(void)__xl6_env;\n", ctx->out);
    }
    xl6CgenBlock(ctx, body);
    if ( !xl6CgenBlockEndsWithReturn(body) ) {
        xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    }
    xl6CgenEmitDefaultErrorReturn(ctx);
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    ctx->currentReturnType = prevReturnType;
    ctx->currentReturnParams = prevReturnParams;
    ctx->currentReturns = prevReturns;
    ctx->currentReturnFunc = prevReturnFunc;
    ctx->currentAnonFunc = prevAnon;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);

    if ( info->captureCount > 0 ) {
        xl6OutputPrintf(ctx->out, "static void xl6_anon_%d_retain(void* env)\n{\n", info->id);
        xl6OutputPrintf(ctx->out, "    XL6_Closure_%d* __env = (XL6_Closure_%d*)env;\n", info->id, info->id);
        xl6OutputPuts("    if (__env != 0) {\n", ctx->out);
        xl6OutputPuts("        __env->ref_count += 1;\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
        xl6OutputPuts("}\n\n", ctx->out);

        xl6OutputPrintf(ctx->out, "static void xl6_anon_%d_release(void* env)\n{\n", info->id);
        xl6OutputPrintf(ctx->out, "    XL6_Closure_%d* __env = (XL6_Closure_%d*)env;\n", info->id, info->id);
        xl6OutputPuts("    if (__env == 0) {\n", ctx->out);
        xl6OutputPuts("        return;\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
        xl6OutputPuts("    __env->ref_count -= 1;\n", ctx->out);
        xl6OutputPuts("    if (__env->ref_count == 0) {\n", ctx->out);
        ctx->indent += 2;
        for ( i = 0; i < info->captureCount; ++i ) {
            xl6CgenReleaseClosureCapture(ctx, &info->captures[i]);
        }
        ctx->indent -= 2;
        xl6OutputPuts("        xrtFree(__env);\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
        xl6OutputPuts("}\n\n", ctx->out);
    }

    xl6CgenEmitAnonVarInvoke(ctx, info);
    xl6CgenEmitAnonXCall(ctx, info);

    xl6OutputPrintf(ctx->out, "static XL6_Function xl6_anon_%d_make(", info->id);
    if ( info->captureCount == 0 ) {
        xl6OutputPuts("void", ctx->out);
    } else {
        for ( i = 0; i < info->captureCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPuts(", ", ctx->out);
            }
            xl6OutputPuts(xl6CgenParamCType(ctx, info->captures[i].type), ctx->out);
            xl6OutputPutc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, info->captures[i].name);
        }
    }
    xl6OutputPuts(")\n{\n", ctx->out);
    if ( info->captureCount > 0 ) {
        xl6OutputPrintf(ctx->out, "    XL6_Closure_%d* __env = (XL6_Closure_%d*)xrtMalloc(sizeof(XL6_Closure_%d));\n", info->id, info->id, info->id);
        xl6OutputPuts("    if (__env == 0) {\n", ctx->out);
        xl6OutputPuts("        xl6_function_fatal(2);\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
        xl6OutputPuts("    memset(__env, 0, sizeof(*__env));\n", ctx->out);
        xl6OutputPuts("    __env->ref_count = 1;\n", ctx->out);
        ctx->indent += 1;
        for ( i = 0; i < info->captureCount; ++i ) {
            xl6CgenCopyClosureCapture(ctx, &info->captures[i]);
        }
        ctx->indent -= 1;
    }
    xl6OutputPuts("    XL6_Function __fn;\n", ctx->out);
    xl6CgenEmitFunctionSigObject(ctx, "__xl6_sig", "", signatureId, params, returns);
    xl6OutputPrintf(ctx->out, "    __fn.signature = 0x%016llXULL;\n", signatureId);
    xl6OutputPuts("    __fn.sig = &__xl6_sig;\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    __fn.env = %s;\n", info->captureCount > 0 ? "__env" : "0");
    xl6OutputPrintf(ctx->out, "    __fn.invoke = (void*)xl6_anon_%d_invoke;\n", info->id);
    if ( ctx->needsFunctionVarRuntime && xl6CgenAnonSupportsVarInvoke(ctx, info) ) {
        xl6OutputPrintf(ctx->out, "    __fn.invoke_var = xl6_anon_%d_invoke_var;\n", info->id);
    } else {
        xl6OutputPuts("    __fn.invoke_var = 0;\n", ctx->out);
    }
    if ( ctx->needsFunctionVarRuntime && xl6CgenAnonSupportsXCall(ctx, info) ) {
        xl6OutputPrintf(ctx->out, "    __fn.invoke_xcall = xl6_anon_%d_xcall;\n", info->id);
    } else {
        xl6OutputPuts("    __fn.invoke_xcall = 0;\n", ctx->out);
    }
    if ( info->captureCount > 0 ) {
        xl6OutputPrintf(ctx->out, "    __fn.retain = xl6_anon_%d_retain;\n", info->id);
        xl6OutputPrintf(ctx->out, "    __fn.release = xl6_anon_%d_release;\n", info->id);
    } else {
        xl6OutputPuts("    __fn.retain = 0;\n", ctx->out);
        xl6OutputPuts("    __fn.release = 0;\n", ctx->out);
    }
    xl6OutputPuts("    return __fn;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}

static int xl6CgenFuncSupportsVarInvoke(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int fixedCount;
    size_t i;

    if ( ctx == NULL || func == NULL ) {
        return 0;
    }
    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    if ( params != NULL && params->childCount > (size_t)fixedCount &&
         !xl6CgenParamIsArrayVararg(params->children[fixedCount]) ) {
        return 0;
    }
    if ( returns != NULL && returns->childCount > 1 ) {
        return 0;
    }
    if ( returns != NULL && returns->childCount == 1 &&
         !xl6CgenTypeSupportsVarInvokeValue(ctx, returns->children[0]) ) {
        return 0;
    }
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( param == NULL || param->childCount == 0 ||
                 !xl6CgenTypeSupportsVarInvokeValue(ctx, param->children[0]) ) {
                return 0;
            }
        }
    }
    return 1;
}

static void xl6CgenEmitFunctionValueInvokeProto(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFunctionReturnStructName(ctx, params, returns);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }

    xl6OutputPutc(' ', ctx->out);
    xl6CgenFunctionValueSymbol(ctx, func, "_invoke");
    xl6OutputPuts("(void* __xl6_env", ctx->out);
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            xl6OutputPuts(", ", ctx->out);
            xl6OutputPuts(param != NULL && param->childCount > 0 ? xl6CgenValueStorageCType(ctx, param->children[0]) : "int", ctx->out);
            xl6OutputPutc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : "_");
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenEmitFunctionValueInvoke(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    const char* firstParamName;
    size_t i;

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    firstParamName = (params != NULL && params->childCount > 0 && params->children[0] != NULL) ?
        params->children[0]->text : "arg";

    xl6CgenFunctionReturnStructDecl(ctx, params, returns);
    xl6OutputPuts("static ", ctx->out);
    xl6CgenEmitFunctionValueInvokeProto(ctx, func);
    xl6OutputPuts("\n{\n", ctx->out);
    xl6OutputPuts("    (void)__xl6_env;\n", ctx->out);
    if ( xl6CgenIsPrintIntrinsicFunc(func) ) {
        xl6OutputPuts("    xl6_xrt_print(", ctx->out);
        xl6CgenFputsCIdent(ctx->out, firstParamName);
        xl6OutputPrintf(ctx->out, ", %d);\n", xl6CgenPrintIntrinsicNewline(func->children[0]->text));
        xl6OutputPuts("    if (", ctx->out);
        xl6CgenFputsCIdent(ctx->out, firstParamName);
        xl6OutputPuts(" != 0) { xvoUnref(", ctx->out);
        xl6CgenFputsCIdent(ctx->out, firstParamName);
        xl6OutputPuts("); ", ctx->out);
        xl6CgenFputsCIdent(ctx->out, firstParamName);
        xl6OutputPuts(" = 0; }\n", ctx->out);
        xl6OutputPuts("}\n\n", ctx->out);
        return;
    }
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6CgenFuncReturnStructName(ctx, func);
        xl6OutputPuts(" __xl6_src = ", ctx->out);
        xl6CgenCallableTargetSymbol(ctx, func);
        xl6OutputPutc('(', ctx->out);
        if ( params != NULL ) {
            for ( i = 0; i < params->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6OutputPuts(", ", ctx->out);
                }
                xl6CgenFputsCIdent(ctx->out, params->children[i] != NULL ? params->children[i]->text : "_");
            }
        }
        xl6OutputPuts(");\n", ctx->out);
        xl6OutputPuts("    ", ctx->out);
        xl6CgenFunctionReturnStructName(ctx, params, returns);
        xl6OutputPuts(" __xl6_out;\n", ctx->out);
        for ( i = 0; i < returns->childCount; ++i ) {
            xl6OutputPrintf(ctx->out, "    __xl6_out.value%u = __xl6_src.value%u;\n", (unsigned)i, (unsigned)i);
        }
        xl6OutputPuts("    return __xl6_out;\n", ctx->out);
        xl6OutputPuts("}\n\n", ctx->out);
        return;
    }
    xl6OutputPuts("    ", ctx->out);
    if ( returns != NULL && returns->childCount > 0 ) {
        xl6OutputPuts("return ", ctx->out);
    }
    xl6CgenCallableTargetSymbol(ctx, func);
    xl6OutputPutc('(', ctx->out);
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPuts(", ", ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, params->children[i] != NULL ? params->children[i]->text : "_");
        }
    }
    xl6OutputPuts(");\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenEmitFunctionValueVarInvoke(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int fixedCount;
    int hasArrayVararg;

    if ( !ctx->needsFunctionVarRuntime || !xl6CgenFuncSupportsVarInvoke(ctx, func) ) {
        return;
    }

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    hasArrayVararg = xl6CgenParamListHasArrayVararg(params, fixedCount);

    xl6OutputPuts("static xvalue ", ctx->out);
    xl6CgenFunctionValueSymbol(ctx, func, "_invoke_var");
    xl6OutputPuts("(void* __xl6_env, int __xl6_argc, xvalue* __xl6_argv)\n{\n", ctx->out);
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
    xl6CgenFunctionValueSymbol(ctx, func, "_invoke");
    xl6OutputPuts("(__xl6_env", ctx->out);
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

static void xl6CgenEmitFunctionValueXCall(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int fixedCount;
    int hasArrayVararg;
    int hasDictVararg;
    size_t i;

    if ( !ctx->needsFunctionVarRuntime || !xl6CgenFuncSupportsXCall(ctx, func) ) {
        return;
    }

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    hasArrayVararg = xl6CgenParamListHasArrayVararg(params, fixedCount);
    hasDictVararg = xl6CgenParamListHasDictVararg(params, fixedCount);

    xl6OutputPuts("static bool ", ctx->out);
    xl6CgenFunctionValueSymbol(ctx, func, "_xcall");
    xl6OutputPuts("(void* __xl6_env, xrt_call_frame* __xl6_frame, xrt_call_result* __xl6_result)\n{\n", ctx->out);
    xl6OutputPuts("    (void)__xl6_env;\n", ctx->out);
    xl6CgenEmitXCallLocals(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);
    xl6CgenEmitXCallSetup(ctx);
    xl6CgenEmitXCallParamSetup(ctx, params, fixedCount);
    xl6CgenEmitXCallArityCheck(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);
    xl6CgenEmitXCallTypeChecks(ctx, params, fixedCount);
    xl6CgenEmitXCallVarargPack(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);

    if ( xl6CgenIsPrintIntrinsicFunc(func) ) {
        xl6OutputPrintf(ctx->out, "    xl6_xrt_print(__xl6_vararg, %d);\n", xl6CgenPrintIntrinsicNewline(func->children[0]->text));
        xl6OutputPuts("    if (__xl6_vararg != 0) { xvoUnref(__xl6_vararg); __xl6_vararg = 0; }\n", ctx->out);
        xl6OutputPuts("    return true;\n", ctx->out);
        xl6OutputPuts("}\n\n", ctx->out);
        return;
    }

    if ( returns != NULL && returns->childCount > 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6CgenFuncReturnStructName(ctx, func);
        xl6OutputPuts(" __xl6_multi = ", ctx->out);
        xl6CgenCallableTargetSymbol(ctx, func);
        xl6OutputPutc('(', ctx->out);
        if ( params != NULL ) {
            for ( i = 0; i < params->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6OutputPuts(", ", ctx->out);
                }
                if ( (hasArrayVararg || hasDictVararg) && i == (size_t)fixedCount ) {
                    xl6OutputPuts("__xl6_vararg", ctx->out);
                } else {
                    xl6CgenEmitXCallArgOrDefault(ctx, params->children[i], (unsigned)i);
                }
            }
        }
        xl6OutputPuts(");\n", ctx->out);
        for ( i = 0; i < returns->childCount; ++i ) {
            char exprText[96];
            snprintf(exprText, sizeof(exprText), "__xl6_multi.value%u", (unsigned)i);
            xl6CgenEmitXCallSetResult(ctx, returns->children[i], (unsigned)i, exprText, hasArrayVararg, hasDictVararg);
        }
    } else if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[0]), ctx->out);
        xl6OutputPuts(" __xl6_single = ", ctx->out);
        xl6CgenCallableTargetSymbol(ctx, func);
        xl6OutputPutc('(', ctx->out);
        if ( params != NULL ) {
            for ( i = 0; i < params->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6OutputPuts(", ", ctx->out);
                }
                if ( (hasArrayVararg || hasDictVararg) && i == (size_t)fixedCount ) {
                    xl6OutputPuts("__xl6_vararg", ctx->out);
                } else {
                    xl6CgenEmitXCallArgOrDefault(ctx, params->children[i], (unsigned)i);
                }
            }
        }
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenEmitXCallSetResult(ctx, returns->children[0], 0u, "__xl6_single", hasArrayVararg, hasDictVararg);
    } else {
        xl6OutputPuts("    ", ctx->out);
        xl6CgenCallableTargetSymbol(ctx, func);
        xl6OutputPutc('(', ctx->out);
        if ( params != NULL ) {
            for ( i = 0; i < params->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6OutputPuts(", ", ctx->out);
                }
                if ( (hasArrayVararg || hasDictVararg) && i == (size_t)fixedCount ) {
                    xl6OutputPuts("__xl6_vararg", ctx->out);
                } else {
                    xl6CgenEmitXCallArgOrDefault(ctx, params->children[i], (unsigned)i);
                }
            }
        }
        xl6OutputPuts(");\n", ctx->out);
    }

    (void)hasArrayVararg;
    (void)hasDictVararg;
    xl6OutputPuts("    return true;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenEmitFunctionValueMake(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    unsigned long long signatureId;

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    signatureId = xl6CgenFunctionSignatureId(ctx, params, returns);

    xl6OutputPuts("static XL6_Function ", ctx->out);
    xl6CgenFunctionValueSymbol(ctx, func, "_make");
    xl6OutputPuts("(void)\n{\n", ctx->out);
    xl6OutputPuts("    XL6_Function __fn;\n", ctx->out);
    xl6CgenEmitFunctionSigObject(ctx, "__xl6_sig", func->text, signatureId, params, returns);
    xl6OutputPrintf(ctx->out, "    __fn.signature = 0x%016llXULL;\n", signatureId);
    xl6OutputPuts("    __fn.sig = &__xl6_sig;\n", ctx->out);
    xl6OutputPuts("    __fn.env = 0;\n", ctx->out);
    xl6OutputPuts("    __fn.invoke = (void*)", ctx->out);
    xl6CgenFunctionValueSymbol(ctx, func, "_invoke");
    xl6OutputPuts(";\n", ctx->out);
    if ( ctx->needsFunctionVarRuntime && xl6CgenFuncSupportsVarInvoke(ctx, func) ) {
        xl6OutputPuts("    __fn.invoke_var = ", ctx->out);
        xl6CgenFunctionValueSymbol(ctx, func, "_invoke_var");
        xl6OutputPuts(";\n", ctx->out);
    } else {
        xl6OutputPuts("    __fn.invoke_var = 0;\n", ctx->out);
    }
    if ( ctx->needsFunctionVarRuntime && xl6CgenFuncSupportsXCall(ctx, func) ) {
        xl6OutputPuts("    __fn.invoke_xcall = ", ctx->out);
        xl6CgenFunctionValueSymbol(ctx, func, "_xcall");
        xl6OutputPuts(";\n", ctx->out);
    } else {
        xl6OutputPuts("    __fn.invoke_xcall = 0;\n", ctx->out);
    }
    xl6OutputPuts("    __fn.retain = 0;\n", ctx->out);
    xl6OutputPuts("    __fn.release = 0;\n", ctx->out);
    xl6OutputPuts("    return __fn;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenEmitFunctionValueAdapterInvokeProto(XL6_CgenContext* ctx, const XL6_AstNode* func, int paramCount, int includeVararg)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    const XL6_AstNode* vararg;
    int fixedCount;
    int i;

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);

    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFunctionReturnStructNameForPrefix(ctx, params, returns, (size_t)paramCount, includeVararg);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }

    xl6OutputPutc(' ', ctx->out);
    xl6CgenFunctionValueAdapterSymbol(ctx, func, paramCount, includeVararg, "_invoke");
    xl6OutputPuts("(void* __xl6_env", ctx->out);
    for ( i = 0; params != NULL && i < paramCount && (size_t)i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPuts(param != NULL && param->childCount > 0 ? xl6CgenValueStorageCType(ctx, param->children[0]) : "int", ctx->out);
        xl6OutputPutc(' ', ctx->out);
        xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : "_");
    }
    if ( includeVararg && params != NULL && params->childCount > (size_t)fixedCount ) {
        vararg = params->children[fixedCount];
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPuts(vararg != NULL && vararg->childCount > 0 ? xl6CgenTypeName(ctx, vararg->children[0]) : "xvalue", ctx->out);
        xl6OutputPuts(" __xl6_vararg", ctx->out);
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenEmitFunctionValueAdapterInvoke(XL6_CgenContext* ctx, const XL6_AstNode* func, int paramCount, int includeVararg)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int fixedCount;
    int actualHasVararg;
    int i;

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    actualHasVararg = params != NULL && params->childCount > (size_t)fixedCount;

    xl6CgenFunctionReturnStructDeclForPrefix(ctx, params, returns, (size_t)paramCount, includeVararg);
    xl6OutputPuts("static ", ctx->out);
    xl6CgenEmitFunctionValueAdapterInvokeProto(ctx, func, paramCount, includeVararg);
    xl6OutputPuts("\n{\n", ctx->out);
    xl6OutputPuts("    (void)__xl6_env;\n", ctx->out);
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6CgenFuncReturnStructName(ctx, func);
        xl6OutputPuts(" __xl6_src = ", ctx->out);
    } else {
        xl6OutputPuts("    ", ctx->out);
        if ( returns != NULL && returns->childCount > 0 ) {
            xl6OutputPuts("return ", ctx->out);
        }
    }
    xl6CgenCallableTargetSymbol(ctx, func);
    xl6OutputPutc('(', ctx->out);
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params != NULL ? params->children[i] : NULL;
        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        if ( i < paramCount ) {
            xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : "_");
        } else if ( param != NULL && param->childCount > 1 ) {
            /* Fill omitted optional parameter with its default expression. */
            xl6CgenExpr(ctx, param->children[1]);
        } else {
            xl6CgenError(ctx, param, "optional adapter parameter is missing a default expression");
            xl6OutputPuts("0", ctx->out);
        }
    }
    if ( actualHasVararg ) {
        if ( fixedCount > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        if ( includeVararg ) {
            xl6OutputPuts("__xl6_vararg", ctx->out);
        } else {
            xl6CgenError(ctx, func, "callable adapter requires a vararg parameter");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        }
    }
    xl6OutputPuts(");\n", ctx->out);
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6CgenFunctionReturnStructNameForPrefix(ctx, params, returns, (size_t)paramCount, includeVararg);
        xl6OutputPuts(" __xl6_out;\n", ctx->out);
        for ( i = 0; i < (int)returns->childCount; ++i ) {
            xl6OutputPrintf(ctx->out, "    __xl6_out.value%d = __xl6_src.value%d;\n", i, i);
        }
        xl6OutputPuts("    return __xl6_out;\n", ctx->out);
    }
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenEmitFunctionValueAdapterXCall(XL6_CgenContext* ctx, const XL6_AstNode* func, int paramCount, int includeVararg)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int actualFixed;
    int hasArrayVararg;
    int hasDictVararg;
    int i;

    if ( !ctx->needsFunctionVarRuntime || !xl6CgenFuncSupportsXCall(ctx, func) ) {
        return;
    }

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    actualFixed = xl6CgenFixedParamCount(params);
    hasArrayVararg = includeVararg && params != NULL && params->childCount > (size_t)actualFixed &&
                     xl6CgenParamIsArrayVararg(params->children[actualFixed]);
    hasDictVararg = includeVararg && params != NULL && params->childCount > (size_t)actualFixed &&
                    xl6CgenParamIsDictVararg(params->children[actualFixed]);

    xl6OutputPuts("static bool ", ctx->out);
    xl6CgenFunctionValueAdapterSymbol(ctx, func, paramCount, includeVararg, "_xcall");
    xl6OutputPuts("(void* __xl6_env, xrt_call_frame* __xl6_frame, xrt_call_result* __xl6_result)\n{\n", ctx->out);
    xl6OutputPuts("    (void)__xl6_env;\n", ctx->out);
    xl6CgenEmitXCallLocals(ctx, params, paramCount, hasArrayVararg, hasDictVararg);
    xl6CgenEmitXCallSetup(ctx);
    xl6CgenEmitXCallParamSetup(ctx, params, paramCount);
    xl6CgenEmitXCallArityCheck(ctx, params, paramCount, hasArrayVararg, hasDictVararg);
    xl6CgenEmitXCallTypeChecks(ctx, params, paramCount);
    xl6CgenEmitXCallVarargPack(ctx, params, paramCount, hasArrayVararg, hasDictVararg);

    if ( returns != NULL && returns->childCount > 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6CgenFunctionReturnStructNameForPrefix(ctx, params, returns, (size_t)paramCount, includeVararg);
        xl6OutputPuts(" __xl6_multi = ", ctx->out);
        xl6CgenFunctionValueAdapterSymbol(ctx, func, paramCount, includeVararg, "_invoke");
        xl6OutputPuts("(__xl6_env", ctx->out);
        for ( i = 0; i < paramCount; ++i ) {
            xl6OutputPuts(", ", ctx->out);
            xl6CgenEmitXCallArgOrDefault(ctx, params != NULL ? params->children[i] : NULL, (unsigned)i);
        }
        if ( hasArrayVararg || hasDictVararg ) {
            xl6OutputPuts(", __xl6_vararg", ctx->out);
        }
        xl6OutputPuts(");\n", ctx->out);
        for ( i = 0; returns != NULL && i < (int)returns->childCount; ++i ) {
            char exprText[96];
            snprintf(exprText, sizeof(exprText), "__xl6_multi.value%d", i);
            xl6CgenEmitXCallSetResult(ctx, returns->children[i], (unsigned)i, exprText, hasArrayVararg, hasDictVararg);
        }
    } else if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[0]), ctx->out);
        xl6OutputPuts(" __xl6_single = ", ctx->out);
        xl6CgenFunctionValueAdapterSymbol(ctx, func, paramCount, includeVararg, "_invoke");
        xl6OutputPuts("(__xl6_env", ctx->out);
        for ( i = 0; i < paramCount; ++i ) {
            xl6OutputPuts(", ", ctx->out);
            xl6CgenEmitXCallArgOrDefault(ctx, params != NULL ? params->children[i] : NULL, (unsigned)i);
        }
        if ( hasArrayVararg || hasDictVararg ) {
            xl6OutputPuts(", __xl6_vararg", ctx->out);
        }
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenEmitXCallSetResult(ctx, returns->children[0], 0u, "__xl6_single", hasArrayVararg, hasDictVararg);
    } else {
        xl6OutputPuts("    ", ctx->out);
        xl6CgenFunctionValueAdapterSymbol(ctx, func, paramCount, includeVararg, "_invoke");
        xl6OutputPuts("(__xl6_env", ctx->out);
        for ( i = 0; i < paramCount; ++i ) {
            xl6OutputPuts(", ", ctx->out);
            xl6CgenEmitXCallArgOrDefault(ctx, params != NULL ? params->children[i] : NULL, (unsigned)i);
        }
        if ( hasArrayVararg || hasDictVararg ) {
            xl6OutputPuts(", __xl6_vararg", ctx->out);
        }
        xl6OutputPuts(");\n", ctx->out);
    }

    (void)hasArrayVararg;
    (void)hasDictVararg;
    xl6OutputPuts("    return true;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenEmitFunctionValueAdapterMake(XL6_CgenContext* ctx, const XL6_AstNode* func, int paramCount, int includeVararg)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    unsigned long long signatureId;

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    signatureId = xl6CgenFunctionSignatureIdForPrefix(ctx, params, returns, (size_t)paramCount, includeVararg);

    xl6OutputPuts("static XL6_Function ", ctx->out);
    xl6CgenFunctionValueAdapterSymbol(ctx, func, paramCount, includeVararg, "_make");
    xl6OutputPuts("(void)\n{\n", ctx->out);
    xl6OutputPuts("    XL6_Function __fn;\n", ctx->out);
    xl6CgenEmitFunctionSigObjectForPrefix(ctx,
                                          "__xl6_sig",
                                          func->text,
                                          signatureId,
                                          params,
                                          returns,
                                          (size_t)paramCount,
                                          includeVararg);
    xl6OutputPrintf(ctx->out, "    __fn.signature = 0x%016llXULL;\n", signatureId);
    xl6OutputPuts("    __fn.sig = &__xl6_sig;\n", ctx->out);
    xl6OutputPuts("    __fn.env = 0;\n", ctx->out);
    xl6OutputPuts("    __fn.invoke = (void*)", ctx->out);
    xl6CgenFunctionValueAdapterSymbol(ctx, func, paramCount, includeVararg, "_invoke");
    xl6OutputPuts(";\n", ctx->out);
    xl6OutputPuts("    __fn.invoke_var = 0;\n", ctx->out);
    if ( ctx->needsFunctionVarRuntime && xl6CgenFuncSupportsXCall(ctx, func) ) {
        xl6OutputPuts("    __fn.invoke_xcall = ", ctx->out);
        xl6CgenFunctionValueAdapterSymbol(ctx, func, paramCount, includeVararg, "_xcall");
        xl6OutputPuts(";\n", ctx->out);
    } else {
        xl6OutputPuts("    __fn.invoke_xcall = 0;\n", ctx->out);
    }
    xl6OutputPuts("    __fn.retain = 0;\n", ctx->out);
    xl6OutputPuts("    __fn.release = 0;\n", ctx->out);
    xl6OutputPuts("    return __fn;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenEmitFunctionValueOptionalAdapters(XL6_CgenContext* ctx, const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int requiredCount;
    int fixedCount;
    int hasVararg;
    int count;

    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    requiredCount = xl6CgenParamRequiredCount(params);
    fixedCount = xl6CgenFixedParamCount(params);
    if ( requiredCount >= fixedCount ) {
        return;
    }
    hasVararg = params != NULL && params->childCount > (size_t)fixedCount;
    for ( count = requiredCount; count < fixedCount; ++count ) {
        int includeVararg = hasVararg ? 1 : 0;
        xl6CgenEmitFunctionValueAdapterInvoke(ctx, func, count, includeVararg);
        xl6CgenEmitFunctionValueAdapterXCall(ctx, func, count, includeVararg);
        xl6CgenEmitFunctionValueAdapterMake(ctx, func, count, includeVararg);
    }
}

static void xl6CgenEmitFunctionValueWrappers(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* func = ctx->root->children[i];
        if ( func == NULL ||
             (func->kind != XL6_AST_FUNC && func->kind != XL6_AST_EXTERN_FUNC && func->kind != XL6_AST_EXPORT_FUNC) ||
             xl6CgenFuncIsGeneric(func) ) {
            continue;
        }
        xl6CgenEmitFunctionValueInvoke(ctx, func);
        xl6CgenEmitFunctionValueVarInvoke(ctx, func);
        xl6CgenEmitFunctionValueXCall(ctx, func);
        xl6CgenEmitFunctionValueMake(ctx, func);
        xl6CgenEmitFunctionValueOptionalAdapters(ctx, func);
    }
}

static void xl6CgenEmitAnonFunctions(XL6_CgenContext* ctx)
{
    size_t i;

    for ( i = 0; i < ctx->anonFuncCount; ++i ) {
        xl6CgenEmitAnonFunction(ctx, &ctx->anonFuncs[i]);
    }
}
