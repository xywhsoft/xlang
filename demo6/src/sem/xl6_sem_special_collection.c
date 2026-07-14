/*
    XRT 专用数据结构的语义入口。

    这些类型保留静态泛型信息，但底层始终是带引用计数的 XRT 句柄。
    编译器只负责类型检查与方法分派，不复制栈、树或队列算法。
*/

static int xl6SemIsSpecialQueueTypeName(const char* name)
{
    return xl6TextEquals(name, "SpscQueue") ||
           xl6TextEquals(name, "MpscQueue") ||
           xl6TextEquals(name, "MpmcQueue") ||
           xl6TextEquals(name, "WaitQueue");
}

static int xl6SemIsSpecialCollectionTypeName(const char* name)
{
    return xl6TextEquals(name, "Stack") ||
           xl6TextEquals(name, "AvlTree") ||
           xl6SemIsSpecialQueueTypeName(name);
}

static int xl6SemIsSpecialCollectionGenericInstant(const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArgs;
    size_t expected;

    if ( node == NULL || node->kind != XL6_AST_GENERIC_INSTANT || node->childCount < 2 ) {
        return 0;
    }
    nameNode = node->children[0];
    typeArgs = node->children[1];
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME ||
         !xl6SemIsSpecialCollectionTypeName(nameNode->text) || typeArgs == NULL ) {
        return 0;
    }
    expected = xl6TextEquals(nameNode->text, "AvlTree") ? 2u : 1u;
    return typeArgs->childCount == expected;
}

static XL6_AstNode* xl6SemCloneSpecialCollectionReceiverType(
    XL6_SemContext* ctx,
    XL6_AstNode* member)
{
    XL6_AstNode* receiver;
    XL6_Symbol* sym;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->childCount == 0 ) {
        return NULL;
    }
    receiver = member->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return xl6SemInferAutoTypeNode(ctx, receiver);
    }
    sym = xl6ScopeFind(ctx->scope, receiver->text);
    return sym != NULL && sym->type != NULL ? xl6SemCloneTypeNode(sym->type) : NULL;
}

static const XL6_AstNode* xl6SemSpecialCollectionTypeArg(
    XL6_SemContext* ctx,
    const XL6_AstNode* typeNode,
    size_t index)
{
    const XL6_AstNode* concreteType = xl6SemResolveAliasType(ctx, typeNode);
    const XL6_AstNode* args = xl6SemTypeNodeGenericArgs(concreteType);

    return args != NULL && index < args->childCount ? args->children[index] : NULL;
}

static XL6_AstNode* xl6SemSpecialCollectionArgExpr(XL6_AstNode* args, size_t index)
{
    XL6_AstNode* arg;

    if ( args == NULL || index >= args->childCount ) {
        return NULL;
    }
    arg = args->children[index];
    return arg != NULL && arg->kind == XL6_AST_ARG && arg->childCount > 0 ? arg->children[0] : NULL;
}

static int xl6SemCheckSpecialCollectionValueArg(
    XL6_SemContext* ctx,
    XL6_AstNode* expr,
    const XL6_AstNode* targetType,
    const char* message)
{
    if ( expr != NULL && targetType != NULL && xl6SemExprCanMatchParamType(ctx, expr, targetType) ) {
        return 1;
    }
    xl6SemTypeMismatchError(
        ctx,
        expr != NULL ? expr : (XL6_AstNode*)targetType,
        message,
        targetType,
        xl6SemExprStaticTypeName(ctx, expr));
    return 0;
}

static int xl6SemCheckSpecialCollectionOutArg(
    XL6_SemContext* ctx,
    XL6_AstNode* expr,
    const XL6_AstNode* targetType,
    const char* message)
{
    XL6_Symbol* sym;

    if ( expr == NULL || expr->kind != XL6_AST_NAME ) {
        xl6SemErrorNode(ctx, expr != NULL ? expr : targetType, message);
        return 0;
    }
    sym = xl6ScopeFind(ctx->scope, expr->text);
    if ( sym == NULL || sym->type == NULL ) {
        xl6SemErrorNode(ctx, expr, "输出目标变量未声明");
        return 0;
    }
    if ( xl6SemSymbolIsConst(sym) ) {
        xl6SemErrorNode(ctx, expr, "const 不能作为输出目标");
        return 0;
    }
    if ( !xl6SemCheckBoundarySymbolUse(ctx, expr, sym, 1) ) {
        return 0;
    }
    if ( targetType == NULL || !xl6SemTypeEquals(ctx, sym->type, targetType) ) {
        xl6SemTypeMismatchError(ctx, expr, message, targetType, xl6SemDisplayTypeName(ctx, sym->type));
        return 0;
    }
    return 1;
}

