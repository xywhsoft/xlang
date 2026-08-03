static void xl6CgenEmitArgvArrayHelper(XL6_Output* out)
{
    xl6OutputPuts("#ifndef XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xvalue_struct* xvalue;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("xvalue xvoCreateArray(void);\n", out);
    xl6OutputPuts("xvalue xvoCreateText(void* sVal, unsigned int iSize, bool bColloc);\n", out);
    xl6OutputPuts("bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);\n", out);
    xl6OutputPuts("void xvoUnref(xvalue pVal);\n\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t size);\n", out);
    xl6OutputPuts("void xrtFree(void* p);\n\n", out);
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
    xl6OutputPuts("#if defined(_WIN32) || defined(_WIN64)\n", out);
    xl6OutputPuts("#include <wchar.h>\n", out);
    xl6OutputPuts("#ifndef XL6_WINAPI_ARGV_DECL\n", out);
    xl6OutputPuts("#define XL6_WINAPI_ARGV_DECL\n", out);
    xl6OutputPuts("__declspec(dllimport) wchar_t* __stdcall GetCommandLineW(void);\n", out);
    xl6OutputPuts("__declspec(dllimport) wchar_t** __stdcall CommandLineToArgvW(const wchar_t* lpCmdLine, int* pNumArgs);\n", out);
    xl6OutputPuts("__declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int CodePage, unsigned long dwFlags, const wchar_t* lpWideCharStr, int cchWideChar, char* lpMultiByteStr, int cbMultiByte, const char* lpDefaultChar, int* lpUsedDefaultChar);\n", out);
    xl6OutputPuts("__declspec(dllimport) void* __stdcall LocalFree(void* hMem);\n", out);
    xl6OutputPuts("#define XL6_CP_UTF8 65001u\n", out);
    xl6OutputPuts("#endif\n\n", out);
    xl6OutputPuts("static char* xl6_wide_to_utf8(const wchar_t* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int size;\n", out);
    xl6OutputPuts("    char* utf8;\n\n", out);
    xl6OutputPuts("    if ( text == 0 ) {\n", out);
    xl6OutputPuts("        text = L\"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    size = WideCharToMultiByte(XL6_CP_UTF8, 0, text, -1, 0, 0, 0, 0);\n", out);
    xl6OutputPuts("    if ( size <= 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    utf8 = (char*)xrtMalloc((size_t)size);\n", out);
    xl6OutputPuts("    if ( utf8 == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( WideCharToMultiByte(XL6_CP_UTF8, 0, text, -1, utf8, size, 0, 0) <= 0 ) {\n", out);
    xl6OutputPuts("        xrtFree(utf8);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return utf8;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static xvalue xl6_windows_argv_to_arg(int fallbackArgc, char** fallbackArgv)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int argc = 0;\n", out);
    xl6OutputPuts("    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);\n", out);
    xl6OutputPuts("    xvalue arg;\n", out);
    xl6OutputPuts("    int i;\n\n", out);
    xl6OutputPuts("    if ( argv == 0 ) {\n", out);
    xl6OutputPuts("        return xl6_argv_to_arg(fallbackArgc, fallbackArgv);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    arg = xvoCreateArray();\n", out);
    xl6OutputPuts("    if ( arg == 0 ) {\n", out);
    xl6OutputPuts("        LocalFree(argv);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    for ( i = 0; i < argc; ++i ) {\n", out);
    xl6OutputPuts("        char* text = xl6_wide_to_utf8(argv[i]);\n", out);
    xl6OutputPuts("        xvalue item = text != 0 ? xvoCreateText((void*)text, 0, false) : 0;\n", out);
    xl6OutputPuts("        if ( text != 0 ) {\n", out);
    xl6OutputPuts("            xrtFree(text);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("        if ( item == 0 || !xvoArrayAppendValue(arg, item, true) ) {\n", out);
    xl6OutputPuts("            if ( item != 0 ) {\n", out);
    xl6OutputPuts("                xvoUnref(item);\n", out);
    xl6OutputPuts("            }\n", out);
    xl6OutputPuts("            xvoUnref(arg);\n", out);
    xl6OutputPuts("            LocalFree(argv);\n", out);
    xl6OutputPuts("            return 0;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    LocalFree(argv);\n", out);
    xl6OutputPuts("    return arg;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("#endif\n\n", out);
    xl6OutputPuts("static xvalue xl6_command_argv_to_arg(int argc, char** argv)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("#if defined(_WIN32) || defined(_WIN64)\n", out);
    xl6OutputPuts("    return xl6_windows_argv_to_arg(argc, argv);\n", out);
    xl6OutputPuts("#else\n", out);
    xl6OutputPuts("    return xl6_argv_to_arg(argc, argv);\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("}\n\n", out);
}

static int xl6CgenIsModuleGlobalDecl(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_VAR_DECL ||
            node->kind == XL6_AST_CONST_DECL ||
            ((node->kind == XL6_AST_EXPORT_VAR || node->kind == XL6_AST_EXPORT_CONST) &&
              !xl6CgenExternalVarHasCSymbol(node)));
}

static int xl6CgenIsModuleGlobalMultiDecl(const XL6_AstNode* node)
{
    return node != NULL && node->kind == XL6_AST_MULTI_VAR_DECL;
}

static const char* xl6CgenModuleGlobalName(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    if ( node->kind == XL6_AST_EXPORT_VAR || node->kind == XL6_AST_EXPORT_CONST ) {
        return xl6CgenExternalVarLocalName(node);
    }
    return node->text;
}

static const XL6_AstNode* xl6CgenModuleGlobalDeclaredType(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    if ( node->kind == XL6_AST_EXPORT_VAR || node->kind == XL6_AST_EXPORT_CONST ) {
        return node->childCount > 1 ? node->children[1] : NULL;
    }
    return node->childCount > 0 ? node->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenModuleGlobalInitExpr(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    if ( node->kind == XL6_AST_EXPORT_VAR || node->kind == XL6_AST_EXPORT_CONST ) {
        return node->childCount > 2 ? node->children[2] : NULL;
    }
    return node->childCount > 1 ? node->children[1] : NULL;
}

static XL6_AstNode* xl6CgenModuleGlobalMultiReturnTypes(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* expr;
    const XL6_AstNode* func;
    const XL6_AstNode* functionType;
    const XL6_AstNode* classMethod;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* returns;
    XL6_AstNode* ownedFunctionType;
    XL6_AstNode* ownedFuncTypeArgs;
    XL6_AstNode* ownedReturns;

    if ( ctx == NULL || !xl6CgenIsModuleGlobalMultiDecl(node) ) {
        return NULL;
    }

    expr = node->childCount > 1 ? node->children[1] : NULL;
    ownedFunctionType = NULL;
    ownedFuncTypeArgs = NULL;
    functionType = xl6CgenResolveMultiReturnFunctionType(ctx, expr, &ownedFunctionType);
    func = functionType == NULL ? xl6CgenResolveMultiReturnCall(ctx, expr, &ownedFuncTypeArgs) : NULL;
    ownerClass = NULL;
    receiverType = NULL;
    classMethod = (functionType == NULL && func == NULL) ?
        xl6CgenResolveMultiReturnClassMethod(ctx, expr, &ownerClass, &receiverType) :
        NULL;
    returns = functionType != NULL ? xl6CgenFunctionReturnTypes(functionType) :
              (func != NULL ? func->children[2] :
              (classMethod != NULL && classMethod->childCount > 2 ? classMethod->children[2] : NULL));
    ownedReturns = xl6CgenConcreteMultiReturnTypes(
        ctx, returns, func, ownedFuncTypeArgs, classMethod, ownerClass, receiverType);
    xl6AstFree(ownedFunctionType);
    xl6AstFree(ownedFuncTypeArgs);
    return ownedReturns;
}

static const XL6_AstNode* xl6CgenModuleGlobalType(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* typeNode;
    const char* name;

    if ( node == NULL ) {
        return NULL;
    }
    name = xl6CgenModuleGlobalName(node);
    typeNode = xl6CgenFindVarType(ctx, name);
    if ( typeNode != NULL && !xl6CTextEquals(typeNode->text, "auto") ) {
        return typeNode;
    }
    return xl6CgenModuleGlobalDeclaredType(node);
}

static int xl6CgenHasModuleGlobalDecl(const XL6_AstNode* root)
{
    size_t i;

    if ( root == NULL ) {
        return 0;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        if ( xl6CgenIsModuleGlobalDecl(root->children[i]) ||
             xl6CgenIsModuleGlobalMultiDecl(root->children[i]) ) {
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
    const char* name;

    name = xl6CgenModuleGlobalName(node);
    return ctx != NULL &&
           node != NULL &&
           name != NULL &&
           xl6CgenIsTypedContainerType(ctx, typeNode) &&
           (xl6CgenFindTrackedVarFlags(ctx, name) & XL6_CGEN_VAR_TYPED_CONTAINER) != 0;
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
        const char* name;
        unsigned int flags;

        if ( xl6CgenIsModuleGlobalMultiDecl(node) ) {
            const XL6_AstNode* names = node->childCount > 0 ? node->children[0] : NULL;
            XL6_AstNode* returns = xl6CgenModuleGlobalMultiReturnTypes(ctx, node);
            size_t j;

            if ( names == NULL || returns == NULL || names->childCount != returns->childCount ) {
                xl6AstFree(returns);
                continue;
            }
            for ( j = 0; j < names->childCount; ++j ) {
                const XL6_AstNode* item = names->children[j];
                typeNode = returns->children[j];
                if ( item == NULL || item->text == NULL || xl6CTextEquals(item->text, "_") || typeNode == NULL ) {
                    continue;
                }
                flags = XL6_CGEN_VAR_MODULE_GLOBAL;
                if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
                    flags |= XL6_CGEN_VAR_TYPED_CONTAINER;
                }
                xl6CgenAddVarTypeWithFlags(ctx, item->text, typeNode, flags);
            }
            xl6CgenOwnTypeNode(ctx, returns);
            continue;
        }

        if ( !xl6CgenIsModuleGlobalDecl(node) ) {
            continue;
        }
        name = xl6CgenModuleGlobalName(node);
        typeNode = xl6CgenModuleGlobalDeclaredType(node);
        initExpr = xl6CgenModuleGlobalInitExpr(node);
        if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") ) {
            XL6_AstNode* inferredType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
            if ( inferredType != NULL ) {
                xl6CgenOwnTypeNode(ctx, inferredType);
                flags = XL6_CGEN_VAR_MODULE_GLOBAL;
                if ( xl6CgenDeclShouldUseTypedContainer(ctx, inferredType, initExpr, 1) ) {
                    flags |= XL6_CGEN_VAR_TYPED_CONTAINER;
                }
                xl6CgenAddVarTypeWithFlags(ctx, name, inferredType, flags);
            }
        } else if ( typeNode != NULL ) {
            flags = XL6_CGEN_VAR_MODULE_GLOBAL;
            if ( xl6CgenDeclShouldUseTypedContainer(ctx, typeNode, initExpr, 0) ) {
                flags |= XL6_CGEN_VAR_TYPED_CONTAINER;
            }
            xl6CgenAddVarTypeWithFlags(ctx, name, typeNode, flags);
        }
    }
}

static void xl6CgenEmitModuleGlobalDeclsEx(
    XL6_CgenContext* ctx,
    const XL6_AstNode* root,
    int forwardOnly)
{
    size_t i;

    if ( ctx == NULL || root == NULL ) {
        return;
    }
    (void)forwardOnly;
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        const XL6_AstNode* typeNode;
        const char* name;

        if ( xl6CgenIsModuleGlobalMultiDecl(node) ) {
            const XL6_AstNode* names = node->childCount > 0 ? node->children[0] : NULL;
            XL6_AstNode* returns = xl6CgenModuleGlobalMultiReturnTypes(ctx, node);
            size_t j;

            if ( names == NULL || returns == NULL || names->childCount != returns->childCount ) {
                xl6AstFree(returns);
                continue;
            }
            for ( j = 0; j < names->childCount; ++j ) {
                const XL6_AstNode* item = names->children[j];
                unsigned int flags;

                if ( item == NULL || item->text == NULL || xl6CTextEquals(item->text, "_") ) {
                    continue;
                }
                name = item->text;
                typeNode = returns->children[j];
                flags = xl6CgenFindTrackedVarFlags(ctx, name);
                xl6OutputPuts("static ", ctx->out);
                if ( (flags & XL6_CGEN_VAR_TYPED_CONTAINER) != 0 ) {
                    xl6OutputPuts(xl6CgenTypedContainerCType(ctx, typeNode), ctx->out);
                } else {
                    xl6OutputPuts(xl6CgenTypeName(ctx, typeNode), ctx->out);
                }
                xl6OutputPutc(' ', ctx->out);
                xl6CgenFputsValueStorageName(ctx, name);
                xl6OutputPuts(";\n", ctx->out);
            }
            xl6AstFree(returns);
            continue;
        }

        if ( !xl6CgenIsModuleGlobalDecl(node) ) {
            continue;
        }
        name = xl6CgenModuleGlobalName(node);
        typeNode = xl6CgenModuleGlobalType(ctx, node);
        xl6OutputPuts("static ", ctx->out);
        if ( xl6CgenModuleGlobalUsesTypedContainerStorage(ctx, node, typeNode) ) {
            xl6OutputPuts(xl6CgenTypedContainerCType(ctx, typeNode), ctx->out);
        } else {
            xl6OutputPuts(xl6CgenTypeName(ctx, typeNode), ctx->out);
        }
        xl6OutputPutc(' ', ctx->out);
        xl6CgenFputsValueStorageName(ctx, name);
        xl6OutputPuts(";\n", ctx->out);
    }
    xl6OutputPutc('\n', ctx->out);
}

static void xl6CgenAddModuleGlobalMultiCleanups(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* names;
    XL6_AstNode* returns;
    size_t i;
    size_t cleanupMark;

    if ( ctx == NULL || !xl6CgenIsModuleGlobalMultiDecl(node) ) {
        return;
    }
    names = node->childCount > 0 ? node->children[0] : NULL;
    returns = xl6CgenModuleGlobalMultiReturnTypes(ctx, node);
    if ( names == NULL || returns == NULL || names->childCount != returns->childCount ) {
        xl6AstFree(returns);
        return;
    }
    cleanupMark = ctx->cleanupCount;
    for ( i = 0; i < names->childCount; ++i ) {
        const XL6_AstNode* item = names->children[i];
        const XL6_AstNode* typeNode;

        if ( item == NULL || item->text == NULL || xl6CTextEquals(item->text, "_") ) {
            continue;
        }
        typeNode = xl6CgenFindVarType(ctx, item->text);
        if ( typeNode == NULL ) {
            typeNode = returns->children[i];
        }
        if ( typeNode == NULL ) {
            continue;
        }
        xl6CgenAddManagedCleanupForType(ctx, item->text, typeNode);
    }
    xl6CgenMarkModuleGlobalCleanupsSince(ctx, cleanupMark);
    xl6AstFree(returns);
}

static void xl6CgenEmitModuleGlobalForwardDecls(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    xl6CgenEmitModuleGlobalDeclsEx(ctx, root, 1);
}

static void xl6CgenEmitModuleGlobalDecls(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    xl6CgenEmitModuleGlobalDeclsEx(ctx, root, 0);
}

static int xl6CgenModuleGlobalNeedsDefaultInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* typeNode)
{
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return 0;
    }
    if ( xl6CgenIsNullableType(typeNode) ) {
        /*
         * nullable 的静态存储已经由 C 运行时清零。不得沿用 T 的默认构造，
         * 尤其不能把 array<T>? 静默初始化成空 array<T>。
         */
        return xl6CgenIsNullableStringTypeNode(ctx, typeNode);
    }
    return xl6CTextEquals(typeNode->text, "string") ||
           xl6CgenIsWeakType(typeNode) ||
           xl6CgenIsXValueType(typeNode) ||
           xl6CgenModuleGlobalUsesTypedContainerStorage(ctx, node, typeNode) ||
           xl6CgenIsFutureLikeType(typeNode) ||
           xl6CgenIsChannelType(typeNode) ||
           xl6CgenIsSpecialCollectionType(ctx, typeNode) ||
           xl6CgenIsThreadType(typeNode) ||
           xl6CgenIsGeneratorType(typeNode) ||
           xl6CgenIsSyncHandleType(typeNode);
}

static void xl6CgenAddModuleGlobalCleanup(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* name;

    name = xl6CgenModuleGlobalName(node);
    if ( ctx == NULL || node == NULL || name == NULL || typeNode == NULL ) {
        return;
    }

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( xl6CgenIsOwnedStringTypeNode(ctx, concreteType) ) {
        xl6CgenAddStringCleanup(ctx, name, concreteType);
        return;
    }
    if ( xl6CgenIsWeakType(concreteType) ) {
        xl6CgenAddWeakCleanup(ctx, name, concreteType);
        return;
    }
    if ( xl6CgenNullableHasManagedInner(ctx, concreteType) ) {
        xl6CgenAddNullableManagedCleanup(ctx, name, concreteType);
        return;
    }
    if ( xl6CgenIsNativeBackedClassType(ctx, concreteType) ) {
        const XL6_AstNode* nativeClassNode = xl6CgenFindNativeBackedClass(ctx, concreteType->text);

        xl6CgenAddNativeClassCleanup(ctx, name, nativeClassNode);
        return;
    }
    if ( xl6CgenIsClassType(ctx, concreteType) ) {
        XL6_AstNode* classNode = xl6CgenClassNodeForType(ctx, concreteType);

        xl6CgenAddClassCleanup(ctx, name, classNode, concreteType);
        return;
    }
    if ( xl6CgenModuleGlobalUsesTypedContainerStorage(ctx, node, concreteType) ) {
        xl6CgenAddTypedContainerCleanup(ctx, name, concreteType);
        return;
    }
    if ( xl6CgenIsSpecialCollectionType(ctx, concreteType) ) {
        xl6CgenAddSpecialCollectionCleanup(ctx, name, concreteType);
        return;
    }
    if ( xl6CgenIsFunctionType(concreteType) ) {
        xl6CgenAddFunctionCleanup(ctx, name, concreteType);
        return;
    }

    xl6CgenAddFutureCleanup(ctx, name, concreteType);
    xl6CgenAddChannelCleanup(ctx, name, concreteType);
    xl6CgenAddThreadCleanup(ctx, name, concreteType);
    xl6CgenAddGeneratorCleanup(ctx, name, concreteType);
    xl6CgenAddSyncCleanup(ctx, name, concreteType);
    xl6CgenAddXValueCleanup(ctx, name, concreteType);
}

static int xl6CgenEmitModuleGlobalLoweredInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* initExpr)
{
    char tempName[64];
    const char* cType;
    const char* name;
    int emitted;

    if ( ctx == NULL || node == NULL || typeNode == NULL || initExpr == NULL ) {
        return 0;
    }
    name = xl6CgenModuleGlobalName(node);
    snprintf(tempName, sizeof(tempName), "__xl6_global_init_%d", ctx->tempId++);
    emitted = 0;
    if ( xl6CgenModuleGlobalUsesTypedContainerStorage(ctx, node, typeNode) ) {
        const XL6_AstNode* storageType = xl6CgenIsNullableType(typeNode)
            ? xl6CgenNullableInnerType(typeNode)
            : typeNode;
        int initByCopy = xl6CgenTypedContainerCanCopyFromExpr(ctx, typeNode, initExpr);
        int initByMove = xl6CgenTypedContainerCanMoveFromExpr(ctx, typeNode, initExpr);
        int initByImport = xl6CgenTypedContainerCanImportDynamicExpr(ctx, typeNode, initExpr);
        XL6_AstNode* memberFunctionType = NULL;
        char memberTempName[64];

        memberTempName[0] = '\0';
        if ( xl6CgenIsNullableType(typeNode) ) {
            const XL6_AstNode* literal =
                xl6CgenTypedContainerInitLiteral(ctx, storageType, initExpr);
            XL6_AstNode* rightType =
                initExpr->kind != XL6_AST_NULL && literal == NULL
                    ? xl6CgenCloneExprTypeForInference(ctx, initExpr)
                    : NULL;
            const XL6_AstNode* rightConcrete = xl6CgenIsNullableType(rightType)
                ? xl6CgenNullableInnerType(rightType)
                : rightType;
            XL6_AstNode* targetCanonical =
                xl6CgenCloneCanonicalTypeNode(ctx, storageType);
            XL6_AstNode* rightCanonical = rightConcrete != NULL
                ? xl6CgenCloneCanonicalTypeNode(ctx, rightConcrete)
                : NULL;
            XL6_HirOwnership ownership = xl6CgenHirOwnership(ctx, initExpr);
            int sourceShared =
                xl6CgenTypedContainerExprIsSharedStorage(ctx, initExpr);
            char sourceName[64];

            if ( initExpr->kind != XL6_AST_NULL && literal == NULL &&
                 (targetCanonical == NULL || rightCanonical == NULL ||
                  !xl6CgenTypeNodeEqual(targetCanonical, rightCanonical)) ) {
                xl6CgenError(
                    ctx,
                    initExpr,
                    "nullable module-global container requires null, the same container type, or a literal");
                xl6AstFree(targetCanonical);
                xl6AstFree(rightType);
                xl6AstFree(rightCanonical);
                return 1;
            }

            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "%s %s = ",
                xl6CgenTypedContainerCType(ctx, storageType),
                tempName);
            if ( initExpr->kind == XL6_AST_NULL ) {
                xl6OutputPuts("0", ctx->out);
            } else if ( literal != NULL ) {
                xl6CgenTypedContainerCreateExprMode(
                    ctx,
                    storageType,
                    "XRT_OBJMODE_SHARED");
            } else {
                snprintf(
                    sourceName,
                    sizeof(sourceName),
                    "__xl6_global_nullable_source_%d",
                    ctx->tempId++);
                /*
                 * source 单独求值，随后再按对象域选择 retain/copy。
                 * 这样函数调用、?? 和安全导航都只执行一次。
                 */
                xl6OutputPuts("0;\n", ctx->out);
                xl6CgenIndent(ctx);
                xl6OutputPrintf(
                    ctx->out,
                    "%s %s = ",
                    xl6CgenTypedContainerCType(ctx, storageType),
                    sourceName);
                xl6CgenExpr(ctx, initExpr);
                xl6OutputPuts(";\n", ctx->out);
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "%s = %s != 0 ? ", tempName, sourceName);
                if ( sourceShared ) {
                    xl6CgenTypedContainerRetainCExpr(ctx, storageType, sourceName);
                } else {
                    xl6CgenTypedContainerCopyCExprMode(
                        ctx,
                        storageType,
                        sourceName,
                        "XRT_OBJMODE_SHARED");
                }
                xl6OutputPuts(" : 0;\n", ctx->out);
                if ( ownership == XL6_HIR_OWNERSHIP_MOVE && !sourceShared ) {
                    xl6CgenIndent(ctx);
                    xl6CgenTypedContainerDestroyValue(
                        ctx,
                        storageType,
                        sourceName);
                }
            }
            xl6OutputPuts(";\n", ctx->out);
            if ( literal != NULL ) {
                xl6CgenTypedContainerFillFromLiteral(
                    ctx,
                    tempName,
                    storageType,
                    initExpr);
            }
            xl6AstFree(targetCanonical);
            xl6AstFree(rightType);
            xl6AstFree(rightCanonical);
            emitted = 1;
            goto module_global_typed_container_done;
        }
        if ( initByMove ) {
            memberFunctionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, initExpr);
            if ( memberFunctionType != NULL ) {
                snprintf(memberTempName, sizeof(memberTempName), "__xl6_member_fn_stmt_%d", ctx->tempId++);
                xl6CgenEmitMemberFunctionStatementTemp(ctx, initExpr, memberTempName);
            }
        }
        if ( initByMove &&
             memberFunctionType == NULL &&
             xl6CgenDeclCallExprArgsNeedCallArgTemps(ctx, initExpr) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, typeNode), tempName);
            xl6OutputPuts("0", ctx->out);
            xl6OutputPuts(";\n", ctx->out);
            if ( !xl6CgenDeclEmitTypedContainerCallExprWithArgTemps(ctx, typeNode, initExpr, tempName) ) {
                xl6CgenError(ctx, initExpr, "module global typed container call argument lowering failed");
            }
            emitted = 1;
        }
        if ( !emitted ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, typeNode), tempName);
            if ( initByCopy ) {
                if ( xl6CgenTypedContainerExprIsSharedStorage(ctx, initExpr) ) {
            xl6CgenTypedContainerRetainExpr(ctx, typeNode, initExpr);
                } else {
                    xl6CgenTypedContainerCopyExprMode(ctx, typeNode, initExpr, "XRT_OBJMODE_SHARED");
                }
            } else if ( initByMove ) {
                if ( memberFunctionType != NULL ) {
                    xl6CgenMemberFunctionStatementInvokeExpr(ctx, initExpr, memberFunctionType, memberTempName);
                } else {
                    xl6CgenExpr(ctx, initExpr);
                }
            } else if ( initByImport ) {
                xl6CgenTypedContainerImportDynamicExprMode(
                    ctx,
                    typeNode,
                    initExpr,
                    "XRT_OBJMODE_SHARED");
            } else {
                xl6CgenTypedContainerCreateExprMode(ctx, typeNode, "XRT_OBJMODE_SHARED");
            }
            xl6OutputPuts(";\n", ctx->out);
            if ( initByImport ) {
                xl6CgenTypedContainerImportCheckStmt(ctx, tempName, initExpr);
            }
            if ( !initByCopy && !initByMove && !initByImport ) {
                XL6_CgenTaskSpawnArgTempScope taskScope;
                int hasTaskScope;

                hasTaskScope = initExpr != NULL &&
                               xl6CgenTaskSpawnArgTempScopeBegin(ctx, initExpr, &taskScope);
                xl6CgenTypedContainerFillFromLiteral(ctx, tempName, typeNode, initExpr);
                if ( hasTaskScope ) {
                    xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
                }
            }
            emitted = 1;
        }
        if ( emitted && initByMove ) {
            char sharedTempName[64];

            snprintf(sharedTempName, sizeof(sharedTempName), "__xl6_global_shared_%d", ctx->tempId++);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, typeNode), sharedTempName);
            xl6CgenTypedContainerCopyCExprMode(ctx, typeNode, tempName, "XRT_OBJMODE_SHARED");
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6CgenTypedContainerDestroyValue(ctx, typeNode, tempName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = %s;\n", tempName, sharedTempName);
        }
        xl6AstFree(memberFunctionType);
