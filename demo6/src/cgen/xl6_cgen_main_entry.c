static int xl6CgenEmitModuleGlobalClassArgTempInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* initExpr,
    const char* cName)
{
    XL6_CgenDeclClassArgTemp* exprTemps;
    XL6_CgenExprReplacement* exprReplacements;
    const XL6_CgenExprReplacement* prevExprReplacements;
    size_t prevExprReplacementCount;
    int exprTempCount;
    int exprTempCapacity;

    if ( ctx == NULL || node == NULL || typeNode == NULL || initExpr == NULL || cName == NULL ||
         xl6CgenIsFunctionType(typeNode) || xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        return 0;
    }

    exprTemps = NULL;
    exprReplacements = NULL;
    exprTempCount = 0;
    exprTempCapacity = 0;
    xl6CgenDeclCollectClassArgTempsInExpr(ctx, initExpr, &exprTemps, &exprTempCount, &exprTempCapacity);
    if ( exprTempCount <= 0 ) {
        xl6CgenDeclFreeClassArgTemps(exprTemps, exprTempCount);
        return 0;
    }

    exprReplacements = xl6CgenDeclCreateExprReplacements(ctx, exprTemps, exprTempCount);
    if ( exprReplacements == NULL ) {
        xl6CgenDeclFreeClassArgTemps(exprTemps, exprTempCount);
        return 0;
    }

    prevExprReplacements = ctx->exprReplacements;
    prevExprReplacementCount = ctx->exprReplacementCount;
    xl6CgenDeclEmitClassArgTemps(ctx, exprTemps, exprTempCount, 0);
    ctx->exprReplacements = exprReplacements;
    ctx->exprReplacementCount = (size_t)exprTempCount;
    if ( xl6CgenClassHandleTargetType(ctx, typeNode) != NULL ) {
        if ( !xl6CgenTryClassCtorBoxXValueStmt(ctx, typeNode, initExpr, cName) ) {
            xl6CgenError(ctx, initExpr, "module global class constructor boxing failed");
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xl6_xrt_move_to_shared(%s);\n", cName, cName);
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPuts(cName, ctx->out);
        xl6OutputPuts(" = ", ctx->out);
        if ( xl6CgenIsXValueType(typeNode) ) {
            xl6CgenXValueFromExprForTarget(ctx, typeNode, initExpr);
        } else {
            xl6CgenExpr(ctx, initExpr);
        }
        xl6OutputPuts(";\n", ctx->out);
    }
    ctx->exprReplacements = prevExprReplacements;
    ctx->exprReplacementCount = prevExprReplacementCount;
    xl6CgenDeclDestroyClassArgTemps(ctx, exprTemps, exprTempCount);
    xrtFree(exprReplacements);
    xl6CgenDeclFreeClassArgTemps(exprTemps, exprTempCount);
    xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, typeNode, cName, initExpr);
    return 1;
}

static int xl6CgenEmitModuleGlobalNativeClassInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* initExpr,
    const char* cName)
{
    const XL6_AstNode* nativeClassNode;
    XL6_AstNode* ctorNativeClass;
    const char* name;

    if ( ctx == NULL || node == NULL || typeNode == NULL || cName == NULL ||
         !xl6CgenIsNativeBackedClassType(ctx, typeNode) ) {
        return 0;
    }
    name = xl6CgenModuleGlobalName(node);

    nativeClassNode = xl6CgenFindNativeBackedClass(ctx, typeNode->text);
    if ( nativeClassNode == NULL ) {
        return 0;
    }
    if ( initExpr == NULL ) {
        return 1;
    }

    ctorNativeClass = NULL;
    if ( xl6CgenCallIsNativeBackedCtor(ctx, initExpr, &ctorNativeClass) ) {
        if ( ctorNativeClass == NULL || !xl6CTextEquals(ctorNativeClass->text, nativeClassNode->text) ) {
            xl6CgenError(ctx, initExpr, "module global native-backed class constructor type does not match declared type");
            return 1;
        }
        xl6CgenNativeBackedCtorAssign(ctx, nativeClassNode, name, initExpr);
        return 1;
    }

    {
        XL6_AstNode* initType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
        const XL6_AstNode* resolvedInitType = xl6CgenResolveAliasType(ctx, initType);

        if ( resolvedInitType != NULL &&
             xl6CTextEquals(resolvedInitType->text, nativeClassNode->text) &&
             (!xl6CgenNativeBackedClassHasDestroy(nativeClassNode) ||
              (initExpr != NULL && initExpr->kind == XL6_AST_CALL)) ) {
            xl6CgenIndent(ctx);
            xl6OutputPuts(cName, ctx->out);
            xl6OutputPuts(" = ", ctx->out);
            xl6CgenExpr(ctx, initExpr);
            xl6OutputPuts(";\n", ctx->out);
        } else {
            xl6CgenError(ctx, initExpr, "module global native-backed class variable must be initialized by same-type constructor or copyable same-type value");
        }
        xl6AstFree(initType);
    }
    return 1;
}

static int xl6CgenEmitModuleGlobalClassInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* initExpr,
    const char* cName)
{
    XL6_AstNode* classNode;
    XL6_AstNode* ctorClass;
    XL6_AstNode* ctorClassType;
    XL6_AstNode* canonicalType;
    const XL6_AstNode* classTypeNode;
    const XL6_AstNode* args;
    const XL6_AstNode* createNode;
    const XL6_AstNode* createParams;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    XL6_CgenClassExprTempScope ctorArgScope;
    int isGenericClass;
    int hasCtorArgScope;

    if ( ctx == NULL || node == NULL || typeNode == NULL || cName == NULL ||
         !xl6CgenIsClassType(ctx, typeNode) ) {
        return 0;
    }

    classNode = xl6CgenClassNodeForType(ctx, typeNode);
    if ( classNode == NULL ) {
        return 0;
    }

    canonicalType = NULL;
    classTypeNode = typeNode;
    isGenericClass = typeNode->childCount > 0 && xl6CgenClassHasTypeParams(classNode);
    if ( isGenericClass ) {
        canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
        if ( canonicalType == NULL ) {
            xl6CgenError(ctx, typeNode, "module global generic class type could not be resolved");
            return 1;
        }
        xl6CgenOwnTypeNode(ctx, canonicalType);
        classTypeNode = canonicalType;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( isGenericClass ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = classTypeNode->children[0];
    }

    xl6CgenClassDefaultInitFields(ctx, classNode, cName);

    if ( initExpr == NULL ) {
        createNode = xl6CgenFindClassCreateCallableWithoutArgs(classNode);
        if ( createNode != NULL ) {
            const XL6_AstNode* createParams;

            /*
                模块级 `T x` 和局部 `T x` 保持一致：
                如果类声明了无必选参数的 create，就必须完成构造调用。
            */
            xl6CgenIndent(ctx);
            if ( isGenericClass ) {
                xl6CgenGenericClassCallableSymbol(ctx, classNode, classTypeNode, createNode);
            } else {
                xl6CgenClassCallableSymbol(ctx, classNode, createNode);
            }
            xl6OutputPrintf(ctx->out, "(&%s", cName);
            createParams = xl6CgenCallableParams(createNode);
            if ( xl6CgenFixedParamCount(createParams) > 0 ) {
                xl6OutputPuts(", ", ctx->out);
                xl6CgenParamNodeArgs(ctx, createParams, NULL);
            }
            xl6OutputPuts(");\n", ctx->out);
        } else if ( xl6CgenClassHasCreate(classNode) ) {
            xl6CgenError(ctx, node, "module global class variable requires explicit constructor arguments");
        } else {
            xl6CgenEmitAutoBaseCreateForValue(ctx, classNode, cName, node);
        }
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        return 1;
    }

    ctorClass = NULL;
    ctorClassType = NULL;
    if ( isGenericClass ) {
        ctorClassType = xl6CgenGenericClassCtorType(ctx, initExpr, &ctorClass);
        if ( ctorClassType == NULL ) {
            XL6_AstNode* initType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
            const XL6_AstNode* resolvedInitType = xl6CgenResolveAliasType(ctx, initType);
            int sameType = resolvedInitType != NULL && xl6CgenTypeNodeEqual(classTypeNode, resolvedInitType);

            if ( !sameType ) {
                xl6AstFree(initType);
                ctx->currentGenericParams = prevParams;
                ctx->currentGenericArgs = prevArgs;
                return 0;
            }
            if ( !xl6CgenDeclEmitXValueCallExprWithArgTemps(ctx, classTypeNode, initExpr, cName) ) {
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "%s = ", cName);
                xl6CgenExpr(ctx, initExpr);
                xl6OutputPuts(";\n", ctx->out);
            }
            xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, classTypeNode, cName, initExpr);
            xl6AstFree(initType);
            ctx->currentGenericParams = prevParams;
            ctx->currentGenericArgs = prevArgs;
            return 1;
        }
        if ( !xl6CgenTypeNodeEqual(classTypeNode, ctorClassType) ) {
            xl6CgenError(ctx, initExpr, "module global generic class constructor type does not match declared type");
            xl6AstFree(ctorClassType);
            ctx->currentGenericParams = prevParams;
            ctx->currentGenericArgs = prevArgs;
            return 1;
        }
        xl6AstFree(ctorClassType);
    } else if ( !xl6CgenCallIsClassCtor(ctx, initExpr, &ctorClass) ) {
        XL6_AstNode* initType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
        const XL6_AstNode* resolvedInitType = xl6CgenResolveAliasType(ctx, initType);
        int sameType = resolvedInitType != NULL && xl6CTextEquals(resolvedInitType->text, classNode->text);
        int upcastInit = !sameType && xl6CgenExprNeedsClassUpcast(ctx, classTypeNode, initExpr, NULL, 0, NULL, NULL);

        if ( !sameType && !upcastInit ) {
            xl6AstFree(initType);
            ctx->currentGenericParams = prevParams;
            ctx->currentGenericArgs = prevArgs;
            return 0;
        }
        if ( upcastInit ) {
            if ( !xl6CgenClassUpcastStmt(ctx, classTypeNode, initExpr, cName) ) {
                xl6CgenError(ctx, initExpr, "module global class upcast lowering failed");
            }
        } else {
            if ( !xl6CgenDeclEmitXValueCallExprWithArgTemps(ctx, classTypeNode, initExpr, cName) ) {
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "%s = ", cName);
                xl6CgenExpr(ctx, initExpr);
                xl6OutputPuts(";\n", ctx->out);
            }
            xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, classTypeNode, cName, initExpr);
        }
        xl6AstFree(initType);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        return 1;
    }
    if ( ctorClass == NULL || !xl6CTextEquals(ctorClass->text, classNode->text) ) {
        xl6CgenError(ctx, initExpr, "module global class constructor type does not match declared type");
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        return 1;
    }

    args = initExpr->childCount > 1 ? initExpr->children[1] : NULL;
    hasCtorArgScope = xl6CgenClassExprTempScopeBegin(ctx, initExpr, &ctorArgScope);
    createNode = xl6CgenResolveClassCreate(ctx, classNode, initExpr);
    if ( createNode != NULL ) {
        xl6CgenIndent(ctx);
        if ( isGenericClass ) {
            xl6CgenGenericClassCallableSymbol(ctx, classNode, classTypeNode, createNode);
        } else {
            xl6CgenClassCallableSymbol(ctx, classNode, createNode);
        }
        xl6OutputPrintf(ctx->out, "(&%s", cName);
        createParams = xl6CgenCallableParams(createNode);
        if ( args != NULL && xl6CgenFixedParamCount(createParams) > 0 ) {
            xl6OutputPuts(", ", ctx->out);
            xl6CgenParamNodeArgs(ctx, createParams, args);
        }
        xl6OutputPuts(");\n", ctx->out);
    } else if ( args != NULL && args->childCount > 0 ) {
        xl6CgenError(ctx, initExpr, "module global class constructor has arguments but no matching create method");
    } else {
        xl6CgenEmitAutoBaseCreateForValue(ctx, classNode, cName, node);
    }
    if ( hasCtorArgScope ) {
        xl6CgenClassExprTempScopeEnd(ctx, &ctorArgScope);
    }

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    return 1;
}