static int xl6SemCheckSpecialCollectionCtorCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    const XL6_AstNode* callee = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    const XL6_AstNode* nameNode = callee != NULL && callee->childCount > 0 ? callee->children[0] : NULL;
    const XL6_AstNode* typeArgs = callee != NULL && callee->childCount > 1 ? callee->children[1] : NULL;
    XL6_AstNode* args = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    size_t i;
    size_t maxArgs;
    int ok = 1;

    if ( !xl6SemIsSpecialCollectionGenericInstant(callee) ) {
        return -1;
    }
    for ( i = 0; typeArgs != NULL && i < typeArgs->childCount; ++i ) {
        if ( !xl6SemCheckType(ctx, typeArgs->children[i]) ) {
            ok = 0;
        }
    }
    if ( nameNode != NULL && xl6TextEquals(nameNode->text, "AvlTree") &&
         typeArgs != NULL && typeArgs->childCount > 0 &&
         !xl6SemTypeHasComparableProtocol(ctx, typeArgs->children[0]) ) {
        xl6SemErrorNode(ctx, typeArgs->children[0], "AvlTree<K, V> 的键类型必须满足 comparable");
        ok = 0;
    }

    maxArgs = nameNode != NULL && xl6SemIsSpecialQueueTypeName(nameNode->text) ? 1u : 0u;
    if ( args != NULL && args->childCount > maxArgs ) {
        xl6SemErrorNode(ctx, args, maxArgs == 0 ? "该构造函数不接收参数" : "队列构造函数最多接收一个容量参数");
        ok = 0;
    }
    for ( i = 0; args != NULL && i < args->childCount; ++i ) {
        XL6_AstNode* arg = args->children[i];
        XL6_AstNode* expr = xl6SemSpecialCollectionArgExpr(args, i);
        XL6_AstNode intType;

        if ( arg == NULL || arg->kind != XL6_AST_ARG ) {
            xl6SemErrorNode(ctx, arg != NULL ? arg : args, "构造函数不接受命名参数");
            ok = 0;
            continue;
        }
        memset(&intType, 0, sizeof(intType));
        intType.kind = XL6_AST_TYPE;
        intType.text = "int";
        if ( expr == NULL || !xl6SemExprCanMatchParamType(ctx, expr, &intType) ) {
            xl6SemErrorNode(ctx, expr != NULL ? expr : arg, "队列容量必须是整数");
            ok = 0;
        }
    }
    return ok;
}

