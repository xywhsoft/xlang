static void xl6CgenAddCleanupEntry(XL6_CgenContext* ctx, const char* name, XL6_CgenCleanupKind kind, const XL6_AstNode* typeNode, const XL6_AstNode* classTypeNode)
{
    XL6_CgenCleanup* newItems;
    size_t newCapacity;
    char cName[512];

    if ( ctx == NULL || name == NULL || typeNode == NULL ) {
        return;
    }
    if ( ctx->cleanupCount == ctx->cleanupCapacity ) {
        newCapacity = ctx->cleanupCapacity == 0 ? 16 : ctx->cleanupCapacity * 2;
        newItems = (XL6_CgenCleanup*)xrtRealloc(ctx->cleanups, newCapacity * sizeof(XL6_CgenCleanup));
        if ( newItems == NULL ) {
            xl6CgenError(ctx, typeNode, "out of memory while growing cgen cleanup stack");
            return;
        }
        ctx->cleanups = newItems;
        ctx->cleanupCapacity = newCapacity;
    }
    xl6CgenValueStorageNameText(ctx, cName, sizeof(cName), name);
    ctx->cleanups[ctx->cleanupCount].name = xrtCopyStr(cName, strlen(cName));
    ctx->cleanups[ctx->cleanupCount].kind = kind;
    ctx->cleanups[ctx->cleanupCount].typeNode = typeNode;
    ctx->cleanups[ctx->cleanupCount].classTypeNode = classTypeNode;
    ctx->cleanups[ctx->cleanupCount].flags = 0u;
    ctx->cleanups[ctx->cleanupCount].scopeDepth =
        ctx->currentScopeDepth != 0 || ctx->currentReturnFunc == NULL
            ? ctx->currentScopeDepth
            : 1;
    ctx->cleanupCount += 1;
}

static void xl6CgenMarkModuleGlobalCleanupsSince(
    XL6_CgenContext* ctx,
    size_t mark)
{
    size_t i;

    if ( ctx == NULL || mark > ctx->cleanupCount ) {
        return;
    }
    for ( i = mark; i < ctx->cleanupCount; ++i ) {
        ctx->cleanups[i].flags |= XL6_CGEN_CLEANUP_MODULE_GLOBAL;
    }
}

static size_t xl6CgenCleanupMarkForScopeDepth(
    const XL6_CgenContext* ctx,
    uint32 targetScopeDepth
)
{
    size_t i;

    if ( ctx == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->cleanupCount; ++i ) {
        if ( ctx->cleanups[i].scopeDepth > targetScopeDepth ) {
            return i;
        }
    }
    return ctx->cleanupCount;
}

static void xl6CgenAddNativeClassCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* nativeClassNode)
{
    if ( nativeClassNode == NULL || !xl6CgenNativeBackedClassHasDestroy(nativeClassNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_NATIVE_CLASS, nativeClassNode, NULL);
}

static void xl6CgenAddClassCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* classNode, const XL6_AstNode* classTypeNode)
{
    if ( classNode == NULL || !xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_CLASS, classNode, classTypeNode);
}

static void xl6CgenAddFunctionCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsFunctionType(typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_FUNCTION, typeNode, NULL);
}

static void xl6CgenAddNullableManagedCleanup(
    XL6_CgenContext* ctx,
    const char* name,
    const XL6_AstNode* typeNode)
{
    if ( !xl6CgenNullableHasManagedInner(ctx, typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(
        ctx,
        name,
        XL6_CGEN_CLEANUP_NULLABLE_MANAGED,
        typeNode,
        NULL);
}

static void xl6CgenAddTypedValueCleanup(
    XL6_CgenContext* ctx,
    const char* name,
    const XL6_AstNode* typeNode)
{
    if ( ctx == NULL || name == NULL || typeNode == NULL ||
         !xl6CgenIsTaggedEnumType(ctx, typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(
        ctx,
        name,
        XL6_CGEN_CLEANUP_TYPED_VALUE,
        typeNode,
        NULL);
}

static void xl6CgenAddFutureCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsFutureLikeType(typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_FUTURE, typeNode, NULL);
}

static void xl6CgenAddChannelCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsChannelType(typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_CHANNEL, typeNode, NULL);
}

static int xl6CgenCleanupIsVarChannel(const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;
    const XL6_AstNode* itemType;

    if ( !xl6CgenIsChannelType(typeNode) ||
         typeNode->childCount == 0 ||
         typeNode->children[0] == NULL ||
         typeNode->children[0]->kind != XL6_AST_LIST ) {
        return 0;
    }
    args = typeNode->children[0];
    itemType = args->childCount > 0 ? args->children[0] : NULL;
    return itemType != NULL &&
           itemType->text != NULL &&
           xl6CTextEquals(itemType->text, "var");
}

static void xl6CgenAddSpecialCollectionCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsSpecialCollectionType(ctx, typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_SPECIAL_COLLECTION, typeNode, NULL);
}

static void xl6CgenAddThreadCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsThreadType(typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_THREAD, typeNode, NULL);
}

