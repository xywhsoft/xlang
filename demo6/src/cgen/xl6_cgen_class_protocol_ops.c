static const char* xl6CgenUnaryProtocolName(const char* op)
{
    if ( xl6CTextEquals(op, "-") ) {
        return "__neg__";
    }
    if ( xl6CTextEquals(op, "+") ) {
        return "__pos__";
    }
    if ( xl6CTextEquals(op, "not") || xl6CTextEquals(op, "!") ) {
        return "__not__";
    }
    if ( xl6CTextEquals(op, "~") ) {
        return "__bitNot__";
    }
    return NULL;
}

static const char* xl6CgenBinaryProtocolName(const char* op)
{
    if ( xl6CTextEquals(op, "+") ) {
        return "__add__";
    }
    if ( xl6CTextEquals(op, "-") ) {
        return "__sub__";
    }
    if ( xl6CTextEquals(op, "*") ) {
        return "__mul__";
    }
    if ( xl6CTextEquals(op, "/") ) {
        return "__div__";
    }
    if ( xl6CTextEquals(op, "%") ) {
        return "__mod__";
    }
    if ( xl6CTextEquals(op, "**") ) {
        return "__pow__";
    }
    if ( xl6CTextEquals(op, "==") ) {
        return "__eq__";
    }
    if ( xl6CTextEquals(op, "!=") ) {
        return "__ne__";
    }
    if ( xl6CTextEquals(op, "<") ) {
        return "__lt__";
    }
    if ( xl6CTextEquals(op, "<=") ) {
        return "__le__";
    }
    if ( xl6CTextEquals(op, ">") ) {
        return "__gt__";
    }
    if ( xl6CTextEquals(op, ">=") ) {
        return "__ge__";
    }
    if ( xl6CTextEquals(op, "~=") ) {
        return "__approx__";
    }
    return NULL;
}

static int xl6CgenClassProtocolUnaryExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    XL6_AstNode* receiverType;
    XL6_AstNode* protocolCall;
    const char* protocolName;

    if ( node == NULL || node->kind != XL6_AST_UNARY || node->childCount == 0 ) {
        return 0;
    }
    receiver = node->children[0];
    receiverType = NULL;
    protocolName = xl6CgenUnaryProtocolName(node->text);
    protocolCall = protocolName != NULL ? xl6CgenProtocolTempCallNew(node) : NULL;
    if ( protocolCall == NULL ||
         !xl6CgenResolveClassProtocolReceiverForCall(ctx, receiver, protocolName, protocolCall, &classNode, &ownerClass, &method, &receiverType) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        return 0;
    }
    xl6CgenProtocolTempCallFree(protocolCall);

    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenClassProtocolBinaryAvailable(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* arg;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    XL6_AstNode* receiverType;
    XL6_AstNode* protocolCall;
    const char* protocolName;
    int result;

    if ( node == NULL || node->kind != XL6_AST_BINARY || node->childCount < 2 ) {
        return 0;
    }
    if ( xl6CTextEquals(node->text, "in") ) {
        receiver = node->children[1];
        arg = node->children[0];
        protocolName = "__contains__";
    } else {
        receiver = node->children[0];
        arg = node->children[1];
        protocolName = xl6CgenBinaryProtocolName(node->text);
    }
    if ( protocolName == NULL ) {
        return 0;
    }

    receiverType = NULL;
    protocolCall = xl6CgenProtocolTempCallNew(node);
    if ( protocolCall != NULL ) {
        xl6CgenProtocolTempCallAddArg(protocolCall, arg);
    }
    result = protocolCall != NULL &&
             xl6CgenResolveClassProtocolReceiverForCall(
                 ctx,
                 receiver,
                 protocolName,
                 protocolCall,
                 &classNode,
                 &ownerClass,
                 &method,
                 &receiverType);
    xl6AstFree(receiverType);
    xl6CgenProtocolTempCallFree(protocolCall);
    if ( result ) {
        return 1;
    }

    if ( xl6CTextEquals(protocolName, "__eq__") || xl6CTextEquals(protocolName, "__ne__") ) {
        XL6_AstNode* leftType = xl6CgenCloneExprTypeForInference(ctx, receiver);
        XL6_AstNode* rightType = xl6CgenCloneExprTypeForInference(ctx, arg);
        const XL6_AstNode* concreteType = xl6CgenResolveAliasType(ctx, leftType);
        XL6_AstNode* derivedClass;

        if ( xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
            concreteType = xl6CgenClassHandleTargetType(ctx, concreteType);
        }
        derivedClass = concreteType != NULL && concreteType->text != NULL ?
                       xl6CgenFindClass(ctx, concreteType->text) : NULL;
        result = derivedClass != NULL &&
                 !xl6CgenClassHasTypeParams(derivedClass) &&
                 xl6CgenClassNeedsAutoDeriveProtocol(ctx, derivedClass, "equals", protocolName) &&
                 xl6CgenTypeNodeEqual(leftType, rightType);
        xl6AstFree(leftType);
        xl6AstFree(rightType);
    }
    return result;
}