static int xl6SemCheckSpecialCollectionMethodCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* member;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const XL6_AstNode* keyType;
    XL6_AstNode* args;
    size_t count;
    const char* typeName;
    const char* method;
    int ok = 1;

    if ( node == NULL || node->childCount == 0 || node->children[0] == NULL ||
         node->children[0]->kind != XL6_AST_MEMBER ) {
        return -1;
    }
    member = node->children[0];
    receiverType = xl6SemCloneSpecialCollectionReceiverType(ctx, member);
    concreteType = xl6SemResolveAliasType(ctx, receiverType);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( !xl6SemIsSpecialCollectionTypeName(typeName) ) {
        xl6AstFree(receiverType);
        return -1;
    }
    if ( member->children[0] == NULL ||
         (member->children[0]->kind != XL6_AST_NAME && member->children[0]->kind != XL6_AST_MEMBER) ) {
        xl6SemErrorNode(ctx, member->children[0],
                        "[SC-TEMP-RECEIVER] 专用数据结构方法必须通过具名变量或字段调用");
        xl6AstFree(receiverType);
        return 0;
    }
    method = member->text;
    args = node->childCount > 1 ? node->children[1] : NULL;
    count = args != NULL ? args->childCount : 0u;
    itemType = xl6SemSpecialCollectionTypeArg(ctx, receiverType, xl6TextEquals(typeName, "AvlTree") ? 1u : 0u);
    keyType = xl6TextEquals(typeName, "AvlTree") ? xl6SemSpecialCollectionTypeArg(ctx, receiverType, 0u) : NULL;

    if ( xl6TextEquals(typeName, "Stack") ) {
        if ( xl6TextEquals(method, "push") ) {
            ok = count == 1 && xl6SemCheckSpecialCollectionValueArg(
                ctx, xl6SemSpecialCollectionArgExpr(args, 0), itemType, "Stack.push 参数类型必须匹配 Stack<T>");
        } else if ( xl6TextEquals(method, "popInto") || xl6TextEquals(method, "peekInto") ) {
            ok = count == 1 && xl6SemCheckSpecialCollectionOutArg(
                ctx, xl6SemSpecialCollectionArgExpr(args, 0), itemType, "Stack 输出目标必须是同类型具名变量");
        } else if ( xl6TextEquals(method, "clear") || xl6TextEquals(method, "count") ) {
            ok = count == 0;
        } else {
            xl6SemErrorNode(ctx, member, "unknown Stack method");
            ok = 0;
        }
    } else if ( xl6TextEquals(typeName, "AvlTree") ) {
        if ( xl6TextEquals(method, "set") ) {
            ok = count == 2 &&
                 xl6SemCheckSpecialCollectionValueArg(ctx, xl6SemSpecialCollectionArgExpr(args, 0), keyType, "AvlTree.set 键类型不匹配") &&
                 xl6SemCheckSpecialCollectionValueArg(ctx, xl6SemSpecialCollectionArgExpr(args, 1), itemType, "AvlTree.set 值类型不匹配");
        } else if ( xl6TextEquals(method, "getInto") ) {
            ok = count == 2 &&
                 xl6SemCheckSpecialCollectionValueArg(ctx, xl6SemSpecialCollectionArgExpr(args, 0), keyType, "AvlTree.getInto 键类型不匹配") &&
                 xl6SemCheckSpecialCollectionOutArg(ctx, xl6SemSpecialCollectionArgExpr(args, 1), itemType, "AvlTree.getInto 输出目标类型不匹配");
        } else if ( xl6TextEquals(method, "contains") || xl6TextEquals(method, "remove") ) {
            ok = count == 1 && xl6SemCheckSpecialCollectionValueArg(
                ctx, xl6SemSpecialCollectionArgExpr(args, 0), keyType, "AvlTree 键类型不匹配");
        } else if ( xl6TextEquals(method, "clear") || xl6TextEquals(method, "count") ||
                    xl6TextEquals(method, "keys") || xl6TextEquals(method, "values") ) {
            ok = count == 0;
        } else {
            xl6SemErrorNode(ctx, member, "unknown AvlTree method");
            ok = 0;
        }
    } else {
        if ( xl6TextEquals(method, "tryPush") ) {
            ok = count == 1 && xl6SemCheckSpecialCollectionValueArg(
                ctx, xl6SemSpecialCollectionArgExpr(args, 0), itemType, "队列元素类型不匹配");
        } else if ( xl6TextEquals(method, "tryPopInto") ||
                    (xl6TextEquals(typeName, "WaitQueue") && xl6TextEquals(method, "popInto")) ) {
            ok = count == 1 && xl6SemCheckSpecialCollectionOutArg(
                ctx, xl6SemSpecialCollectionArgExpr(args, 0), itemType, "队列输出目标类型不匹配");
        } else if ( xl6TextEquals(typeName, "WaitQueue") && xl6TextEquals(method, "popTimeoutInto") ) {
            XL6_AstNode intType;
            memset(&intType, 0, sizeof(intType));
            intType.kind = XL6_AST_TYPE;
            intType.text = "int";
            ok = count == 2 &&
                 xl6SemCheckSpecialCollectionOutArg(ctx, xl6SemSpecialCollectionArgExpr(args, 0), itemType, "队列输出目标类型不匹配") &&
                 xl6SemCheckSpecialCollectionValueArg(ctx, xl6SemSpecialCollectionArgExpr(args, 1), &intType, "等待超时必须是整数毫秒");
        } else if ( xl6TextEquals(method, "close") || xl6TextEquals(method, "count") ||
                    xl6TextEquals(method, "isClosed") || xl6TextEquals(method, "isDrained") ) {
            ok = count == 0;
        } else {
            xl6SemErrorNode(ctx, member, "unknown queue method");
            ok = 0;
        }
    }
    if ( !ok && count != 0 && args == NULL ) {
        xl6SemErrorNode(ctx, member, "专用数据结构方法参数不匹配");
    } else if ( !ok && count == 0 &&
                (xl6TextEquals(method, "clear") || xl6TextEquals(method, "count") ||
                 xl6TextEquals(method, "keys") || xl6TextEquals(method, "values") ||
                 xl6TextEquals(method, "close") || xl6TextEquals(method, "isClosed") ||
                 xl6TextEquals(method, "isDrained")) ) {
        xl6SemErrorNode(ctx, member, "该方法不接收参数");
    }
    xl6AstFree(receiverType);
    return ok;
}