module_global_typed_container_done:
        ;
    } else if ( xl6CgenIsXValueType(typeNode) ) {
        if ( initExpr->kind == XL6_AST_CALL && xl6CgenDeclExprNeedsCallArgTemps(ctx, initExpr) ) {
            xl6CgenXValueLocalInitStmt(ctx, typeNode, tempName, initExpr);
            emitted = 1;
        } else if ( xl6CgenXValueExprNeedsSpecialStmt(ctx, initExpr) ) {
            xl6CgenXValueLocalInitStmt(ctx, typeNode, tempName, initExpr);
            emitted = 1;
        } else {
            emitted = xl6CgenDeclTryXValueArrayLiteralInit(ctx, typeNode, tempName, initExpr) ||
                      xl6CgenDeclTryXValueNullCoalesceInit(ctx, typeNode, tempName, initExpr) ||
                      xl6CgenDeclTryXValueBinaryInit(ctx, typeNode, tempName, initExpr) ||
                      xl6CgenDeclTryXValueConditionalInit(ctx, typeNode, tempName, initExpr) ||
                      xl6CgenDeclTryTaskSpawnArgTempsInit(ctx, typeNode, "xvalue", tempName, initExpr);
        }
    } else if ( !xl6CgenIsFunctionType(typeNode) ) {
        cType = xl6CgenTypeName(ctx, typeNode);
        emitted = xl6CgenDeclTryStaticNullCoalesceInit(ctx, typeNode, cType, tempName, initExpr) ||
                  xl6CgenDeclTryStaticXValueCompareInit(ctx, typeNode, cType, tempName, initExpr) ||
                  xl6CgenDeclTryStaticXValueApproxInit(ctx, typeNode, cType, tempName, initExpr) ||
                  xl6CgenDeclTryTaskSpawnArgTempsInit(ctx, typeNode, cType, tempName, initExpr);
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
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xl6_xrt_move_to_shared(%s);\n", tempName, tempName);
    }
    xl6CgenIndent(ctx);
    xl6CgenFputsValueStorageName(ctx, name);
    xl6OutputPuts(" = ", ctx->out);
    /*
        语句级初始化临时量已经遵守目标类型的拥有语义。这里直接移交给模块存储；
        再复制一次不仅浪费内存，还会遗失无人负责释放的中间字符串。
    */
    xl6OutputPuts(tempName, ctx->out);
    xl6OutputPuts(";\n", ctx->out);
    if ( xl6CgenIsStringTypeNode(ctx, typeNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (", ctx->out);
        xl6CgenFputsValueStorageName(ctx, name);
        xl6OutputPuts(" == 0) { xl6_runtime_fatal(3); }\n", ctx->out);
    }
    return 1;
}

