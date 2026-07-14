/*
    XRT 专用数据结构的 C 生成适配。

    这里只生成类型描述、句柄生命周期和方法调用。所有数据结构算法与
    元素所有权操作都由 XRT 实现，生成代码不保存第二份容器状态。
*/

static int xl6CgenIsSpecialQueueTypeName(const char* name)
{
    return xl6CTextEquals(name, "SpscQueue") ||
           xl6CTextEquals(name, "MpscQueue") ||
           xl6CTextEquals(name, "MpmcQueue") ||
           xl6CTextEquals(name, "WaitQueue");
}

static int xl6CgenIsSpecialCollectionTypeName(const char* name)
{
    return xl6CTextEquals(name, "Stack") ||
           xl6CTextEquals(name, "AvlTree") ||
           xl6CgenIsSpecialQueueTypeName(name);
}

static int xl6CgenIsSpecialCollectionType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    return concreteType != NULL && xl6CgenIsSpecialCollectionTypeName(concreteType->text);
}

static const XL6_AstNode* xl6CgenSpecialCollectionTypeArgs(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    return concreteType != NULL && concreteType->childCount > 0 &&
           concreteType->children[0] != NULL && concreteType->children[0]->kind == XL6_AST_LIST
        ? concreteType->children[0]
        : NULL;
}

static int xl6CgenIsSpecialCollectionGenericInstant(const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* args;
    size_t expected;

    if ( node == NULL || node->kind != XL6_AST_GENERIC_INSTANT || node->childCount < 2 ) {
        return 0;
    }
    nameNode = node->children[0];
    args = node->children[1];
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME ||
         !xl6CgenIsSpecialCollectionTypeName(nameNode->text) || args == NULL ) {
        return 0;
    }
    expected = xl6CTextEquals(nameNode->text, "AvlTree") ? 2u : 1u;
    return args->childCount == expected;
}

static XL6_AstNode* xl6CgenSpecialCollectionCtorType(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* instant;
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArgs;
    XL6_AstNode* typeNode;
    XL6_AstNode* argsCopy;

    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return NULL;
    }
    instant = call->children[0];
    if ( !xl6CgenIsSpecialCollectionGenericInstant(instant) ) {
        return NULL;
    }
    nameNode = instant->children[0];
    typeArgs = instant->children[1];
    typeNode = xl6CgenNewSimpleType(nameNode->text, nameNode);
    argsCopy = xl6CgenCloneCanonicalTypeNode(ctx, typeArgs);
    if ( typeNode == NULL || argsCopy == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(argsCopy);
        xl6CgenError(ctx, call, "专用数据结构构造类型推断内存不足");
        return NULL;
    }
    xl6AstAddChild(typeNode, argsCopy);
    return typeNode;
}

static XL6_AstNode* xl6CgenSpecialCollectionMethodType(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* member;
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* typeArgs;
    const XL6_AstNode* itemType;
    XL6_AstNode* result;

    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return NULL;
    }
    member = call->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->childCount == 0 ) {
        return NULL;
    }
    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    concreteType = xl6CgenResolveAliasType(ctx, receiverType);
    if ( concreteType == NULL || !xl6CgenIsSpecialCollectionTypeName(concreteType->text) ) {
        xl6AstFree(receiverType);
        return NULL;
    }
    result = NULL;
    if ( xl6CTextEquals(member->text, "push") || xl6CTextEquals(member->text, "popInto") ||
         xl6CTextEquals(member->text, "peekInto") || xl6CTextEquals(member->text, "set") ||
         xl6CTextEquals(member->text, "getInto") || xl6CTextEquals(member->text, "contains") ||
         xl6CTextEquals(member->text, "remove") || xl6CTextEquals(member->text, "tryPush") ||
         xl6CTextEquals(member->text, "tryPopInto") || xl6CTextEquals(member->text, "popInto") ||
         xl6CTextEquals(member->text, "popTimeoutInto") || xl6CTextEquals(member->text, "isClosed") ||
         xl6CTextEquals(member->text, "isDrained") ) {
        result = xl6CgenNewSimpleType("bool", member);
    } else if ( xl6CTextEquals(member->text, "count") ) {
        result = xl6CgenNewSimpleType("int", member);
    } else if ( xl6CTextEquals(concreteType->text, "AvlTree") &&
                (xl6CTextEquals(member->text, "keys") || xl6CTextEquals(member->text, "values")) ) {
        XL6_AstNode* resultArgs;
        XL6_AstNode* itemCopy;

        typeArgs = xl6CgenSpecialCollectionTypeArgs(ctx, receiverType);
        itemType = typeArgs != NULL && typeArgs->childCount == 2
            ? typeArgs->children[xl6CTextEquals(member->text, "keys") ? 0 : 1]
            : NULL;
        result = xl6CgenNewSimpleType("array", member);
        resultArgs = xl6AstNew(XL6_AST_LIST, NULL);
        itemCopy = xl6CgenCloneCanonicalTypeNode(ctx, itemType);
        if ( result == NULL || resultArgs == NULL || itemCopy == NULL ) {
            xl6AstFree(result);
            xl6AstFree(resultArgs);
            xl6AstFree(itemCopy);
            result = NULL;
        } else {
            xl6AstAddChild(resultArgs, itemCopy);
            xl6AstAddChild(result, resultArgs);
        }
    }
    xl6AstFree(receiverType);
    return result;
}