static void xl6CgenEmitModuleGlobalInit(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* typeNode;
    const XL6_AstNode* initExpr;
    const char* name;
    char cName[512];
    size_t cleanupMark;

    if ( ctx == NULL || !xl6CgenIsModuleGlobalDecl(node) ) {
        return;
    }
    name = xl6CgenModuleGlobalName(node);
    typeNode = xl6CgenModuleGlobalType(ctx, node);
    initExpr = xl6CgenModuleGlobalInitExpr(node);
    xl6CgenValueStorageNameText(ctx, cName, sizeof(cName), name);
    cleanupMark = ctx->cleanupCount;
    xl6CgenAddModuleGlobalCleanup(ctx, node, typeNode);
    xl6CgenMarkModuleGlobalCleanupsSince(ctx, cleanupMark);
    if ( xl6CgenIsWeakType(typeNode) ) {
        xl6CgenIndent(ctx);
        xl6CgenFputsValueStorageName(ctx, name);
        xl6OutputPuts(" = ", ctx->out);
        xl6CgenWeakValueExpr(ctx, initExpr);
        xl6OutputPuts(";\n", ctx->out);
        return;
    }
    if ( xl6CgenNullableHasManagedInner(ctx, typeNode) ) {
        xl6CgenIndent(ctx);
        xl6CgenFputsValueStorageName(ctx, name);
        xl6OutputPuts(" = ", ctx->out);
        xl6CgenNullableTaggedValueExpr(ctx, typeNode, initExpr);
        xl6OutputPuts(";\n", ctx->out);
        return;
    }
    if ( xl6CgenEmitModuleGlobalNativeClassInit(ctx, node, typeNode, initExpr, cName) ) {
        return;
    }
    if ( xl6CgenEmitModuleGlobalClassInit(ctx, node, typeNode, initExpr, cName) ) {
        xl6CgenClassValuePublishShared(ctx, typeNode, cName);
        return;
    }
    if ( initExpr == NULL && !xl6CgenModuleGlobalNeedsDefaultInit(ctx, node, typeNode) ) {
        return;
    }
    if ( initExpr != NULL &&
         xl6CgenEmitModuleGlobalClassArgTempInit(ctx, node, typeNode, initExpr, cName) ) {
        return;
    }
    if ( initExpr != NULL && xl6CgenEmitModuleGlobalLoweredInit(ctx, node, typeNode, initExpr) ) {
        return;
    }
    if ( initExpr == NULL && xl6CgenClassHandleTargetType(ctx, typeNode) != NULL ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xvoCreateNull();\n", cName);
        return;
    }
    if ( xl6CgenIsNullableStringTypeNode(ctx, typeNode) ) {
        xl6CgenIndent(ctx);
        xl6CgenFputsValueStorageName(ctx, name);
        xl6OutputPuts(" = xl6_nullable_string_copy(", ctx->out);
        if ( initExpr != NULL ) {
            xl6CgenStringSourceExpr(ctx, typeNode, initExpr);
        } else {
            xl6OutputPuts("0", ctx->out);
        }
        xl6OutputPuts(");\n", ctx->out);
        return;
    }
    if ( xl6CgenIsStringTypeNode(ctx, typeNode) ) {
        xl6CgenIndent(ctx);
        xl6CgenFputsValueStorageName(ctx, name);
        xl6OutputPuts(" = xrtCopyStr(", ctx->out);
        if ( initExpr != NULL ) {
            xl6CgenStringSourceExpr(ctx, typeNode, initExpr);
        } else {
            xl6OutputPuts("\"\"", ctx->out);
        }
        xl6OutputPuts(", 0u);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s == 0) { xl6_runtime_fatal(3); }\n", cName);
        return;
    }

    xl6CgenIndent(ctx);
    xl6CgenFputsValueStorageName(ctx, name);
    xl6OutputPuts(" = ", ctx->out);
    if ( initExpr == NULL ) {
        if ( xl6CgenModuleGlobalUsesTypedContainerStorage(ctx, node, typeNode) ) {
            xl6CgenTypedContainerCreateExprMode(ctx, typeNode, "XRT_OBJMODE_SHARED");
        } else if ( xl6CgenIsSpecialCollectionType(ctx, typeNode) ) {
            xl6CgenSpecialCollectionDefaultCreateExpr(ctx, typeNode);
        } else if ( xl6CgenIsSyncHandleType(typeNode) ) {
            if ( !xl6CgenSyncDefaultCtorExpr(ctx, typeNode, node->line, node->column) ) {
                xl6CgenDefaultValue(ctx, typeNode);
            }
        } else {
            xl6CgenDefaultValue(ctx, typeNode);
        }
    } else if ( xl6CgenIsXValueType(typeNode) ) {
        xl6OutputPuts("xl6_xrt_move_to_shared(", ctx->out);
        xl6CgenXValueFromExprForTarget(ctx, typeNode, initExpr);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6CgenFunctionValueExpr(ctx, initExpr, typeNode);
    } else {
        xl6CgenExpr(ctx, initExpr);
    }
    xl6OutputPuts(";\n", ctx->out);
    if ( initExpr != NULL ) {
        if ( xl6CgenIsFunctionType(typeNode) && xl6CgenExprIsFunctionBorrow(ctx, initExpr) ) {
            xl6CgenRetainFunctionValue(ctx, name);
        }
        if ( xl6CgenIsSpecialCollectionType(ctx, typeNode) &&
             !xl6CgenSpecialCollectionExprIsOwned(ctx, initExpr) ) {
            xl6CgenIndent(ctx);
            xl6CgenFputsValueStorageName(ctx, name);
            xl6OutputPuts(" = ", ctx->out);
            xl6CgenSpecialCollectionRetainCExpr(ctx, typeNode, cName);
            xl6OutputPuts(";\n", ctx->out);
        }
        xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, typeNode, cName, initExpr);
    }
}

