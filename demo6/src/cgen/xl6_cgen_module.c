int xl6CgenEmitC(XL6_Output* out, XL6_AstNode* root, XL6_DiagList* diags)
{
    return xl6CgenEmitCEx(out, root, diags, NULL);
}

int xl6CgenEmitCEx(XL6_Output* out, XL6_AstNode* root, XL6_DiagList* diags, const XL6_CgenOptions* options)
{
    XL6_CgenContext ctx;
    size_t i;
    int hasTopLevel;
    int hasModuleGlobals;
    int hasEntryRuntime;
    int hasStringRuntime;
    int hasTimeRuntime;
    int hasTypeConvertRuntime;
    int hasFunctionRuntime;
    int hasFunctionVarRuntime;
    int hasXValueRuntime;
    int hasRefRuntime;
    int hasErrorRuntime;
    int hasDynamicForInErrorRuntime;
    int hasAsyncRuntime;
    int hasCryptoRuntime;
    int hasUrlRuntime;
    int hasQueryRuntime;
    int hasRegexRuntime;
    int hasProcessRuntime;
    int hasDirRuntime;
    int hasStringEdgeRuntime;
    int hasStdStringRuntime;
    int hasPathRuntime;
    int hasHttpServerRuntime;
    int hasHttpClientRuntime;
    int hasBufferRuntime;
    int hasEncodingRuntime;
    int hasLogRuntime;
    int hasTemplateRuntime;
    int hasFileRuntime;
    int hasAsyncFileRuntime;
    int hasTccRuntime;
    int hasXlangRuntime;
    int hasTaskSpawn;
    int hasPointMove;
    int needsObjectClass;
    const XL6_AstNode* mainFunc;

    if ( out == NULL || root == NULL ) {
        return 0;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.out = out;
    ctx.root = root;
    ctx.diags = diags;
    ctx.ok = 1;
    if ( options != NULL ) {
        ctx.debugMode = options->debugMode;
        ctx.debugSourcePath = options->debugSourcePath;
    }

    xl6CgenCollectAnonFuncs(&ctx, root);
    xl6CgenCollectGenericInstancesToFixedPoint(&ctx);
    xl6CgenCollectRuntimeTypeDescs(&ctx, root);
    xl6CgenFreeVarsSince(&ctx, 0);
    mainFunc = xl6CgenFindMainEntry(root);
    needsObjectClass = xl6CgenNodeNeedsBuiltinObjectClass(root);
    hasTopLevel = xl6CgenHasTopLevelRuntimeStmt(root);
    hasModuleGlobals = xl6CgenHasModuleGlobalDecl(root);
    hasEntryRuntime = hasTopLevel || hasModuleGlobals;
    hasStringRuntime = xl6CgenProgramNeedsStringRuntime(&ctx, root);
    hasTypeConvertRuntime = xl6CgenProgramNeedsTypeConvertRuntime(&ctx, root);
    hasFunctionRuntime = xl6CgenNodeNeedsFunctionRuntime(root);
    hasFunctionVarRuntime = xl6CgenProgramNeedsFunctionVarRuntime(&ctx, root);
    hasRefRuntime = xl6CgenNodeNeedsRefRuntime(root);
    hasXValueRuntime = xl6CgenNodeNeedsXValueRuntime(root) || hasFunctionVarRuntime ||
                       ctx.runtimeTypeDescCount > 0 || hasRefRuntime;
    hasTimeRuntime = xl6CgenNodeNeedsTimeRuntime(root);
    hasAsyncRuntime = xl6CgenNodeNeedsAsyncRuntime(root);
    hasTaskSpawn = xl6CgenNodeNeedsTaskSpawn(root);
    hasPointMove = xl6CgenNodeNeedsPointMove(&ctx, root);
    hasDynamicForInErrorRuntime = xl6CgenProgramNeedsDynamicForInErrorRuntime(&ctx, root);
    hasErrorRuntime = xl6CgenNodeNeedsErrorRuntime(root) || hasAsyncRuntime ||
                      hasDynamicForInErrorRuntime || hasTypeConvertRuntime || hasRefRuntime;
    hasCryptoRuntime = xl6CgenNodeNeedsCryptoRuntime(root);
    hasUrlRuntime = xl6CgenNodeNeedsUrlRuntime(root);
    hasQueryRuntime = xl6CgenNodeNeedsQueryRuntime(root);
    hasRegexRuntime = xl6CgenNodeNeedsRegexRuntime(root);
    hasProcessRuntime = xl6CgenNodeNeedsProcessRuntime(root);
    hasDirRuntime = xl6CgenNodeNeedsDirRuntime(root);
    hasStringEdgeRuntime = xl6CgenNodeNeedsStringEdgeRuntime(root);
    hasStdStringRuntime = xl6CgenNodeNeedsStdStringRuntime(root);
    hasPathRuntime = xl6CgenNodeNeedsPathRuntime(root);
    hasHttpServerRuntime = xl6CgenNodeNeedsHttpServerRuntime(root);
    hasHttpClientRuntime = xl6CgenNodeNeedsHttpClientRuntime(root);
    hasBufferRuntime = xl6CgenNodeNeedsBufferRuntime(root);
    if ( hasHttpServerRuntime || hasHttpClientRuntime ) {
        /*
            HTTP 请求/响应 helper 会暴露 Buffer 视图复制函数。
            即使用户代码没有显式声明 Buffer，也必须生成这组运行时封装。
        */
        hasBufferRuntime = 1;
    }
    hasEncodingRuntime = xl6CgenNodeNeedsEncodingRuntime(root);
    hasLogRuntime = xl6CgenNodeNeedsLogRuntime(root);
    hasTemplateRuntime = xl6CgenNodeNeedsTemplateRuntime(root);
    hasFileRuntime = xl6CgenNodeNeedsFileRuntime(root);
    hasAsyncFileRuntime = xl6CgenNodeNeedsAsyncFileRuntime(root);
    hasTccRuntime = xl6CgenNodeNeedsTccRuntime(root);
    hasXlangRuntime = xl6CgenNodeNeedsXlangRuntime(root);
    if ( hasAsyncFileRuntime ) {
        hasAsyncRuntime = 1;
        hasErrorRuntime = 1;
    }
    if ( hasFileRuntime ) {
        hasErrorRuntime = 1;
    }
    if ( hasStdStringRuntime || hasDirRuntime ) {
        hasXValueRuntime = 1;
    }
    if ( hasTemplateRuntime ) {
        hasXValueRuntime = 1;
    }
    if ( hasXlangRuntime ) {
        hasStringRuntime = 1;
    }
    if ( needsObjectClass ) {
        hasXValueRuntime = 1;
    }
    if ( hasHttpServerRuntime ) {
        hasFunctionVarRuntime = 1;
        hasXValueRuntime = 1;
    }
    if ( hasTaskSpawn ) {
        hasFunctionVarRuntime = 1;
        hasXValueRuntime = 1;
    }
    if ( hasAsyncRuntime && hasFunctionRuntime ) {
        hasFunctionVarRuntime = 1;
        hasXValueRuntime = 1;
    }
    ctx.needsXValueRuntime = hasXValueRuntime;
    ctx.needsFunctionVarRuntime = hasFunctionVarRuntime;
    ctx.needsErrorRuntime = hasErrorRuntime;
    ctx.wrapExplicitMainWithTopLevel = (mainFunc != NULL && hasEntryRuntime);

    xl6OutputPuts("/* Generated by xlang DEMO6 cgen. */\n\n", out);
    xl6OutputPuts("#include <stdbool.h>\n", out);
    xl6OutputPuts("#include <stdint.h>\n", out);
    xl6OutputPuts("#include <stdlib.h>\n", out);
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("#include <string.h>\n\n", out);
    if ( ctx.debugMode ) {
        xl6OutputPuts("#include <stdio.h>\n\n", out);
    }
    xl6OutputPuts("typedef intptr_t xl6_ssize_t;\n\n", out);
    xl6OutputPuts("#define XL_RUNTIME_NEED_LIFECYCLE 1\n", out);
    if ( hasPointMove ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_POINT_MOVE 1\n", out);
    }
    if ( hasStringRuntime ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_STRING 1\n", out);
    }
    if ( hasXValueRuntime ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_XVALUE 1\n", out);
    }
    if ( mainFunc != NULL || hasEntryRuntime ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_ARGV 1\n", out);
    }
    /*
        xl_runtime.h 是 xlang 生成代码的内部运行时头。
        使用专用 VFS 路径，避免 release/include 下的外部残留文件覆盖内置版本。
    */
    xl6OutputPuts("#include <__xl_runtime/xl_runtime.h>\n\n", out);
    xl6CgenEmitTempScopeRuntime(out);
    if ( ctx.debugMode ) {
        xl6CgenEmitDebugRuntime(out);
    }
    if ( hasFunctionRuntime || hasHttpServerRuntime || hasTaskSpawn || hasFunctionVarRuntime ) {
        xl6CgenEmitFunctionRuntime(out);
    }
    if ( xl6CgenNodeNeedsApprox(root) ) {
        xl6CgenEmitApproxProtos(out);
    }
    if ( hasTypeConvertRuntime ) {
        xl6CgenEmitTypeConvertRuntime(out);
    }
    if ( hasTimeRuntime ) {
        xl6CgenEmitTimeRuntime(out);
    }
    if ( hasErrorRuntime ) {
        xl6CgenEmitErrorRuntime(out);
    }
    if ( hasRefRuntime ) {
        xl6CgenEmitRefRuntime(out);
    }
    if ( hasAsyncRuntime ) {
        xl6CgenEmitAsyncRuntime(out, hasTaskSpawn);
    }
    if ( hasAsyncFileRuntime ) {
        xl6CgenEmitAsyncFileRuntime(out);
    }
    if ( hasFileRuntime ) {
        xl6CgenEmitFileRuntime(out);
    }
    if ( hasXValueRuntime ) {
        xl6CgenEmitRuntimeTypeDescs(&ctx);
    }
    if ( hasCryptoRuntime ) {
        xl6CgenEmitCryptoRuntime(out);
    }
    if ( hasUrlRuntime ) {
        xl6CgenEmitUrlRuntime(out);
    }
    if ( hasQueryRuntime ) {
        xl6CgenEmitQueryRuntime(out);
    }
    if ( hasRegexRuntime ) {
        xl6CgenEmitRegexRuntime(out);
    }
    if ( hasProcessRuntime ) {
        xl6CgenEmitProcessRuntime(out);
    }
    if ( hasDirRuntime ) {
        xl6CgenEmitDirRuntime(out);
    }
    if ( hasStringEdgeRuntime ) {
        xl6CgenEmitStringEdgeRuntime(out);
    }
    if ( hasStdStringRuntime ) {
        xl6CgenEmitStdStringRuntime(out);
    }
    if ( hasPathRuntime ) {
        xl6CgenEmitPathRuntime(out);
    }
    if ( hasFunctionVarRuntime ) {
        xl6CgenEmitFunctionVarRuntime(out, hasTaskSpawn);
    }
    if ( hasBufferRuntime ) {
        xl6CgenEmitBufferRuntime(out);
    }
    if ( hasHttpServerRuntime ) {
        xl6CgenEmitHttpServerRuntime(out);
    }
    if ( hasHttpClientRuntime ) {
        xl6CgenEmitHttpClientRuntime(out);
    }
    if ( hasEncodingRuntime ) {
        xl6CgenEmitEncodingRuntime(out);
    }
    if ( hasLogRuntime ) {
        xl6CgenEmitLogRuntime(out);
    }
    if ( hasTemplateRuntime ) {
        xl6CgenEmitTemplateRuntime(out);
    }
    if ( hasTccRuntime ) {
        xl6CgenEmitTccRuntime(out);
    }
    if ( hasXlangRuntime ) {
        xl6CgenEmitXlangRuntime(out);
    }
    if ( xl6CgenNodeNeedsIntPow(root) ) {
        xl6CgenEmitIntPowHelper(out);
    }
    if ( xl6CgenNodeNeedsRangeContains(root) ) {
        xl6CgenEmitRangeContainsHelper(out);
    }
    xl6CgenClassForwardDecls(&ctx, root);
    xl6CgenCollectGenericClassDecls(&ctx, root);

    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_UNION ) {
            xl6CgenUnionDecls(&ctx, root->children[i]);
        }
    }
    xl6CgenRegisterModuleGlobalTypes(&ctx, root);
    xl6CgenEmitModuleGlobalForwardDecls(&ctx, root);

    /*
        class 默认字段初始化会被写入默认构造 helper，而该 helper 早于普通函数定义。
        单返回值函数不依赖多返回值结构体，可在 class 结构体前向声明之后安全提前声明。
    */
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* child = root->children[i];
        const XL6_AstNode* returns;

        if ( child == NULL ) {
            continue;
        }
        returns = child->childCount > 2 ? child->children[2] : NULL;
        if ( returns != NULL && returns->childCount > 1 ) {
            continue;
        }
        if ( child->kind == XL6_AST_FUNC &&
             !xl6CgenFuncIsGeneric(child) &&
             !xl6CgenIsPrintIntrinsicFunc(child) ) {
            xl6CgenFuncProto(&ctx, child);
            xl6OutputPuts(";\n", out);
        } else if ( child->kind == XL6_AST_ASYNC_FUNC && !xl6CgenFuncIsGeneric(child) ) {
            if ( xl6CgenFuncReturnsGenerator(&ctx, child) ) {
                xl6CgenFuncProto(&ctx, child);
            } else {
                xl6CgenAsyncFuncProto(&ctx, child);
            }
            xl6OutputPuts(";\n", out);
        } else if ( child->kind == XL6_AST_EXPORT_FUNC ) {
            xl6CgenExportInternalProto(&ctx, child);
            xl6OutputPuts(";\n", out);
        } else if ( child->kind == XL6_AST_EXTERN_FUNC && !xl6CgenIsRuntimeHeaderExtern(child) ) {
            xl6CgenExternFuncProto(&ctx, child);
            xl6OutputPuts(";\n", out);
        }
    }
    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_CLASS &&
             (!xl6CgenIsBuiltinObjectClass(root->children[i]) || needsObjectClass) ) {
            xl6CgenClassDecls(&ctx, root->children[i]);
        }
    }
    if ( hasXValueRuntime && xl6CgenNodeNeedsRuntimeTypeOps(root) ) {
        xl6CgenEmitRuntimeTypeHelpers(&ctx);
    }
    if ( hasXValueRuntime ) {
        xl6CgenEmitRecordFieldHelpers(&ctx);
        xl6CgenEmitProgramTypeNameHelper(&ctx);
    }
    for ( i = 0; i < root->childCount; ++i ) {
        if ( (root->children[i]->kind == XL6_AST_FUNC || root->children[i]->kind == XL6_AST_EXPORT_FUNC) &&
             !xl6CgenFuncIsGeneric(root->children[i]) &&
             !xl6CgenIsPrintIntrinsicFunc(root->children[i]) ) {
            xl6CgenFuncReturnStructDecl(&ctx, root->children[i]);
        }
    }
    for ( i = 0; i < ctx.genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx.currentGenericParams;
        const XL6_AstNode* prevArgs = ctx.currentGenericArgs;

        ctx.currentGenericParams = xl6CgenFuncTypeParams(ctx.genericInstances[i].func);
        ctx.currentGenericArgs = ctx.genericInstances[i].typeArgs;
        xl6CgenFuncReturnStructDecl(&ctx, ctx.genericInstances[i].func);
        ctx.currentGenericParams = prevParams;
        ctx.currentGenericArgs = prevArgs;
    }
    xl6CgenFunctionReturnStructDecls(&ctx, root);
    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_EXTERN_FUNC ) {
            if ( !xl6CgenIsRuntimeHeaderExtern(root->children[i]) ) {
                xl6CgenExternFuncProto(&ctx, root->children[i]);
                xl6OutputPuts(";\n", out);
            }
            if ( xl6CgenExternCallNeedsWrapper(&ctx, root->children[i]) ) {
                xl6CgenExternCallWrapperProto(&ctx, root->children[i]);
                xl6OutputPuts(";\n", out);
            }
        } else if ( root->children[i]->kind == XL6_AST_EXTERN_VAR ) {
            xl6CgenExternVarDecl(&ctx, root->children[i]);
        } else if ( root->children[i]->kind == XL6_AST_EXPORT_FUNC ) {
            xl6CgenExportInternalProto(&ctx, root->children[i]);
            xl6OutputPuts(";\n", out);
            if ( root->children[i]->text != NULL && root->children[i]->text[0] != '\0' ) {
                xl6CgenExportWrapperProto(&ctx, root->children[i]);
                xl6OutputPuts(";\n", out);
            }
        } else if ( root->children[i]->kind == XL6_AST_EXPORT_VAR ||
                    root->children[i]->kind == XL6_AST_EXPORT_CONST ) {
            if ( xl6CgenExternalVarHasCSymbol(root->children[i]) ) {
                xl6CgenExportVarDef(&ctx, root->children[i]);
            }
        } else if ( root->children[i]->kind == XL6_AST_FUNC &&
                    !xl6CgenFuncIsGeneric(root->children[i]) &&
                    !xl6CgenIsPrintIntrinsicFunc(root->children[i]) ) {
            xl6CgenFuncProto(&ctx, root->children[i]);
            xl6OutputPuts(";\n", out);
        } else if ( root->children[i]->kind == XL6_AST_ASYNC_FUNC && !xl6CgenFuncIsGeneric(root->children[i]) ) {
            if ( xl6CgenFuncReturnsGenerator(&ctx, root->children[i]) ) {
                xl6CgenFuncProto(&ctx, root->children[i]);
            } else {
                xl6CgenAsyncFuncProto(&ctx, root->children[i]);
            }
            xl6OutputPuts(";\n", out);
        }
    }
    for ( i = 0; i < ctx.genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx.currentGenericParams;
        const XL6_AstNode* prevArgs = ctx.currentGenericArgs;

        ctx.currentGenericParams = xl6CgenFuncTypeParams(ctx.genericInstances[i].func);
        ctx.currentGenericArgs = ctx.genericInstances[i].typeArgs;
        if ( xl6CgenIsAsyncFunc(ctx.genericInstances[i].func) &&
             !xl6CgenFuncReturnsGenerator(&ctx, ctx.genericInstances[i].func) ) {
            xl6CgenAsyncFuncProto(&ctx, ctx.genericInstances[i].func);
        } else {
            xl6CgenFuncProto(&ctx, ctx.genericInstances[i].func);
        }
        xl6OutputPuts(";\n", out);
        ctx.currentGenericParams = prevParams;
        ctx.currentGenericArgs = prevArgs;
    }
    xl6CgenGenericClassMethodProtos(&ctx);
    xl6CgenClassProtocolWrapperProtos(&ctx);
    xl6CgenEmitModuleGlobalDecls(&ctx, root);
    if ( root->childCount > 0 ) {
        xl6OutputPutc('\n', out);
    }

    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_EXTERN_FUNC ) {
            xl6CgenExternCallWrapper(&ctx, root->children[i]);
        }
    }

    if ( hasFunctionRuntime || hasTaskSpawn || hasFunctionVarRuntime ) {
        xl6CgenEmitFunctionValueWrappers(&ctx);
    }
    xl6CgenEmitAnonFunctions(&ctx);

    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_CLASS &&
             (!xl6CgenIsBuiltinObjectClass(root->children[i]) || needsObjectClass) ) {
            xl6CgenClassMethods(&ctx, root->children[i]);
        }
    }

    xl6CgenGenericClassMethods(&ctx);
    xl6CgenClassProtocolWrappers(&ctx);

    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_FUNC &&
             !xl6CgenFuncIsGeneric(root->children[i]) &&
             !xl6CgenIsPrintIntrinsicFunc(root->children[i]) ) {
            if ( xl6CgenFuncReturnsGenerator(&ctx, root->children[i]) ) {
                xl6CgenGeneratorFunc(&ctx, root->children[i]);
            } else {
                xl6CgenFunc(&ctx, root->children[i]);
            }
        } else if ( root->children[i]->kind == XL6_AST_ASYNC_FUNC && !xl6CgenFuncIsGeneric(root->children[i]) ) {
            if ( xl6CgenFuncReturnsGenerator(&ctx, root->children[i]) ) {
                xl6CgenGeneratorFunc(&ctx, root->children[i]);
            } else {
                xl6CgenAsyncFunc(&ctx, root->children[i]);
            }
        } else if ( root->children[i]->kind == XL6_AST_EXPORT_FUNC ) {
            xl6CgenExportFunc(&ctx, root->children[i]);
            if ( root->children[i]->text != NULL && root->children[i]->text[0] != '\0' ) {
                xl6CgenExportWrapper(&ctx, root->children[i]);
            }
        }
    }
    for ( i = 0; i < ctx.genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx.currentGenericParams;
        const XL6_AstNode* prevArgs = ctx.currentGenericArgs;

        ctx.currentGenericParams = xl6CgenFuncTypeParams(ctx.genericInstances[i].func);
        ctx.currentGenericArgs = ctx.genericInstances[i].typeArgs;
        if ( xl6CgenFuncReturnsGenerator(&ctx, ctx.genericInstances[i].func) ) {
            xl6CgenGeneratorFunc(&ctx, ctx.genericInstances[i].func);
        } else if ( xl6CgenIsAsyncFunc(ctx.genericInstances[i].func) ) {
            xl6CgenAsyncFunc(&ctx, ctx.genericInstances[i].func);
        } else {
            xl6CgenFunc(&ctx, ctx.genericInstances[i].func);
        }
        ctx.currentGenericParams = prevParams;
        ctx.currentGenericArgs = prevArgs;
    }

    if ( hasEntryRuntime ) {
        xl6CgenEmitTopLevelXlangMain(&ctx, root, mainFunc, hasErrorRuntime);
    }
    if ( (mainFunc != NULL || hasEntryRuntime) && (options == NULL || !options->suppressMainWrapper) ) {
        xl6CgenEmitMainWrapper(&ctx);
    }
    if ( (mainFunc != NULL || hasEntryRuntime) && options != NULL && options->suppressMainWrapper ) {
        xl6CgenEmitScriptErrorBridge(&ctx);
    }

    hasTopLevel = ctx.ok && (diags == NULL || diags->count == 0);
    xl6CgenFreeVars(&ctx);
    xl6CgenFreeEmittedClasses(&ctx);
    xl6CgenFreeEmittedUnions(&ctx);
    xl6CgenFreeFunctionReturnStructs(&ctx);
    xl6CgenFreeCleanups(&ctx);
    xl6CgenFreeLoopTargets(&ctx);
    xl6CgenFreeTryTargets(&ctx);
    xl6CgenFreeFinallyTargets(&ctx);
    xl6CgenFreeGenericInstances(&ctx);
    xl6CgenFreeGenericClassInstances(&ctx);
    xl6CgenFreeRuntimeTypeDescs(&ctx);
    xl6CgenFreeAnonFuncs(&ctx);
    xl6CgenFreeOwnedTypeNodes(&ctx);
    return hasTopLevel;
}
