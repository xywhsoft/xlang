static void xl6CgenEmitArgvArrayHelper(XL6_Output* out)
{
    xl6OutputPuts("#ifndef XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xvalue_struct* xvalue;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("xvalue xvoCreateArray(void);\n", out);
    xl6OutputPuts("xvalue xvoCreateText(void* sVal, unsigned int iSize, bool bColloc);\n", out);
    xl6OutputPuts("bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);\n", out);
    xl6OutputPuts("void xvoUnref(xvalue pVal);\n\n", out);
    xl6OutputPuts("static xvalue xl6_argv_to_arg(int argc, char** argv)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue arg = xvoCreateArray();\n", out);
    xl6OutputPuts("    int i;\n\n", out);
    xl6OutputPuts("    if ( arg == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    for ( i = 0; i < argc; ++i ) {\n", out);
    xl6OutputPuts("        const char* text = (argv != 0 && argv[i] != 0) ? argv[i] : \"\";\n", out);
    xl6OutputPuts("        xvalue item = xvoCreateText((void*)text, 0, false);\n", out);
    xl6OutputPuts("        if ( item == 0 || !xvoArrayAppendValue(arg, item, true) ) {\n", out);
    xl6OutputPuts("            if ( item != 0 ) {\n", out);
    xl6OutputPuts("                xvoUnref(item);\n", out);
    xl6OutputPuts("            }\n", out);
    xl6OutputPuts("            xvoUnref(arg);\n", out);
    xl6OutputPuts("            return 0;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return arg;\n", out);
    xl6OutputPuts("}\n\n", out);
}

static void xl6CgenEmitMainLikeWrapper(XL6_CgenContext* ctx, const char* symbolName)
{
    xl6OutputPuts("int ", ctx->out);
    xl6OutputPuts(symbolName != NULL ? symbolName : "main", ctx->out);
    xl6OutputPuts("(int argc, char** argv)\n{\n", ctx->out);
    xl6OutputPuts("    XL_InitOptions __xl6_options = xlInitOptions(argc, argv);\n", ctx->out);
    xl6OutputPuts("    xvalue __xl6_arg;\n", ctx->out);
    xl6OutputPuts("    int __xl6_code;\n", ctx->out);
    xl6OutputPuts("\n", ctx->out);
    xl6OutputPuts("    if ( !xlInit(&__xl6_options) ) {\n", ctx->out);
    xl6OutputPuts("        return 1;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    __xl6_arg = xl6_argv_to_arg(argc, argv);\n", ctx->out);
    xl6OutputPuts("    if ( __xl6_arg == 0 ) {\n", ctx->out);
    xl6OutputPuts("        xlUnit();\n", ctx->out);
    xl6OutputPuts("        return 1;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    __xl6_code = xlang_main(__xl6_arg);\n", ctx->out);
    if ( ctx->needsErrorRuntime ) {
        xl6OutputPuts("    if ( xl6_error_has() ) {\n", ctx->out);
        xl6OutputPuts("        xl6_error_print_uncaught();\n", ctx->out);
        xl6OutputPuts("        xl6_error_clear();\n", ctx->out);
        xl6OutputPuts("        xvoUnref(__xl6_arg);\n", ctx->out);
        xl6OutputPuts("        xlUnit();\n", ctx->out);
        xl6OutputPuts("        return 1;\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
    }
    xl6OutputPuts("    xvoUnref(__xl6_arg);\n", ctx->out);
    xl6OutputPuts("    xlUnit();\n", ctx->out);
    xl6OutputPuts("    return __xl6_code;\n", ctx->out);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenEmitMainWrapper(XL6_CgenContext* ctx)
{
    xl6CgenEmitMainLikeWrapper(ctx, "main");
}

static void xl6CgenEmitScriptErrorBridge(XL6_CgenContext* ctx)
{
    if ( !ctx->needsErrorRuntime ) {
        return;
    }

    xl6OutputPuts("int xl_script_has_error(void)\n{\n", ctx->out);
    xl6OutputPuts("    return xl6_error_has();\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
    xl6OutputPuts("void xl_script_print_uncaught(void)\n{\n", ctx->out);
    xl6OutputPuts("    xl6_error_print_uncaught();\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
    xl6OutputPuts("void xl_script_clear_error(void)\n{\n", ctx->out);
    xl6OutputPuts("    xl6_error_clear();\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}

static int xl6CgenIsModuleGlobalDecl(const XL6_AstNode* node)
{
    return node != NULL && (node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL);
}

static const XL6_AstNode* xl6CgenModuleGlobalType(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* typeNode;

    if ( node == NULL ) {
        return NULL;
    }
    typeNode = xl6CgenFindVarType(ctx, node->text);
    if ( typeNode != NULL && !xl6CTextEquals(typeNode->text, "auto") ) {
        return typeNode;
    }
    return node->childCount > 0 ? node->children[0] : NULL;
}

static int xl6CgenHasModuleGlobalDecl(const XL6_AstNode* root)
{
    size_t i;

    if ( root == NULL ) {
        return 0;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        if ( xl6CgenIsModuleGlobalDecl(root->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenModuleGlobalUsesTypedContainerStorage(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* typeNode)
{
    return ctx != NULL &&
           node != NULL &&
           node->text != NULL &&
           xl6CgenIsTypedContainerType(ctx, typeNode) &&
           (xl6CgenFindTrackedVarFlags(ctx, node->text) & XL6_CGEN_VAR_TYPED_CONTAINER) != 0;
}

static void xl6CgenRegisterModuleGlobalTypes(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    size_t i;

    if ( ctx == NULL || root == NULL ) {
        return;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        const XL6_AstNode* typeNode;
        const XL6_AstNode* initExpr;

        if ( !xl6CgenIsModuleGlobalDecl(node) ) {
            continue;
        }
        typeNode = node->childCount > 0 ? node->children[0] : NULL;
        initExpr = node->childCount > 1 ? node->children[1] : NULL;
        if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") ) {
            XL6_AstNode* inferredType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
            if ( inferredType != NULL ) {
                xl6CgenOwnTypeNode(ctx, inferredType);
                xl6CgenNeedsAddDeclVarType(ctx, node->text, inferredType, initExpr, 1);
            }
        } else if ( typeNode != NULL ) {
            xl6CgenNeedsAddDeclVarType(ctx, node->text, typeNode, initExpr, 0);
        }
    }
}

static void xl6CgenEmitModuleGlobalDecls(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    size_t i;

    if ( ctx == NULL || root == NULL ) {
        return;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        const XL6_AstNode* typeNode;

        if ( !xl6CgenIsModuleGlobalDecl(node) ) {
            continue;
        }
        typeNode = xl6CgenModuleGlobalType(ctx, node);
        if ( xl6CgenModuleGlobalUsesTypedContainerStorage(ctx, node, typeNode) ) {
            xl6OutputPuts(xl6CgenTypedContainerCType(ctx, typeNode), ctx->out);
        } else {
            xl6OutputPuts(xl6CgenTypeName(ctx, typeNode), ctx->out);
        }
        xl6OutputPutc(' ', ctx->out);
        xl6CgenFputsCIdent(ctx->out, node->text);
        xl6OutputPuts(";\n", ctx->out);
    }
    xl6OutputPutc('\n', ctx->out);
}

static int xl6CgenModuleGlobalNeedsDefaultInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* typeNode)
{
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return 0;
    }
    return xl6CTextEquals(typeNode->text, "string") ||
           xl6CgenIsXValueType(typeNode) ||
           xl6CgenModuleGlobalUsesTypedContainerStorage(ctx, node, typeNode) ||
           xl6CgenIsFutureLikeType(typeNode) ||
           xl6CgenIsChannelType(typeNode) ||
           xl6CgenIsThreadType(typeNode) ||
           xl6CgenIsGeneratorType(typeNode) ||
           xl6CgenIsSyncHandleType(typeNode);
}

static void xl6CgenAddModuleGlobalCleanup(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    if ( ctx == NULL || node == NULL || node->text == NULL || typeNode == NULL ) {
        return;
    }

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( xl6CgenIsNativeBackedClassType(ctx, concreteType) ) {
        const XL6_AstNode* customNode = xl6CgenFindNativeBackedClass(ctx, concreteType->text);

        xl6CgenAddNativeClassCleanup(ctx, node->text, customNode);
        return;
    }
    if ( xl6CgenIsClassType(ctx, concreteType) ) {
        XL6_AstNode* classNode = xl6CgenClassNodeForType(ctx, concreteType);

        xl6CgenAddClassCleanup(ctx, node->text, classNode, concreteType);
        return;
    }
    if ( xl6CgenModuleGlobalUsesTypedContainerStorage(ctx, node, concreteType) ) {
        xl6CgenAddTypedContainerCleanup(ctx, node->text, concreteType);
        return;
    }
    if ( xl6CgenIsFunctionType(concreteType) ) {
        xl6CgenAddFunctionCleanup(ctx, node->text, concreteType);
        return;
    }

    xl6CgenAddFutureCleanup(ctx, node->text, concreteType);
    xl6CgenAddChannelCleanup(ctx, node->text, concreteType);
    xl6CgenAddThreadCleanup(ctx, node->text, concreteType);
    xl6CgenAddGeneratorCleanup(ctx, node->text, concreteType);
    xl6CgenAddSyncCleanup(ctx, node->text, concreteType);
    xl6CgenAddXValueCleanup(ctx, node->text, concreteType);
}

static int xl6CgenEmitModuleGlobalLoweredInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* initExpr)
{
    char tempName[64];
    const char* cType;
    int emitted;

    if ( ctx == NULL || node == NULL || typeNode == NULL || initExpr == NULL ) {
        return 0;
    }
    snprintf(tempName, sizeof(tempName), "__xl6_global_init_%d", ctx->tempId++);
    emitted = 0;
    if ( xl6CgenModuleGlobalUsesTypedContainerStorage(ctx, node, typeNode) ) {
        int initByCopy = xl6CgenTypedContainerCanCopyFromExpr(ctx, typeNode, initExpr);
        int initByMove = xl6CgenTypedContainerCanMoveFromExpr(ctx, typeNode, initExpr);
        XL6_AstNode* memberFunctionType = NULL;
        char memberTempName[64];

        memberTempName[0] = '\0';
        if ( initByMove ) {
            memberFunctionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, initExpr);
            if ( memberFunctionType != NULL ) {
                snprintf(memberTempName, sizeof(memberTempName), "__xl6_member_fn_stmt_%d", ctx->tempId++);
                xl6CgenEmitMemberFunctionStatementTemp(ctx, initExpr, memberTempName);
            }
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, typeNode), tempName);
        if ( initByCopy ) {
            xl6CgenTypedContainerCopyExpr(ctx, typeNode, initExpr);
        } else if ( initByMove ) {
            if ( memberFunctionType != NULL ) {
                xl6CgenMemberFunctionStatementInvokeExpr(ctx, initExpr, memberFunctionType, memberTempName);
            } else {
                xl6CgenExpr(ctx, initExpr);
            }
        } else {
            xl6CgenTypedContainerCreateExpr(ctx, typeNode);
        }
        xl6OutputPuts(";\n", ctx->out);
        if ( !initByCopy && !initByMove ) {
            XL6_CgenTaskSpawnArgTempScope taskScope;
            int hasTaskScope;

            hasTaskScope = initExpr != NULL &&
                           xl6CgenTaskSpawnArgTempScopeBegin(ctx, initExpr, &taskScope);
            xl6CgenTypedContainerFillFromLiteral(ctx, tempName, typeNode, initExpr);
            if ( hasTaskScope ) {
                xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
            }
        }
        xl6AstFree(memberFunctionType);
        emitted = 1;
    } else if ( xl6CgenIsXValueType(typeNode) ) {
        emitted = xl6CgenDeclTryXValueArrayLiteralInit(ctx, typeNode, tempName, initExpr) ||
                  xl6CgenDeclTryXValueNullCoalesceInit(ctx, typeNode, tempName, initExpr) ||
                  xl6CgenDeclTryXValueBinaryInit(ctx, typeNode, tempName, initExpr) ||
                  xl6CgenDeclTryXValueConditionalInit(ctx, typeNode, tempName, initExpr);
    } else if ( !xl6CgenIsFunctionType(typeNode) ) {
        cType = xl6CgenTypeName(ctx, typeNode);
        emitted = xl6CgenDeclTryStaticNullCoalesceInit(ctx, typeNode, cType, tempName, initExpr) ||
                  xl6CgenDeclTryStaticXValueCompareInit(ctx, typeNode, cType, tempName, initExpr) ||
                  xl6CgenDeclTryStaticXValueApproxInit(ctx, typeNode, cType, tempName, initExpr);
        if ( !emitted && xl6CgenDeclCanEmitCallArgTemp(ctx, typeNode, initExpr) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s %s = ", cType, tempName);
            xl6CgenDefaultValue(ctx, typeNode);
            xl6OutputPuts(";\n", ctx->out);
            emitted = xl6CgenDeclEmitCallArgTemp(ctx, typeNode, cType, tempName, initExpr);
        }
    }
    if ( !emitted ) {
        return 0;
    }
    xl6CgenIndent(ctx);
    xl6CgenFputsCIdent(ctx->out, node->text);
    xl6OutputPuts(" = ", ctx->out);
    xl6OutputPuts(tempName, ctx->out);
    xl6OutputPuts(";\n", ctx->out);
    return 1;
}

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
         xl6CgenIsFunctionType(typeNode) ) {
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
    xl6CgenIndent(ctx);
    xl6OutputPuts(cName, ctx->out);
    xl6OutputPuts(" = ", ctx->out);
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6CgenXValueFromExprForTarget(ctx, typeNode, initExpr);
    } else {
        xl6CgenExpr(ctx, initExpr);
    }
    xl6OutputPuts(";\n", ctx->out);
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
    const XL6_AstNode* customNode;
    XL6_AstNode* ctorCustom;

    if ( ctx == NULL || node == NULL || typeNode == NULL || cName == NULL ||
         !xl6CgenIsNativeBackedClassType(ctx, typeNode) ) {
        return 0;
    }

    customNode = xl6CgenFindNativeBackedClass(ctx, typeNode->text);
    if ( customNode == NULL ) {
        return 0;
    }
    if ( initExpr == NULL ) {
        return 1;
    }

    ctorCustom = NULL;
    if ( xl6CgenCallIsNativeBackedCtor(ctx, initExpr, &ctorCustom) ) {
        if ( ctorCustom == NULL || !xl6CTextEquals(ctorCustom->text, customNode->text) ) {
            xl6CgenError(ctx, initExpr, "module global native-backed class constructor type does not match declared type");
            return 1;
        }
        xl6CgenNativeBackedCtorAssign(ctx, customNode, node->text, initExpr);
        return 1;
    }

    {
        XL6_AstNode* initType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
        const XL6_AstNode* resolvedInitType = xl6CgenResolveAliasType(ctx, initType);

        if ( resolvedInitType != NULL &&
             xl6CTextEquals(resolvedInitType->text, customNode->text) &&
             (!xl6CgenNativeBackedClassHasDestroy(customNode) ||
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
    XL6_AstNode* canonicalType;
    const XL6_AstNode* classTypeNode;
    const XL6_AstNode* args;
    const XL6_AstNode* createNode;
    const XL6_AstNode* createParams;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    int isGenericClass;

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
        xl6CgenEmitAutoBaseCreateForValue(ctx, classNode, cName, node);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        return 1;
    }

    ctorClass = NULL;
    if ( !xl6CgenCallIsClassCtor(ctx, initExpr, &ctorClass) ) {
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        return 0;
    }
    if ( ctorClass == NULL || !xl6CTextEquals(ctorClass->text, classNode->text) ) {
        xl6CgenError(ctx, initExpr, "module global class constructor type does not match declared type");
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        return 1;
    }

    args = initExpr->childCount > 1 ? initExpr->children[1] : NULL;
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

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    return 1;
}

static void xl6CgenEmitModuleGlobalInit(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* typeNode;
    const XL6_AstNode* initExpr;
    char cName[512];

    if ( ctx == NULL || !xl6CgenIsModuleGlobalDecl(node) ) {
        return;
    }
    typeNode = xl6CgenModuleGlobalType(ctx, node);
    initExpr = node->childCount > 1 ? node->children[1] : NULL;
    xl6CgenCIdentText(cName, sizeof(cName), node->text);
    xl6CgenAddModuleGlobalCleanup(ctx, node, typeNode);
    if ( xl6CgenEmitModuleGlobalNativeClassInit(ctx, node, typeNode, initExpr, cName) ) {
        return;
    }
    if ( xl6CgenEmitModuleGlobalClassInit(ctx, node, typeNode, initExpr, cName) ) {
        return;
    }
    if ( initExpr == NULL && !xl6CgenModuleGlobalNeedsDefaultInit(ctx, node, typeNode) ) {
        return;
    }
    if ( initExpr != NULL && xl6CgenEmitModuleGlobalLoweredInit(ctx, node, typeNode, initExpr) ) {
        return;
    }
    if ( initExpr != NULL &&
         xl6CgenEmitModuleGlobalClassArgTempInit(ctx, node, typeNode, initExpr, cName) ) {
        return;
    }

    xl6CgenIndent(ctx);
    xl6CgenFputsCIdent(ctx->out, node->text);
    xl6OutputPuts(" = ", ctx->out);
    if ( initExpr == NULL ) {
        if ( xl6CgenModuleGlobalUsesTypedContainerStorage(ctx, node, typeNode) ) {
            xl6CgenTypedContainerCreateExpr(ctx, typeNode);
        } else {
            xl6CgenDefaultValue(ctx, typeNode);
        }
    } else if ( xl6CgenIsXValueType(typeNode) ) {
        xl6CgenXValueFromExprForTarget(ctx, typeNode, initExpr);
    } else if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6CgenFunctionValueExpr(ctx, initExpr, typeNode);
    } else {
        xl6CgenExpr(ctx, initExpr);
    }
    xl6OutputPuts(";\n", ctx->out);
    if ( initExpr != NULL ) {
        if ( xl6CgenIsFunctionType(typeNode) && xl6CgenExprIsFunctionBorrow(ctx, initExpr) ) {
            xl6CgenRetainFunctionValue(ctx, node->text);
        }
        xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, typeNode, cName, initExpr);
    }
}

static void xl6CgenEmitTopLevelXlangMain(XL6_CgenContext* ctx, XL6_AstNode* root, const XL6_AstNode* mainFunc, int hasErrorRuntime)
{
    const char* oldErrorExitLabel;
    size_t i;

    oldErrorExitLabel = ctx->currentErrorExitLabel;
    xl6OutputPuts("int xlang_main(xvalue arg)\n{\n", ctx->out);
    xl6OutputPuts("    (void)arg;\n", ctx->out);
    if ( mainFunc != NULL ) {
        xl6OutputPuts("    int __xl6_main_code = 0;\n", ctx->out);
    }
    ctx->indent = 1;
    ctx->currentErrorExitLabel = "__xl6_script_error";
    for ( i = 0; i < root->childCount; ++i ) {
        if ( xl6CgenIsModuleGlobalDecl(root->children[i]) ) {
            xl6CgenEmitModuleGlobalInit(ctx, root->children[i]);
        } else if ( xl6CgenIsTopLevelRuntimeStmt(root->children[i]) ) {
            xl6CgenStmt(ctx, root->children[i]);
        }
    }
    if ( hasErrorRuntime ) {
        xl6OutputPuts("    if ( xl6_error_has() ) {\n", ctx->out);
        xl6OutputPuts("        goto __xl6_script_error;\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
    }
    if ( mainFunc != NULL ) {
        xl6OutputPuts("    __xl6_main_code = ", ctx->out);
        xl6CgenUserMainSymbol(ctx->out);
        xl6OutputPuts("(arg);\n", ctx->out);
        xl6CgenCloseScope(ctx, 0);
        xl6OutputPuts("    return __xl6_main_code;\n", ctx->out);
    } else {
        xl6CgenCloseScope(ctx, 0);
        xl6OutputPuts("    return 0;\n", ctx->out);
    }
    if ( hasErrorRuntime ) {
        xl6OutputPuts("__xl6_script_error:\n", ctx->out);
        xl6OutputPuts("    return 0;\n", ctx->out);
    }
    xl6OutputPuts("}\n\n", ctx->out);
    ctx->indent = 0;
    ctx->currentErrorExitLabel = oldErrorExitLabel;
}