/*
 * 模块初始化成功后，全局值必须一直存活到显式卸载。
 * 普通错误作用域会在正常路径末尾释放全部值，因此这里仅生成失败路径清理，
 * 正常路径的清理由 xl_script_module_unit 统一生成。
 */
static void xl6CgenCloseModuleInitErrorScope(
    XL6_CgenContext* ctx,
    size_t cleanupMark,
    const char* errorLabel)
{
    XL6_CgenErrorScope scope;
    size_t i;
    size_t lowerMark;

    if ( ctx == NULL || ctx->errorScopeCount == 0 ) {
        return;
    }

    scope = ctx->errorScopes[ctx->errorScopeCount - 1];
    ctx->errorScopeCount -= 1;
    if ( scope.cleanupMark != cleanupMark || scope.checkpointCount == 0 ) {
        xrtFree(scope.checkpoints);
        return;
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "goto __xl6_cleanup_done_%d;\n", scope.checkpoints[scope.checkpointCount - 1].id);
    for ( i = scope.checkpointCount; i > 0; --i ) {
        XL6_CgenErrorCheckpoint* checkpoint = &scope.checkpoints[i - 1];

        lowerMark = i > 1 ? scope.checkpoints[i - 2].cleanupCount : cleanupMark;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_cleanup_error_%d:\n", checkpoint->id);
        xl6CgenEmitCleanupRange(ctx, lowerMark, checkpoint->cleanupCount);
        xl6CgenIndent(ctx);
        if ( i > 1 ) {
            xl6OutputPrintf(ctx->out, "goto __xl6_cleanup_error_%d;\n", scope.checkpoints[i - 2].id);
        } else {
            xl6OutputPrintf(ctx->out, "goto %s;\n", errorLabel);
        }
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_cleanup_done_%d:\n", scope.checkpoints[scope.checkpointCount - 1].id);
    xrtFree(scope.checkpoints);
}

static void xl6CgenEmitTopLevelXlangMain(
    XL6_CgenContext* ctx,
    XL6_AstNode* root,
    const XL6_AstNode* mainFunc,
    int hasErrorRuntime,
    int hasAsyncRuntime,
    int hasOsRuntime)
{
    const char* oldErrorExitLabel;
    size_t cleanupMark;
    size_t i;

    oldErrorExitLabel = ctx->currentErrorExitLabel;
    cleanupMark = ctx->cleanupCount;
    xl6OutputPuts("static int __xl6_module_initialized = 0;\n\n", ctx->out);
    xl6OutputPuts("int xl_script_module_init(xvalue arg)\n{\n", ctx->out);
    xl6OutputPuts("    if ( __xl6_module_initialized ) {\n", ctx->out);
    xl6OutputPuts("        return 1;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    (void)arg;\n", ctx->out);
    xl6OutputPuts("    XL6_TempScope __xl6_temp_scope_module = xl6_temp_scope_begin();\n", ctx->out);
    if ( hasOsRuntime ) {
        xl6OutputPuts("    xl6_xrt_os_set_args(arg);\n", ctx->out);
    }
    ctx->indent = 1;
    ctx->currentErrorExitLabel = "__xl6_module_init_error";
    xl6CgenPushErrorScope(ctx, cleanupMark);
    xl6CgenEmitProtocolModuleInit(ctx);
    for ( i = 0; i < root->childCount; ++i ) {
        if ( xl6CgenIsModuleGlobalMultiDecl(root->children[i]) ) {
            xl6CgenAddModuleGlobalMultiCleanups(ctx, root->children[i]);
            xl6CgenMultiAssign(ctx, root->children[i]);
            if ( xl6CgenNodeMayRaise(root->children[i]) ) {
                xl6CgenEmitExceptionPropagate(ctx, root->children[i]);
            }
        } else if ( xl6CgenIsModuleGlobalDecl(root->children[i]) ) {
            size_t expressionSlotMark = ctx->coalesceSlotCount;
            const XL6_AstNode* globalInit =
                xl6CgenModuleGlobalInitExpr(root->children[i]);

            xl6CgenPrepareNullCoalesceTemps(ctx, globalInit);
            xl6CgenEmitModuleGlobalInit(ctx, root->children[i]);
            ctx->coalesceSlotCount = expressionSlotMark;
        } else if ( xl6CgenIsTopLevelRuntimeStmt(root->children[i]) ) {
            xl6CgenStmt(ctx, root->children[i]);
        }
    }
    if ( hasErrorRuntime ) {
        xl6OutputPuts("    if ( xl6_error_has() ) {\n", ctx->out);
        ctx->indent += 1;
        if ( !xl6CgenEmitCurrentErrorScopeJump(ctx) ) {
            xl6CgenIndent(ctx);
            xl6OutputPuts("goto __xl6_module_init_error;\n", ctx->out);
        }
        ctx->indent -= 1;
        xl6OutputPuts("    }\n", ctx->out);
    }
    xl6CgenCloseModuleInitErrorScope(ctx, cleanupMark, "__xl6_module_init_error");
    /* module_init 的表达式临时值不能泄漏到 module_unit 的作用域。 */
    xl6CgenEmitModuleCleanups(ctx, cleanupMark, 0);
    xl6OutputPuts("    __xl6_module_initialized = 1;\n", ctx->out);
    xl6OutputPuts("    xl6_temp_scope_end(&__xl6_temp_scope_module);\n", ctx->out);
    xl6OutputPuts("    return 1;\n", ctx->out);
    xl6OutputPuts("__xl6_module_init_error:\n", ctx->out);
    if ( hasAsyncRuntime ) {
        xl6OutputPuts("    xl6_async_wait_idle();\n", ctx->out);
    }
    xl6CgenEmitProtocolModuleUnit(ctx);
    if ( hasOsRuntime ) {
        xl6OutputPuts("    xl6_xrt_os_clear_args();\n", ctx->out);
    }
    xl6OutputPuts("    xl6_temp_scope_end(&__xl6_temp_scope_module);\n", ctx->out);
    xl6OutputPuts("    return 0;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);

    xl6OutputPuts("void xl_script_module_unit(void)\n{\n", ctx->out);
    xl6OutputPuts("    if ( !__xl6_module_initialized ) {\n", ctx->out);
    xl6OutputPuts("        return;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    __xl6_module_initialized = 0;\n", ctx->out);
    ctx->indent = 1;
    if ( hasAsyncRuntime ) {
        xl6OutputPuts("    xl6_async_wait_idle();\n", ctx->out);
    }
    xl6CgenEmitProtocolModuleUnit(ctx);
    xl6CgenEmitModuleCleanups(ctx, cleanupMark, 1);
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    if ( hasOsRuntime ) {
        xl6OutputPuts("    xl6_xrt_os_clear_args();\n", ctx->out);
    }
    xl6OutputPuts("}\n\n", ctx->out);

    xl6OutputPuts("int xlang_main(xvalue arg)\n{\n", ctx->out);
    if ( mainFunc != NULL ) {
        xl6OutputPuts("    int __xl6_main_code = 0;\n", ctx->out);
    }
    xl6OutputPuts("    if ( !xl_script_module_init(arg) ) {\n", ctx->out);
    xl6OutputPuts("        return 0;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    if ( mainFunc != NULL ) {
        xl6OutputPuts("    __xl6_main_code = ", ctx->out);
        xl6CgenUserMainSymbol(ctx->out);
        xl6OutputPuts("(arg);\n", ctx->out);
        if ( hasErrorRuntime ) {
            xl6OutputPuts("    if ( xl6_error_has() ) {\n", ctx->out);
            xl6OutputPuts("        goto __xl6_script_error;\n", ctx->out);
            xl6OutputPuts("    }\n", ctx->out);
        }
        xl6OutputPuts("    xl_script_module_unit();\n", ctx->out);
        xl6OutputPuts("    return __xl6_main_code;\n", ctx->out);
    } else {
        xl6OutputPuts("    xl_script_module_unit();\n", ctx->out);
        xl6OutputPuts("    return 0;\n", ctx->out);
    }
    if ( hasErrorRuntime ) {
        xl6OutputPuts("__xl6_script_error:\n", ctx->out);
        xl6OutputPuts("    xl_script_module_unit();\n", ctx->out);
        xl6OutputPuts("    return 0;\n", ctx->out);
    }
    xl6OutputPuts("}\n\n", ctx->out);
    ctx->indent = 0;
    ctx->currentErrorExitLabel = oldErrorExitLabel;
}