static int xl6SemCheckSpecialCollectionCall(XL6_SemContext* ctx, XL6_AstNode* node)
{
    int result = xl6SemCheckSpecialCollectionCtorCall(ctx, node);
    return result >= 0 ? result : xl6SemCheckSpecialCollectionMethodCall(ctx, node);
}

static const char* xl6SemSpecialCollectionCallResultTypeName(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* member;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    const char* method;
    const char* result = NULL;

    if ( node == NULL || node->childCount == 0 ) {
        return NULL;
    }
    if ( xl6SemIsSpecialCollectionGenericInstant(node->children[0]) ) {
        const XL6_AstNode* nameNode = node->children[0]->children[0];
        return nameNode != NULL ? nameNode->text : NULL;
    }
    member = node->children[0];
    if ( member == NULL || member->kind != XL6_AST_MEMBER ) {
        return NULL;
    }
    receiverType = xl6SemCloneSpecialCollectionReceiverType(ctx, member);
    concreteType = xl6SemResolveAliasType(ctx, receiverType);
    if ( concreteType == NULL || !xl6SemIsSpecialCollectionTypeName(concreteType->text) ) {
        xl6AstFree(receiverType);
        return NULL;
    }
    method = member->text;
    if ( xl6TextEquals(method, "push") || xl6TextEquals(method, "popInto") ||
         xl6TextEquals(method, "peekInto") || xl6TextEquals(method, "set") ||
         xl6TextEquals(method, "getInto") || xl6TextEquals(method, "contains") ||
         xl6TextEquals(method, "remove") || xl6TextEquals(method, "tryPush") ||
         xl6TextEquals(method, "tryPopInto") || xl6TextEquals(method, "popInto") ||
         xl6TextEquals(method, "popTimeoutInto") || xl6TextEquals(method, "isClosed") ||
         xl6TextEquals(method, "isDrained") ) {
        result = "bool";
    } else if ( xl6TextEquals(method, "count") ) {
        result = "int";
    }
    xl6AstFree(receiverType);
    return result;
}

static XL6_AstNode* xl6SemCloneSpecialCollectionCallType(XL6_SemContext* ctx, XL6_AstNode* node)
{
    XL6_AstNode* member;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* scalarType;

    scalarType = xl6SemSpecialCollectionCallResultTypeName(ctx, node);
    if ( scalarType != NULL ) {
        return xl6AstNewText(XL6_AST_TYPE, scalarType, strlen(scalarType), NULL);
    }
    if ( node == NULL || node->childCount == 0 || node->children[0] == NULL ||
         node->children[0]->kind != XL6_AST_MEMBER ) {
        return NULL;
    }
    member = node->children[0];
    if ( !xl6TextEquals(member->text, "keys") && !xl6TextEquals(member->text, "values") ) {
        return NULL;
    }
    receiverType = xl6SemCloneSpecialCollectionReceiverType(ctx, member);
    concreteType = xl6SemResolveAliasType(ctx, receiverType);
    if ( concreteType == NULL || !xl6TextEquals(concreteType->text, "AvlTree") ) {
        xl6AstFree(receiverType);
        return NULL;
    }
    itemType = xl6SemSpecialCollectionTypeArg(
        ctx,
        receiverType,
        xl6TextEquals(member->text, "keys") ? 0u : 1u);
    {
        XL6_AstNode* result = xl6SemNewGenericContainerType("array", xl6SemCloneTypeNode(itemType), member);
        xl6AstFree(receiverType);
        return result;
    }
}