static void xl6CgenAddGeneratorCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsGeneratorType(typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_GENERATOR, typeNode, NULL);
}

static void xl6CgenAddSyncCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsSyncHandleType(typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_SYNC, typeNode, NULL);
}

static void xl6CgenAddXValueCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsXValueType(typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_XVALUE, typeNode, NULL);
}

static void xl6CgenAddTypedContainerCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_TYPED_CONTAINER, typeNode, NULL);
}

static void xl6CgenAddStringCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_STRING, typeNode, NULL);
}

static void xl6CgenAddTypedContainerParamCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_TYPED_CONTAINER_PARAM, typeNode, NULL);
}

static void xl6CgenAddErrorCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_ERROR, typeNode, NULL);
}

static void xl6CgenAddWeakCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    if ( !xl6CgenIsWeakType(typeNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_WEAK, typeNode, NULL);
}

static void xl6CgenBeginTempScope(XL6_CgenContext* ctx, const XL6_AstNode* atNode)
{
    char name[64];

    if ( ctx == NULL || atNode == NULL ) {
        return;
    }
    snprintf(name, sizeof(name), "__xl6_temp_scope_%d", ctx->tempId++);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "XL6_TempScope %s = xl6_temp_scope_begin();\n", name);
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_TEMP_SCOPE, atNode, NULL);
}

/*
 * 属性访问在表达式层是借用值，但 getter 函数必须返回一份独立所有权。
 * 将这份所有权提升到调用者的根表达式作用域：赋值/传参会再 retain，
 * 纯链式访问则在调用者语句结束时自动释放。
 */
static void xl6CgenPromotePropertyManagedReturn(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* returnName)
{
    const XL6_AstNode* concreteType;
    const char* dropName;
    const char* cType;
    const char* rootScopeName;
    size_t i;

    if ( ctx == NULL || typeNode == NULL || returnName == NULL ||
         ctx->currentReturnFunc == NULL ||
         ctx->currentReturnFunc->kind != XL6_AST_GET ) {
        return;
    }

    dropName = NULL;
    cType = NULL;
    if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
        dropName = xl6CgenTypedContainerDestroyFuncName(
            concreteType != NULL ? concreteType->text : NULL);
        cType = xl6CgenTypedContainerCType(ctx, typeNode);
    } else if ( xl6CgenIsXValueType(typeNode) ||
                xl6CgenClassHandleTargetType(ctx, typeNode) != NULL ) {
        dropName = "xvoUnref";
        cType = xl6CgenValueStorageCType(ctx, typeNode);
    }
    if ( dropName == NULL || cType == NULL ) {
        return;
    }

    rootScopeName = NULL;
    for ( i = 0; i < ctx->cleanupCount; ++i ) {
        if ( ctx->cleanups[i].kind == XL6_CGEN_CLEANUP_TEMP_SCOPE ) {
            rootScopeName = ctx->cleanups[i].name;
            break;
        }
    }
    if ( rootScopeName == NULL ) {
        xl6CgenError(ctx, ctx->currentReturnFunc, "property return has no root temporary scope");
        return;
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(
        ctx->out,
        "%s = (%s)xl6_temp_promote_to((void*)%s, "
        "(XL6_TempManagedDrop)%s, %s.depth > 0u ? %s.depth - 1u : 0u);\n",
        returnName,
        cType,
        returnName,
        dropName,
        rootScopeName,
        rootScopeName);
}