static int xl6CgenExprResolvesToClass(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* ownedType;
    const XL6_AstNode* concreteType;
    int result;

    ownedType = xl6CgenCloneExprTypeForInference(ctx, expr);
    concreteType = xl6CgenResolveAliasType(ctx, ownedType);
    if ( xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
        concreteType = xl6CgenClassHandleTargetType(ctx, concreteType);
    }
    result = concreteType != NULL &&
             concreteType->text != NULL &&
             xl6CgenFindClass(ctx, concreteType->text) != NULL;
    xl6AstFree(ownedType);
    return result;
}

static int xl6CgenBinaryOperatorIsArithmetic(const char* op)
{
    return xl6CTextEquals(op, "+") ||
           xl6CTextEquals(op, "-") ||
           xl6CTextEquals(op, "*") ||
           xl6CTextEquals(op, "/") ||
           xl6CTextEquals(op, "%") ||
           xl6CTextEquals(op, "**");
}

static int xl6CgenClassProtocolBinaryExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* arg;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* argType;
    XL6_AstNode* receiverType;
    XL6_AstNode* protocolCall;
    const char* protocolName;

    if ( node == NULL || node->kind != XL6_AST_BINARY || node->childCount < 2 ) {
        return 0;
    }

    if ( xl6CTextEquals(node->text, "in") ) {
        receiver = node->children[1];
        arg = node->children[0];
        protocolName = "__contains__";
    } else {
        receiver = node->children[0];
        arg = node->children[1];
        protocolName = xl6CgenBinaryProtocolName(node->text);
    }
    receiverType = NULL;
    protocolCall = protocolName != NULL ? xl6CgenProtocolTempCallNew(node) : NULL;
    if ( protocolCall != NULL ) {
        xl6CgenProtocolTempCallAddArg(protocolCall, arg);
    }
    if ( protocolCall == NULL ||
         !xl6CgenResolveClassProtocolReceiverForCall(ctx, receiver, protocolName, protocolCall, &classNode, &ownerClass, &method, &receiverType) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        if ( xl6CgenClassAutoEqualsBinaryExpr(ctx, receiver, arg, protocolName) ) {
            return 1;
        }

        /*
            class 运算只能由对应协议实现。这里必须截住 class handle，
            不能让它继续落入普通 C 运算符生成路径。
        */
        if ( xl6CgenBinaryOperatorIsArithmetic(node->text) &&
             (xl6CgenExprResolvesToClass(ctx, receiver) ||
              xl6CgenExprResolvesToClass(ctx, arg)) ) {
            xl6CgenError(ctx, node, "class operator requires matching protocol method");
            xl6OutputPuts("0", ctx->out);
            return 1;
        }
        return 0;
    }
    xl6CgenProtocolTempCallFree(protocolCall);

    argType = xl6CgenClassProtocolParamType(method, 0);
    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenClassProtocolExprArg(ctx, argType, arg);
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenClassProtocolCallExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* args;
    const XL6_AstNode* params;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    XL6_AstNode* receiverType;
    int hasParams;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    receiver = node->children[0];
    receiverType = NULL;
    if ( !xl6CgenResolveClassProtocolReceiverForCall(ctx, receiver, "__call__", node, &classNode, &ownerClass, &method, &receiverType) ) {
        return 0;
    }

    args = node->childCount > 1 ? node->children[1] : NULL;
    params = method != NULL && method->childCount > 1 ? method->children[1] : NULL;
    hasParams = params != NULL && params->childCount > 0;
    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    if ( hasParams ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenKnownCallArgs(ctx, method, args);
    }
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenClassProtocolMemberHasLenGetter(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL ||
         !xl6CTextEquals(member->text, "length") || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    if ( !xl6CgenResolveClassProtocolReceiver(ctx, receiver, "__len__", &classNode, NULL, NULL, NULL) ) {
        return 0;
    }
    return xl6CgenFindClassFieldNode(ctx, classNode, member->text) == NULL;
}

