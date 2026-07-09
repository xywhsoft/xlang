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
    xl6CgenCIdentText(cName, sizeof(cName), name);
    ctx->cleanups[ctx->cleanupCount].name = xrtCopyStr(cName, strlen(cName));
    ctx->cleanups[ctx->cleanupCount].kind = kind;
    ctx->cleanups[ctx->cleanupCount].typeNode = typeNode;
    ctx->cleanups[ctx->cleanupCount].classTypeNode = classTypeNode;
    ctx->cleanupCount += 1;
}

static void xl6CgenAddNativeClassCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* customNode)
{
    if ( customNode == NULL || !xl6CgenNativeBackedClassHasDestroy(customNode) ) {
        return;
    }
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_NATIVE_CLASS, customNode, NULL);
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
    if ( ctx == NULL ) {
        return;
    }
    xl6CgenFreeCleanupsSince(ctx, 0);
    xrtFree(ctx->cleanups);
    ctx->cleanups = NULL;
    ctx->cleanupCapacity = 0;
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
        xl6OutputPrintf(ctx->out, "xrtChannelDestroy(%s);\n", cleanup->name);
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
            xl6OutputPrintf(ctx->out, "xrtTypedArrayDestroy(%s);\n", cleanup->name);
        } else if ( xl6CTextEquals(typeName, "list") ) {
            xl6OutputPrintf(ctx->out, "xrtTypedListDestroy(%s);\n", cleanup->name);
        } else if ( xl6CTextEquals(typeName, "set") ) {
            xl6OutputPrintf(ctx->out, "xrtTypedSetDestroy(%s);\n", cleanup->name);
        } else if ( xl6CTextEquals(typeName, "dict") ) {
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

    xl6CgenCIdentText(cName, sizeof(cName), name);
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

    xl6CgenCIdentText(cName, sizeof(cName), name);
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

    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }
    if ( expr->kind == XL6_AST_MEMBER || expr->kind == XL6_AST_SAFE_MEMBER ) {
        const XL6_AstNode* receiver;
        XL6_AstNode* receiverType;
        XL6_AstNode* fieldType;
        int result;

        if ( expr->text != NULL && xl6CTextEquals(expr->text, "length") ) {
            return 0;
        }
        receiver = expr->childCount > 0 ? expr->children[0] : NULL;
        receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
        if ( xl6CgenIsXValueType(receiverType) ) {
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
        const XL6_AstNode* index = expr->childCount > 1 ? expr->children[1] : NULL;
        XL6_AstNode* receiverType;
        if ( xl6CgenExprUsesTypedContainerStorage(ctx, receiver) ) {
            return 0;
        }
        receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
        if ( xl6CgenIsXValueType(receiverType) && xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            xl6AstFree(receiverType);
            return 0;
        }
        xl6AstFree(receiverType);
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
    if ( xl6CgenExprUsesTypedContainerStorage(ctx, expr) ) {
        return 0;
    }
    typeNode = xl6CgenFindVarType(ctx, expr->text);
    return xl6CgenIsXValueType(typeNode);
}

static void xl6CgenEmitCleanupsSince(XL6_CgenContext* ctx, size_t mark)
{
    size_t i;

    if ( ctx == NULL || mark > ctx->cleanupCount ) {
        return;
    }
    for ( i = ctx->cleanupCount; i > mark; --i ) {
        xl6CgenEmitOneCleanup(ctx, &ctx->cleanups[i - 1]);
    }
}

static void xl6CgenCloseScope(XL6_CgenContext* ctx, size_t mark)
{
    xl6CgenEmitCleanupsSince(ctx, mark);
    xl6CgenFreeCleanupsSince(ctx, mark);
}