static void xl6CgenFreeCleanupsSince(XL6_CgenContext* ctx, size_t mark)
{
    size_t i;

    if ( ctx == NULL || mark > ctx->cleanupCount ) {
        return;
    }
    for ( i = mark; i < ctx->cleanupCount; ++i ) {
        xrtFree(ctx->cleanups[i].name);
    }
    ctx->cleanupCount = mark;
}

static void xl6CgenFreeCleanups(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    xl6CgenFreeCleanupsSince(ctx, 0);
    xrtFree(ctx->cleanups);
    ctx->cleanups = NULL;
    ctx->cleanupCapacity = 0;
    for ( i = 0; i < ctx->errorScopeCount; ++i ) {
        xrtFree(ctx->errorScopes[i].checkpoints);
    }
    xrtFree(ctx->errorScopes);
    ctx->errorScopes = NULL;
    ctx->errorScopeCount = 0;
    ctx->errorScopeCapacity = 0;
}

static int xl6CgenNativeClassCleanupIsTracked(XL6_CgenContext* ctx, const char* name)
{
    char cName[512];
    size_t i;

    if ( ctx == NULL || name == NULL ) {
        return 0;
    }
    xl6CgenValueStorageNameText(ctx, cName, sizeof(cName), name);
    for ( i = ctx->cleanupCount; i > 0; --i ) {
        const XL6_CgenCleanup* cleanup = &ctx->cleanups[i - 1];
        if ( cleanup->kind == XL6_CGEN_CLEANUP_NATIVE_CLASS &&
             cleanup->name != NULL &&
             xl6CTextEquals(cleanup->name, cName) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenClassCleanupIsTracked(XL6_CgenContext* ctx, const char* name)
{
    char cName[512];
    size_t i;

    if ( ctx == NULL || name == NULL ) {
        return 0;
    }
    xl6CgenValueStorageNameText(ctx, cName, sizeof(cName), name);
    for ( i = ctx->cleanupCount; i > 0; --i ) {
        const XL6_CgenCleanup* cleanup = &ctx->cleanups[i - 1];
        if ( cleanup->kind == XL6_CGEN_CLEANUP_CLASS &&
             cleanup->name != NULL &&
             xl6CTextEquals(cleanup->name, cName) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenClearMovedNativeClassReturnSource(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    char cName[512];

    if ( expr == NULL || expr->kind != XL6_AST_NAME || expr->text == NULL ||
         !xl6CgenNativeClassCleanupIsTracked(ctx, expr->text) ) {
        return;
    }
    xl6CgenValueStorageNameText(ctx, cName, sizeof(cName), expr->text);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.handle = 0;\n", cName);
}

static void xl6CgenEmitOneCleanup(XL6_CgenContext* ctx, const XL6_CgenCleanup* cleanup)
{
    const XL6_AstNode* destroyNode;

    if ( ctx == NULL || cleanup == NULL || cleanup->name == NULL ) {
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_ERROR ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xl6_error_free(%s);\n", cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_ITER_SNAPSHOT ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xl6_xrt_iter_snapshot_free(%s);\n", cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_XRT_PTR ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xrtFree(%s);\n", cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_STRING ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xrtFree((void*)%s);\n", cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_TEMP_SCOPE ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xl6_temp_scope_end(&%s);\n", cleanup->name);
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_WEAK ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvoWeakUnit(&%s);\n", cleanup->name);
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_NULLABLE_MANAGED ) {
        char dropName[64];

        xl6CgenNullableDropHelperName(
            dropName,
            sizeof(dropName),
            cleanup->typeNode);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s(&%s);\n", dropName, cleanup->name);
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_TYPED_VALUE ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("xrtTypeDropValue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, cleanup->typeNode);
        xl6OutputPrintf(ctx->out, ", &%s);\n", cleanup->name);
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_TYPED_CONTAINER_PARAM ) {
        char ownerName[512];

        xl6CgenTypedParamOwnerName(ownerName, sizeof(ownerName), cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s && %s != 0) {\n", ownerName, cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6CgenTypedContainerDestroyValue(ctx, cleanup->typeNode, cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( cleanup->typeNode == NULL ) {
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_FUTURE ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xFutureRelease(%s);\n", cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_CHANNEL ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out,
                        "%s(%s);\n",
                        xl6CgenCleanupIsVarChannel(cleanup->typeNode) ? "xl6_channel_destroy_xvalue" : "xl6_channel_destroy",
                        cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_SPECIAL_COLLECTION ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6CgenSpecialCollectionReleaseCExpr(ctx, cleanup->typeNode, cleanup->name);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_THREAD ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xl6_thread_release(%s);\n", cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_GENERATOR ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xl6_generator_destroy(%s);\n", cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_SYNC ) {
        const char* typeName = cleanup->typeNode != NULL ? cleanup->typeNode->text : NULL;
        const char* destroyName = NULL;

        if ( xl6CTextEquals(typeName, "mutex") ) {
            destroyName = "xrtMutexDestroy";
        } else if ( xl6CTextEquals(typeName, "sem") ) {
            destroyName = "xrtSemDestroy";
        } else if ( xl6CTextEquals(typeName, "cond") ) {
            destroyName = "xrtCondDestroy";
        } else if ( xl6CTextEquals(typeName, "rwlock") ) {
            destroyName = "xrtRWLockDestroy";
        }
        if ( destroyName == NULL ) {
            return;
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s(%s);\n", destroyName, cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( cleanup->kind == XL6_CGEN_CLEANUP_NATIVE_CLASS ) {
        destroyNode = xl6CgenFindNativeBackedClassSpecial(cleanup->typeNode, "destroy");
        if ( destroyNode == NULL ) {
            return;
        }

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s.handle != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6CgenCStringSymbol(ctx->out, destroyNode->text);
        xl6OutputPrintf(ctx->out, "(%s.handle);\n", cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s.handle = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }

    if ( cleanup->kind == XL6_CGEN_CLEANUP_FUNCTION ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s.release != 0 && %s.env != 0) {\n", cleanup->name, cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s.release(%s.env);\n", cleanup->name, cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s.env = 0;\n", cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s.release = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }

    if ( cleanup->kind == XL6_CGEN_CLEANUP_XVALUE ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "(void)xl6_temp_managed_disown((void*)%s, (XL6_TempManagedDrop)xvoUnref);\n",
            cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", cleanup->name);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }

    if ( cleanup->kind == XL6_CGEN_CLEANUP_TYPED_CONTAINER ) {
        const XL6_AstNode* concreteType = xl6CgenTypedContainerConcreteType(ctx, cleanup->typeNode);
        const char* typeName = concreteType != NULL ? concreteType->text : NULL;

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cleanup->name);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        if ( xl6CTextEquals(typeName, "array") ) {
            xl6OutputPrintf(ctx->out, "(void)xl6_temp_managed_disown((void*)%s, (XL6_TempManagedDrop)xrtTypedArrayDestroy);\n", cleanup->name);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xrtTypedArrayDestroy(%s);\n", cleanup->name);
        } else if ( xl6CTextEquals(typeName, "list") ) {
            xl6OutputPrintf(ctx->out, "(void)xl6_temp_managed_disown((void*)%s, (XL6_TempManagedDrop)xrtTypedListDestroy);\n", cleanup->name);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xrtTypedListDestroy(%s);\n", cleanup->name);
        } else if ( xl6CTextEquals(typeName, "set") ) {
            xl6OutputPrintf(ctx->out, "(void)xl6_temp_managed_disown((void*)%s, (XL6_TempManagedDrop)xrtTypedSetDestroy);\n", cleanup->name);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xrtTypedSetDestroy(%s);\n", cleanup->name);
        } else if ( xl6CTextEquals(typeName, "dict") ) {
            xl6OutputPrintf(ctx->out, "(void)xl6_temp_managed_disown((void*)%s, (XL6_TempManagedDrop)xrtTypedDictDestroy);\n", cleanup->name);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xrtTypedDictDestroy(%s);\n", cleanup->name);
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", cleanup->name);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }

    if ( !xl6CgenClassNeedsDestroy(ctx, cleanup->typeNode) ) {
        return;
    }

    xl6CgenIndent(ctx);
    if ( cleanup->classTypeNode != NULL && cleanup->classTypeNode->childCount > 0 ) {
        xl6CgenGenericClassMemberSymbol(ctx, cleanup->typeNode, cleanup->classTypeNode, "destroy");
    } else {
    xl6CgenClassMemberSymbol(ctx->out, cleanup->typeNode->text, "destroy");
    }
    xl6OutputPrintf(ctx->out, "(&%s);\n", cleanup->name);
}

static void xl6CgenRetainFunctionValue(XL6_CgenContext* ctx, const char* name)
{
    char cName[512];

    xl6CgenValueStorageNameText(ctx, cName, sizeof(cName), name);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s.retain != 0 && %s.env != 0) {\n", cName, cName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.retain(%s.env);\n", cName, cName);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenReleaseFunctionValue(XL6_CgenContext* ctx, const char* name)
{
    char cName[512];

    xl6CgenValueStorageNameText(ctx, cName, sizeof(cName), name);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s.release != 0 && %s.env != 0) {\n", cName, cName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.release(%s.env);\n", cName, cName);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static int xl6CgenExprIsFunctionBorrow(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* typeNode;

    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }
    if ( expr->kind == XL6_AST_CONDITIONAL && expr->childCount > 2 ) {
        return xl6CgenExprIsFunctionBorrow(ctx, expr->children[1]) &&
               xl6CgenExprIsFunctionBorrow(ctx, expr->children[2]);
    }
    if ( expr->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* memberType = xl6CgenCloneMemberFieldTypeForInference(ctx, expr);
        int result = xl6CgenIsFunctionType(memberType);
        xl6AstFree(memberType);
        return result;
    }
    if ( expr->kind == XL6_AST_AWAIT ) {
        if ( xl6CgenAwaitExprReturnsOwnedTemporaryCtx(ctx, expr) ) {
            return 0;
        }
        XL6_AstNode* awaitType = xl6CgenCloneExprTypeForInference(ctx, expr);
        int result = xl6CgenIsFunctionType(awaitType);
        xl6AstFree(awaitType);
        return result;
    }
    if ( expr->kind != XL6_AST_NAME ) {
        return 0;
    }
    typeNode = xl6CgenFindVarType(ctx, expr->text);
    return xl6CgenIsFunctionType(typeNode);
}

static int xl6CgenExprIsXValueBorrow(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* typeNode;
    XL6_HirOwnership ownership;
    size_t replacementIndex;

    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }
    for ( replacementIndex = 0; replacementIndex < ctx->exprReplacementCount; ++replacementIndex ) {
        const XL6_CgenExprReplacement* replacement = &ctx->exprReplacements[replacementIndex];

        if ( replacement->expr == expr && replacement->cName != NULL &&
             (replacement->flags & XL6_CGEN_EXPR_REPLACEMENT_MOVE) != 0u ) {
            return 0;
        }
    }
    /*
     * 源表达式的所有权已经由语义层写入 typed HIR，后端不得再次猜测。
     * BORROW/SHARED 只有在表达式真实使用 xvalue 存储时才需要非消费读取；
     * 普通 int/string 等值即使标记为借用，也不能误走 xvalue helper。
     */
    ownership = xl6CgenHirOwnership(ctx, expr);
    if ( xl6CgenHirOwnershipIsPlanned(ctx, expr) ) {
        if ( ownership == XL6_HIR_OWNERSHIP_MOVE ||
             ownership == XL6_HIR_OWNERSHIP_COPY ) {
            return 0;
        }
        if ( ownership == XL6_HIR_OWNERSHIP_BORROW ||
             ownership == XL6_HIR_OWNERSHIP_SHARED ) {
            XL6_AstNode* exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
            int isXValue = xl6CgenIsXValueType(exprType);

            xl6AstFree(exprType);
            if ( isXValue ) {
                return 1;
            }
        }
    }
    if ( expr->kind == XL6_AST_NAME &&
         expr->semanticSymbolId == XL6_SYMBOL_ID_INVALID &&
         ownership == XL6_HIR_OWNERSHIP_BORROW ) {
        /* 当前 class 的隐式字段名称没有局部符号，但语义层明确标记为借用。 */
        return 1;
    }
    if ( expr->kind == XL6_AST_MEMBER || expr->kind == XL6_AST_SAFE_MEMBER ) {
        const XL6_AstNode* receiver;
        XL6_AstNode* receiverType;
        XL6_AstNode* fieldType;
        int receiverIsClassHandle;
        int result;

        if ( expr->text != NULL && xl6CTextEquals(expr->text, "length") ) {
            return 0;
        }
        receiver = expr->childCount > 0 ? expr->children[0] : NULL;
        receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
        receiverIsClassHandle = xl6CgenClassHandleTargetType(ctx, receiverType) != NULL;
        if ( !receiverIsClassHandle && xl6CgenIsXValueType(receiverType) ) {
            xl6AstFree(receiverType);
            return 0;
        }
        xl6AstFree(receiverType);
        if ( expr->kind == XL6_AST_SAFE_MEMBER ) {
            return 0;
        }

        fieldType = xl6CgenCloneMemberFieldTypeForInference(ctx, expr);
        result = xl6CgenIsXValueType(fieldType);
        xl6AstFree(fieldType);
        return result;
    }
    if ( expr->kind == XL6_AST_INDEX || expr->kind == XL6_AST_SAFE_INDEX ) {
        const XL6_AstNode* receiver = expr->childCount > 0 ? expr->children[0] : NULL;
        XL6_AstNode* receiverType;
        int receiverIsVar;

        if ( xl6CgenExprUsesTypedContainerStorage(ctx, receiver) ) {
            return 0;
        }
        receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
        receiverIsVar = receiverType != NULL && xl6CTextEquals(receiverType->text, "var");
        xl6AstFree(receiverType);

        /*
         * 字符串安全索引和 var 字符串索引通过 record-field 桥返回拥有值。
         * 数值索引与运行期 var key 则直接借用容器元素，必须跟随 receiver 的所有权。
         */
        if ( expr->kind == XL6_AST_SAFE_INDEX || receiverIsVar ) {
            return 0;
        }
        return xl6CgenExprIsXValueBorrow(ctx, receiver);
    }
    if ( expr->kind == XL6_AST_AWAIT ) {
        XL6_AstNode* awaitType;
        int result;

        if ( xl6CgenAwaitExprReturnsOwnedTemporaryCtx(ctx, expr) ) {
            return 0;
        }
        awaitType = xl6CgenCloneExprTypeForInference(ctx, expr);
        result = xl6CgenIsXValueType(awaitType);
        xl6AstFree(awaitType);
        return result;
    }
    if ( expr->kind != XL6_AST_NAME ) {
        return 0;
    }
    if ( ctx->currentClass != NULL &&
         (xl6CTextEquals(expr->text, "self") || xl6CTextEquals(expr->text, "base")) ) {
        return 1;
    }
    if ( xl6CgenExprUsesTypedContainerStorage(ctx, expr) ) {
        return 0;
    }
    typeNode = xl6CgenFindVarType(ctx, expr->text);
    return xl6CgenIsXValueType(typeNode);
}

static void xl6CgenEmitCleanupsSinceExcept(XL6_CgenContext* ctx, size_t mark, const char* skipName)
{
    size_t i;

    if ( ctx == NULL || mark > ctx->cleanupCount ) {
        return;
    }
    for ( i = ctx->cleanupCount; i > mark; --i ) {
        const XL6_CgenCleanup* cleanup = &ctx->cleanups[i - 1];
        if ( skipName != NULL && cleanup->name != NULL && xl6CTextEquals(cleanup->name, skipName) ) {
            continue;
        }
        xl6CgenEmitOneCleanup(ctx, cleanup);
    }
}

static void xl6CgenEmitCleanupsSince(XL6_CgenContext* ctx, size_t mark)
{
    xl6CgenEmitCleanupsSinceExcept(ctx, mark, NULL);
}

static void xl6CgenEmitModuleCleanups(
    XL6_CgenContext* ctx,
    size_t mark,
    int moduleGlobals)
{
    size_t i;

    if ( ctx == NULL || mark > ctx->cleanupCount ) {
        return;
    }
    for ( i = ctx->cleanupCount; i > mark; --i ) {
        const XL6_CgenCleanup* cleanup = &ctx->cleanups[i - 1];
        int isModuleGlobal =
            (cleanup->flags & XL6_CGEN_CLEANUP_MODULE_GLOBAL) != 0;

        if ( isModuleGlobal == moduleGlobals ) {
            xl6CgenEmitOneCleanup(ctx, cleanup);
        }
    }
}

static void xl6CgenEmitCleanupRange(XL6_CgenContext* ctx, size_t mark, size_t end)
{
    size_t i;

    if ( ctx == NULL || mark > end || end > ctx->cleanupCount ) {
        return;
    }
    for ( i = end; i > mark; --i ) {
        xl6CgenEmitOneCleanup(ctx, &ctx->cleanups[i - 1]);
    }
}

static XL6_CgenErrorScope* xl6CgenCurrentErrorScope(XL6_CgenContext* ctx)
{
    if ( ctx == NULL || ctx->errorScopeCount == 0 ) {
        return NULL;
    }
    return &ctx->errorScopes[ctx->errorScopeCount - 1];
}

static int xl6CgenPushErrorScope(XL6_CgenContext* ctx, size_t cleanupMark)
{
    XL6_CgenErrorScope* items;
    XL6_CgenErrorScope* scope;
    XL6_CgenTryTarget* target;
    size_t capacity;

    if ( ctx == NULL || !ctx->needsErrorRuntime ) {
        return 0;
    }
    if ( ctx->errorScopeCount == ctx->errorScopeCapacity ) {
        capacity = ctx->errorScopeCapacity == 0 ? 16 : ctx->errorScopeCapacity * 2;
        items = (XL6_CgenErrorScope*)xrtRealloc(
            ctx->errorScopes,
            capacity * sizeof(XL6_CgenErrorScope));
        if ( items == NULL ) {
            xl6CgenError(ctx, NULL, "out of memory while growing cgen error scope stack");
            return 0;
        }
        ctx->errorScopes = items;
        ctx->errorScopeCapacity = capacity;
    }

    scope = &ctx->errorScopes[ctx->errorScopeCount++];
    memset(scope, 0, sizeof(*scope));
    scope->cleanupMark = cleanupMark;
    target = xl6CgenCurrentTryTarget(ctx);
    if ( target != NULL ) {
        scope->target = *target;
        scope->hasTarget = 1;
    }
    return 1;
}

static XL6_CgenErrorCheckpoint* xl6CgenErrorScopeCheckpoint(
    XL6_CgenContext* ctx,
    XL6_CgenErrorScope* scope,
    size_t cleanupCount)
{
    XL6_CgenErrorCheckpoint* items;
    size_t i;
    size_t capacity;

    if ( ctx == NULL || scope == NULL || cleanupCount < scope->cleanupMark ) {
        return NULL;
    }
    for ( i = 0; i < scope->checkpointCount; ++i ) {
        if ( scope->checkpoints[i].cleanupCount == cleanupCount ) {
            return &scope->checkpoints[i];
        }
    }
    if ( scope->checkpointCount == scope->checkpointCapacity ) {
        capacity = scope->checkpointCapacity == 0 ? 8 : scope->checkpointCapacity * 2;
        items = (XL6_CgenErrorCheckpoint*)xrtRealloc(
            scope->checkpoints,
            capacity * sizeof(XL6_CgenErrorCheckpoint));
        if ( items == NULL ) {
            xl6CgenError(ctx, NULL, "out of memory while growing cgen error checkpoints");
            return NULL;
        }
        scope->checkpoints = items;
        scope->checkpointCapacity = capacity;
    }
    scope->checkpoints[scope->checkpointCount].cleanupCount = cleanupCount;
    scope->checkpoints[scope->checkpointCount].id = ctx->tempId++;
    scope->checkpointCount += 1;
    return &scope->checkpoints[scope->checkpointCount - 1];
}

static int xl6CgenEmitCurrentErrorScopeJump(XL6_CgenContext* ctx)
{
    XL6_CgenErrorScope* scope = xl6CgenCurrentErrorScope(ctx);
    XL6_CgenErrorCheckpoint* checkpoint;

    if ( scope == NULL ) {
        return 0;
    }
    checkpoint = xl6CgenErrorScopeCheckpoint(ctx, scope, ctx->cleanupCount);
    if ( checkpoint == NULL ) {
        return 0;
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "goto __xl6_cleanup_error_%d;\n", checkpoint->id);
    return 1;
}

static void xl6CgenCloseErrorScope(XL6_CgenContext* ctx, size_t cleanupMark)
{
    XL6_CgenErrorScope scope;
    XL6_CgenErrorCheckpoint* parentCheckpoint;
    size_t terminalMark;
    size_t i;
    size_t lowerMark;

    if ( ctx == NULL || ctx->errorScopeCount == 0 ) {
        xl6CgenCloseScope(ctx, cleanupMark);
        return;
    }

    scope = ctx->errorScopes[ctx->errorScopeCount - 1];
    ctx->errorScopeCount -= 1;
    if ( scope.cleanupMark != cleanupMark || scope.checkpointCount == 0 ) {
        xrtFree(scope.checkpoints);
        xl6CgenCloseScope(ctx, cleanupMark);
        return;
    }

    parentCheckpoint = NULL;
    if ( !scope.hasTarget && ctx->errorScopeCount > 0 ) {
        XL6_CgenErrorScope* parent =
            &ctx->errorScopes[ctx->errorScopeCount - 1];

        /*
            子作用域只销毁自己新增的值，再跳入父作用域的清理链。
            不能在每个子出口重复内联全部父级清理；模块拥有大量全局值时，
            那种写法会令生成代码按“全局数量 x 错误出口数量”平方级膨胀。
        */
        parentCheckpoint =
            xl6CgenErrorScopeCheckpoint(ctx, parent, cleanupMark);
    }

    /* 正常路径跳过异常清理标签。 */
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "goto __xl6_cleanup_done_%d;\n", scope.checkpoints[scope.checkpointCount - 1].id);

    /*
        checkpoint 按源码生成顺序递增。每段只销毁前一个里程碑之后新增的值，
        因而不会触碰错误点之后尚未初始化的局部变量。
    */
    for ( i = scope.checkpointCount; i > 0; --i ) {
        XL6_CgenErrorCheckpoint* checkpoint = &scope.checkpoints[i - 1];
        lowerMark = i > 1 ? scope.checkpoints[i - 2].cleanupCount : cleanupMark;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_cleanup_error_%d:\n", checkpoint->id);
        xl6CgenEmitCleanupRange(ctx, lowerMark, checkpoint->cleanupCount);
        if ( i > 1 ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "goto __xl6_cleanup_error_%d;\n", scope.checkpoints[i - 2].id);
            continue;
        }
        terminalMark = scope.hasTarget ? scope.target.cleanupMark : 0;
        if ( parentCheckpoint == NULL ) {
            xl6CgenEmitCleanupRange(ctx, terminalMark, cleanupMark);
        }
        if ( scope.hasTarget ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "goto __xl6_try_%s_%d;\n",
                scope.target.targetFinally ? "finally" : "catch",
                scope.target.id);
        } else if ( parentCheckpoint != NULL ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "goto __xl6_cleanup_error_%d;\n",
                parentCheckpoint->id);
        } else if ( ctx->currentErrorExitLabel != NULL ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "goto %s;\n", ctx->currentErrorExitLabel);
        } else {
            xl6CgenEmitDefaultErrorReturn(ctx);
        }
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_cleanup_done_%d:\n", scope.checkpoints[scope.checkpointCount - 1].id);
    xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    xrtFree(scope.checkpoints);
}

static void xl6CgenCloseScope(XL6_CgenContext* ctx, size_t mark)
{
    xl6CgenEmitCleanupsSince(ctx, mark);
    xl6CgenFreeCleanupsSince(ctx, mark);
}