static int xl6CgenClassProtocolLenMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    XL6_AstNode* receiverType;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL ||
         !xl6CTextEquals(member->text, "length") || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    receiverType = NULL;
    if ( !xl6CgenResolveClassProtocolReceiver(ctx, receiver, "__len__", &classNode, &ownerClass, &method, &receiverType) ) {
        return 0;
    }
    if ( xl6CgenFindClassFieldNode(ctx, classNode, member->text) != NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }

    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenClassProtocolMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    XL6_AstNode* receiverType;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    if ( xl6CgenClassProtocolLenMemberAccess(ctx, member) ) {
        return 1;
    }
    receiver = member->children[0];
    receiverType = NULL;
    if ( !xl6CgenResolveClassProtocolReceiver(ctx, receiver, "__field__", &classNode, &ownerClass, &method, &receiverType) ) {
        return 0;
    }
    if ( xl6CgenFindClassFieldNode(ctx, classNode, member->text) != NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }

    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, member->text);
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenClassProtocolMemberHasGetter(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    if ( xl6CgenClassProtocolMemberHasLenGetter(ctx, member) ) {
        return 1;
    }
    receiver = member->children[0];
    if ( !xl6CgenResolveClassProtocolReceiver(ctx, receiver, "__field__", &classNode, NULL, NULL, NULL) ) {
        return 0;
    }
    return xl6CgenFindClassFieldNode(ctx, classNode, member->text) == NULL;
}

static int xl6CgenClassProtocolMemberSetStmt(XL6_CgenContext* ctx, const XL6_AstNode* left, const XL6_AstNode* right)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* valueType;
    XL6_AstNode* receiverType;
    int valueIsXValue;
    char valueTemp[64];

    if ( left == NULL || left->kind != XL6_AST_MEMBER || left->text == NULL || left->childCount == 0 ) {
        return 0;
    }
    if ( xl6CgenClassProtocolMemberHasLenGetter(ctx, left) ) {
        return 0;
    }
    receiver = left->children[0];
    receiverType = NULL;
    if ( !xl6CgenResolveClassProtocolReceiver(ctx, receiver, "__setField__", &classNode, &ownerClass, &method, &receiverType) ) {
        return 0;
    }
    if ( !xl6CgenClassProtocolReceiverIsAddressable(receiver) ) {
        xl6AstFree(receiverType);
        xl6CgenError(ctx, left, "协议字段写入接收者必须可寻址");
        return 0;
    }
    if ( xl6CgenFindClassFieldNode(ctx, classNode, left->text) != NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }

    valueType = xl6CgenClassProtocolParamType(method, 1);
    valueIsXValue = xl6CgenIsXValueType(valueType);
    valueTemp[0] = '\0';
    if ( valueIsXValue ) {
        snprintf(valueTemp, sizeof(valueTemp), "__xl6_protocol_value_%d", ctx->tempId++);
        xl6CgenClassProtocolTempInit(ctx, valueTemp, valueType, right);
    }
    xl6CgenIndent(ctx);
    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        if ( valueIsXValue ) {
            xl6CgenClassProtocolTempCleanup(ctx, valueTemp);
        }
        return 1;
    }
    xl6AstFree(receiverType);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, left->text);
    xl6OutputPuts(", ", ctx->out);
    if ( valueIsXValue ) {
        xl6OutputPuts(valueTemp, ctx->out);
    } else {
        xl6CgenClassProtocolExprArg(ctx, valueType, right);
    }
    xl6OutputPuts(");\n", ctx->out);
    return 1;
}

static int xl6CgenClassProtocolIndexHasGetter(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* protocolCall;
    int result;

    if ( node == NULL || (node->kind != XL6_AST_INDEX && node->kind != XL6_AST_SAFE_INDEX) || node->childCount < 2 ) {
        return 0;
    }
    receiver = node->children[0];
    protocolCall = xl6CgenProtocolTempCallNew(node);
    if ( protocolCall == NULL || !xl6CgenProtocolCallAddIndexArgs(protocolCall, node) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        return 0;
    }
    result = xl6CgenResolveClassProtocolReceiverForCall(ctx, receiver, "__item__", protocolCall, NULL, NULL, NULL, NULL);
    xl6CgenProtocolTempCallFree(protocolCall);
    return result;
}

static int xl6CgenClassProtocolIndexExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    XL6_AstNode* receiverType;
    XL6_AstNode* protocolCall;
    size_t i;

    if ( node == NULL || (node->kind != XL6_AST_INDEX && node->kind != XL6_AST_SAFE_INDEX) || node->childCount < 2 ) {
        return 0;
    }
    receiver = node->children[0];
    receiverType = NULL;
    protocolCall = xl6CgenProtocolTempCallNew(node);
    if ( protocolCall == NULL || !xl6CgenProtocolCallAddIndexArgs(protocolCall, node) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        return 0;
    }
    if ( !xl6CgenResolveClassProtocolReceiverForCall(ctx, receiver, "__item__", protocolCall, &classNode, &ownerClass, &method, &receiverType) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        return 0;
    }
    xl6CgenProtocolTempCallFree(protocolCall);

    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    for ( i = 0; i < xl6CgenIndexArgCount(node); ++i ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenClassProtocolExprArg(ctx, xl6CgenClassProtocolParamType(method, i), xl6CgenIndexArgAt(node, i));
    }
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenClassProtocolIndexSetStmt(XL6_CgenContext* ctx, const XL6_AstNode* left, const XL6_AstNode* right)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* index;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* keyType;
    const XL6_AstNode* valueType;
    XL6_AstNode* receiverType;
    XL6_AstNode* protocolCall;
    int keyIsXValue;
    int valueIsXValue;
    char keyTemp[64];
    char valueTemp[64];
    size_t argCount;
    size_t i;

    if ( left == NULL || left->kind != XL6_AST_INDEX || left->childCount < 2 ) {
        return 0;
    }
    receiver = left->children[0];
    index = xl6CgenIndexArgAt(left, 0);
    argCount = xl6CgenIndexArgCount(left);
    receiverType = NULL;
    protocolCall = xl6CgenProtocolTempCallNew(left);
    if ( protocolCall == NULL ||
         !xl6CgenProtocolCallAddIndexArgs(protocolCall, left) ||
         !xl6CgenProtocolTempCallAddArg(protocolCall, right) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        return 0;
    }
    if ( !xl6CgenResolveClassProtocolReceiverForCall(ctx, receiver, "__setItem__", protocolCall, &classNode, &ownerClass, &method, &receiverType) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        return 0;
    }
    xl6CgenProtocolTempCallFree(protocolCall);
    if ( !xl6CgenClassProtocolReceiverIsAddressable(receiver) ) {
        xl6AstFree(receiverType);
        xl6CgenError(ctx, left, "协议索引写入接收者必须可寻址");
        return 0;
    }

    keyType = xl6CgenClassProtocolParamType(method, 0);
    valueType = xl6CgenClassProtocolParamType(method, argCount);
    keyIsXValue = xl6CgenIsXValueType(keyType);
    valueIsXValue = xl6CgenIsXValueType(valueType);
    keyTemp[0] = '\0';
    valueTemp[0] = '\0';
    if ( keyIsXValue ) {
        snprintf(keyTemp, sizeof(keyTemp), "__xl6_protocol_key_%d", ctx->tempId++);
        xl6CgenClassProtocolTempInit(ctx, keyTemp, keyType, index);
    }
    if ( valueIsXValue ) {
        snprintf(valueTemp, sizeof(valueTemp), "__xl6_protocol_value_%d", ctx->tempId++);
        xl6CgenClassProtocolTempInit(ctx, valueTemp, valueType, right);
    }
    xl6CgenIndent(ctx);
    if ( !xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, ownerClass, method, receiverType) ) {
        xl6AstFree(receiverType);
        if ( valueIsXValue ) {
            xl6CgenClassProtocolTempCleanup(ctx, valueTemp);
        }
        if ( keyIsXValue ) {
            xl6CgenClassProtocolTempCleanup(ctx, keyTemp);
        }
        return 1;
    }
    xl6AstFree(receiverType);
    xl6OutputPuts(", ", ctx->out);
    if ( keyIsXValue ) {
        xl6OutputPuts(keyTemp, ctx->out);
    } else {
        xl6CgenClassProtocolExprArg(ctx, keyType, index);
    }
    for ( i = 1; i < argCount; ++i ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenClassProtocolExprArg(ctx, xl6CgenClassProtocolParamType(method, i), xl6CgenIndexArgAt(left, i));
    }
    xl6OutputPuts(", ", ctx->out);
    if ( valueIsXValue ) {
        xl6OutputPuts(valueTemp, ctx->out);
    } else {
        xl6CgenClassProtocolExprArg(ctx, valueType, right);
    }
    xl6OutputPuts(");\n", ctx->out);
    return 1;
}