static const XL6_AstNode* xl6CgenSpecialCollectionCallArg(const XL6_AstNode* call, size_t index)
{
    const XL6_AstNode* args;
    const XL6_AstNode* arg;

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    if ( args == NULL || index >= args->childCount ) {
        return NULL;
    }
    arg = args->children[index];
    return arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
}

static void xl6CgenSpecialCollectionValueAddress(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* expr)
{
    xl6OutputPuts("&(", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, typeNode), ctx->out);
    xl6OutputPuts("){", ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPuts("}", ctx->out);
}

static void xl6CgenSpecialCollectionOutAddress(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    xl6OutputPuts("&(", ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenSpecialCollectionRetainExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));

    if ( concreteType != NULL && xl6CTextEquals(concreteType->text, "Stack") ) {
        xl6OutputPuts("xrtTypedStackRetain(", ctx->out);
    } else if ( concreteType != NULL && xl6CTextEquals(concreteType->text, "AvlTree") ) {
        xl6OutputPuts("xrtTypedAvlTreeRetain(", ctx->out);
    } else {
        xl6OutputPuts("xrtTypedQueueRetain(", ctx->out);
    }
    xl6CgenExpr(ctx, expr);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenSpecialCollectionRetainCExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* expr)
{
    const XL6_AstNode* concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));

    if ( concreteType != NULL && xl6CTextEquals(concreteType->text, "Stack") ) {
        xl6OutputPrintf(ctx->out, "xrtTypedStackRetain(%s)", expr != NULL ? expr : "0");
    } else if ( concreteType != NULL && xl6CTextEquals(concreteType->text, "AvlTree") ) {
        xl6OutputPrintf(ctx->out, "xrtTypedAvlTreeRetain(%s)", expr != NULL ? expr : "0");
    } else {
        xl6OutputPrintf(ctx->out, "xrtTypedQueueRetain(%s)", expr != NULL ? expr : "0");
    }
}

static void xl6CgenSpecialCollectionReleaseCExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* expr)
{
    const XL6_AstNode* concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));

    if ( concreteType != NULL && xl6CTextEquals(concreteType->text, "Stack") ) {
        xl6OutputPrintf(ctx->out, "xrtTypedStackRelease(%s)", expr != NULL ? expr : "0");
    } else if ( concreteType != NULL && xl6CTextEquals(concreteType->text, "AvlTree") ) {
        xl6OutputPrintf(ctx->out, "xrtTypedAvlTreeRelease(%s)", expr != NULL ? expr : "0");
    } else {
        xl6OutputPrintf(ctx->out, "xrtTypedQueueRelease(%s)", expr != NULL ? expr : "0");
    }
}

static int xl6CgenSpecialCollectionExprIsOwned(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    int owned;

    if ( expr == NULL ) {
        return 0;
    }
    /* await 对具名 future 执行 retain，对临时 future 转移句柄；两条路径都返回新拥有引用。 */
    if ( expr->kind == XL6_AST_AWAIT ) {
        return 1;
    }
    if ( expr->kind != XL6_AST_CALL ) {
        return 0;
    }
    if ( expr->childCount > 0 && xl6CgenIsSpecialCollectionGenericInstant(expr->children[0]) ) {
        return 1;
    }
    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    owned = xl6CgenIsSpecialCollectionType(ctx, typeNode);
    xl6AstFree(typeNode);
    return owned;
}

static int xl6CgenSpecialCollectionCtorCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call,
    const XL6_AstNode* callee)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArgs;
    const XL6_AstNode* capacity;

    if ( !xl6CgenIsSpecialCollectionGenericInstant(callee) ) {
        return 0;
    }
    nameNode = callee->children[0];
    typeArgs = callee->children[1];
    capacity = xl6CgenSpecialCollectionCallArg(call, 0);
    xl6OutputPuts("xl6_special_collection_require(", ctx->out);
    if ( xl6CTextEquals(nameNode->text, "Stack") ) {
        xl6OutputPuts("xrtTypedStackCreate(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, typeArgs->children[0]);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(nameNode->text, "AvlTree") ) {
        xl6OutputPuts("xrtTypedAvlTreeCreate(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, typeArgs->children[0]);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, typeArgs->children[1]);
        xl6OutputPuts(", 1u)", ctx->out);
    } else {
        int kind = xl6CTextEquals(nameNode->text, "SpscQueue") ? 1 :
                   (xl6CTextEquals(nameNode->text, "MpscQueue") ? 2 :
                    (xl6CTextEquals(nameNode->text, "MpmcQueue") ? 3 : 4));
        xl6OutputPrintf(ctx->out, "xrtTypedQueueCreate(%dU, ", kind);
        xl6CgenEmitXrtTypeDescRef(ctx, typeArgs->children[0]);
        xl6OutputPuts(", ", ctx->out);
        if ( capacity != NULL ) {
            xl6CgenExpr(ctx, capacity);
        } else {
            xl6OutputPuts("64", ctx->out);
        }
        xl6OutputPutc(')', ctx->out);
    }
    xl6OutputPrintf(ctx->out, ", %d, %d)", callee->line, callee->column);
    return 1;
}

static void xl6CgenSpecialCollectionDefaultCreateExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    const XL6_AstNode* typeArgs = xl6CgenSpecialCollectionTypeArgs(ctx, typeNode);
    const char* name = concreteType != NULL ? concreteType->text : NULL;

    xl6OutputPuts("xl6_special_collection_require(", ctx->out);
    if ( xl6CTextEquals(name, "Stack") ) {
        xl6OutputPuts("xrtTypedStackCreate(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, typeArgs != NULL && typeArgs->childCount > 0 ? typeArgs->children[0] : NULL);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(name, "AvlTree") ) {
        xl6OutputPuts("xrtTypedAvlTreeCreate(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, typeArgs != NULL && typeArgs->childCount > 0 ? typeArgs->children[0] : NULL);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, typeArgs != NULL && typeArgs->childCount > 1 ? typeArgs->children[1] : NULL);
        xl6OutputPuts(", 1u)", ctx->out);
    } else {
        int kind = xl6CTextEquals(name, "SpscQueue") ? 1 :
                   (xl6CTextEquals(name, "MpscQueue") ? 2 :
                    (xl6CTextEquals(name, "MpmcQueue") ? 3 : 4));
        xl6OutputPrintf(ctx->out, "xrtTypedQueueCreate(%dU, ", kind);
        xl6CgenEmitXrtTypeDescRef(ctx, typeArgs != NULL && typeArgs->childCount > 0 ? typeArgs->children[0] : NULL);
        xl6OutputPuts(", 64u)", ctx->out);
    }
    xl6OutputPrintf(ctx->out, ", %d, %d)", typeNode != NULL ? typeNode->line : 0, typeNode != NULL ? typeNode->column : 0);
}

static int xl6CgenSpecialCollectionMethodCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call,
    const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* typeArgs;
    const XL6_AstNode* itemType;
    const XL6_AstNode* keyType;
    const XL6_AstNode* arg0;
    const XL6_AstNode* arg1;
    const char* typeName;
    const char* method;

    if ( call == NULL || member == NULL || member->kind != XL6_AST_MEMBER || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    concreteType = xl6CgenResolveAliasType(ctx, receiverType);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( !xl6CgenIsSpecialCollectionTypeName(typeName) ) {
        xl6AstFree(receiverType);
        return 0;
    }
    typeArgs = xl6CgenSpecialCollectionTypeArgs(ctx, receiverType);
    itemType = typeArgs != NULL && typeArgs->childCount > 0
        ? typeArgs->children[xl6CTextEquals(typeName, "AvlTree") ? 1 : 0]
        : NULL;
    keyType = typeArgs != NULL && typeArgs->childCount > 0 ? typeArgs->children[0] : NULL;
    arg0 = xl6CgenSpecialCollectionCallArg(call, 0);
    arg1 = xl6CgenSpecialCollectionCallArg(call, 1);
    method = member->text;

#define XL6_SPECIAL_RECEIVER() xl6CgenExpr(ctx, receiver)
    if ( xl6CTextEquals(typeName, "Stack") ) {
        if ( xl6CTextEquals(method, "push") ) {
            xl6OutputPuts("xrtTypedStackPush(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts(", ", ctx->out);
            xl6CgenSpecialCollectionValueAddress(ctx, itemType, arg0);
            xl6OutputPutc(')', ctx->out);
        } else if ( xl6CTextEquals(method, "popInto") || xl6CTextEquals(method, "peekInto") ) {
            xl6OutputPuts(xl6CTextEquals(method, "popInto") ? "xrtTypedStackPop(" : "xrtTypedStackPeek(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts(", ", ctx->out);
            xl6CgenSpecialCollectionOutAddress(ctx, arg0);
            xl6OutputPutc(')', ctx->out);
        } else if ( xl6CTextEquals(method, "clear") ) {
            xl6OutputPuts("(xrtTypedStackClear(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts("), 0)", ctx->out);
        } else if ( xl6CTextEquals(method, "count") ) {
            xl6OutputPuts("((long long)xrtTypedStackCount(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts("))", ctx->out);
        } else {
            xl6AstFree(receiverType);
            return 0;
        }
    } else if ( xl6CTextEquals(typeName, "AvlTree") ) {
        if ( xl6CTextEquals(method, "set") ) {
            xl6OutputPuts("xrtTypedAvlTreeSet(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts(", ", ctx->out);
            xl6CgenSpecialCollectionValueAddress(ctx, keyType, arg0);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenSpecialCollectionValueAddress(ctx, itemType, arg1);
            xl6OutputPutc(')', ctx->out);
        } else if ( xl6CTextEquals(method, "getInto") ) {
            xl6OutputPuts("xrtTypedAvlTreeGet(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts(", ", ctx->out);
            xl6CgenSpecialCollectionValueAddress(ctx, keyType, arg0);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenSpecialCollectionOutAddress(ctx, arg1);
            xl6OutputPutc(')', ctx->out);
        } else if ( xl6CTextEquals(method, "contains") || xl6CTextEquals(method, "remove") ) {
            xl6OutputPuts(xl6CTextEquals(method, "contains") ? "xrtTypedAvlTreeContains(" : "xrtTypedAvlTreeRemove(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts(", ", ctx->out);
            xl6CgenSpecialCollectionValueAddress(ctx, keyType, arg0);
            xl6OutputPutc(')', ctx->out);
        } else if ( xl6CTextEquals(method, "clear") ) {
            xl6OutputPuts("(xrtTypedAvlTreeClear(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts("), 0)", ctx->out);
        } else if ( xl6CTextEquals(method, "count") ) {
            xl6OutputPuts("((long long)xrtTypedAvlTreeCount(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts("))", ctx->out);
        } else if ( xl6CTextEquals(method, "keys") || xl6CTextEquals(method, "values") ) {
            xl6OutputPuts(xl6CTextEquals(method, "keys") ? "xrtTypedAvlTreeKeys(" : "xrtTypedAvlTreeValues(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts(", 0u)", ctx->out);
        } else {
            xl6AstFree(receiverType);
            return 0;
        }
    } else {
        if ( xl6CTextEquals(method, "tryPush") ) {
            xl6OutputPuts("(xrtTypedQueueTryPush(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts(", ", ctx->out);
            xl6CgenSpecialCollectionValueAddress(ctx, itemType, arg0);
            xl6OutputPuts(") == 0)", ctx->out);
        } else if ( xl6CTextEquals(method, "tryPopInto") || xl6CTextEquals(method, "popInto") ) {
            xl6OutputPuts(xl6CTextEquals(method, "popInto") ? "(xrtTypedQueuePop(" : "(xrtTypedQueueTryPop(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts(", ", ctx->out);
            xl6CgenSpecialCollectionOutAddress(ctx, arg0);
            xl6OutputPuts(") == 0)", ctx->out);
        } else if ( xl6CTextEquals(method, "popTimeoutInto") ) {
            xl6OutputPuts("(xrtTypedQueuePopTimeout(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts(", ", ctx->out);
            xl6CgenSpecialCollectionOutAddress(ctx, arg0);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, arg1);
            xl6OutputPuts(") == 0)", ctx->out);
        } else if ( xl6CTextEquals(method, "close") ) {
            xl6OutputPuts("(xrtTypedQueueClose(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts("), 0)", ctx->out);
        } else if ( xl6CTextEquals(method, "count") ) {
            xl6OutputPuts("((long long)xrtTypedQueueApproxCount(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPuts("))", ctx->out);
        } else if ( xl6CTextEquals(method, "isClosed") || xl6CTextEquals(method, "isDrained") ) {
            xl6OutputPuts(xl6CTextEquals(method, "isClosed") ? "xrtTypedQueueIsClosed(" : "xrtTypedQueueIsDrained(", ctx->out);
            XL6_SPECIAL_RECEIVER();
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6AstFree(receiverType);
            return 0;
        }
    }
#undef XL6_SPECIAL_RECEIVER
    xl6AstFree(receiverType);
    return 1;
}

static void xl6CgenEmitSpecialCollectionRuntime(XL6_Output* out)
{
    xl6OutputPuts("/* XRT typed special collections. */\n", out);
    xl6OutputPuts("typedef struct xrt_typed_stack_struct* xtstack;\n", out);
    xl6OutputPuts("typedef struct xrt_typed_avl_tree_struct* xtavltree;\n", out);
    xl6OutputPuts("typedef struct xrt_typed_queue_struct* xtqueue;\n", out);
    xl6OutputPuts("xtstack xrtTypedStackCreate(const xrt_type_desc*);\n", out);
    xl6OutputPuts("xtstack xrtTypedStackRetain(xtstack);\n", out);
    xl6OutputPuts("void xrtTypedStackRelease(xtstack);\n", out);
    xl6OutputPuts("bool xrtTypedStackPush(xtstack, const void*);\n", out);
    xl6OutputPuts("bool xrtTypedStackPop(xtstack, void*);\n", out);
    xl6OutputPuts("bool xrtTypedStackPeek(xtstack, void*);\n", out);
    xl6OutputPuts("void xrtTypedStackClear(xtstack);\n", out);
    xl6OutputPuts("uint32_t xrtTypedStackCount(xtstack);\n", out);
    xl6OutputPuts("xtavltree xrtTypedAvlTreeCreate(const xrt_type_desc*, const xrt_type_desc*, uint32_t);\n", out);
    xl6OutputPuts("xtavltree xrtTypedAvlTreeRetain(xtavltree);\n", out);
    xl6OutputPuts("void xrtTypedAvlTreeRelease(xtavltree);\n", out);
    xl6OutputPuts("bool xrtTypedAvlTreeSet(xtavltree, const void*, const void*);\n", out);
    xl6OutputPuts("bool xrtTypedAvlTreeGet(xtavltree, const void*, void*);\n", out);
    xl6OutputPuts("bool xrtTypedAvlTreeContains(xtavltree, const void*);\n", out);
    xl6OutputPuts("bool xrtTypedAvlTreeRemove(xtavltree, const void*);\n", out);
    xl6OutputPuts("void xrtTypedAvlTreeClear(xtavltree);\n", out);
    xl6OutputPuts("uint32_t xrtTypedAvlTreeCount(xtavltree);\n", out);
    xl6OutputPuts("xtarray xrtTypedAvlTreeKeys(xtavltree, uint32_t);\n", out);
    xl6OutputPuts("xtarray xrtTypedAvlTreeValues(xtavltree, uint32_t);\n", out);
    xl6OutputPuts("xtqueue xrtTypedQueueCreate(uint32_t, const xrt_type_desc*, uint32_t);\n", out);
    xl6OutputPuts("xtqueue xrtTypedQueueRetain(xtqueue);\n", out);
    xl6OutputPuts("void xrtTypedQueueRelease(xtqueue);\n", out);
    xl6OutputPuts("int xrtTypedQueueTryPush(xtqueue, const void*);\n", out);
    xl6OutputPuts("int xrtTypedQueueTryPop(xtqueue, void*);\n", out);
    xl6OutputPuts("int xrtTypedQueuePop(xtqueue, void*);\n", out);
    xl6OutputPuts("int xrtTypedQueuePopTimeout(xtqueue, void*, uint32_t);\n", out);
    xl6OutputPuts("uint32_t xrtTypedQueueApproxCount(xtqueue);\n", out);
    xl6OutputPuts("void xrtTypedQueueClose(xtqueue);\n", out);
    xl6OutputPuts("bool xrtTypedQueueIsClosed(xtqueue);\n", out);
    xl6OutputPuts("bool xrtTypedQueueIsDrained(xtqueue);\n", out);
    xl6OutputPuts("static void* xl6_special_collection_require(void* handle, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    (void)line; (void)column;\n", out);
    xl6OutputPuts("    if ( handle == 0 ) { xl6_runtime_fatal(3); }\n", out);
    xl6OutputPuts("    return handle;\n", out);
    xl6OutputPuts("}\n\n", out);
}
