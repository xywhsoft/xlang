static XL6_AstNode* xl6CgenCloneTypeNode(const XL6_AstNode* node)
{
    XL6_AstNode* copy;
    size_t i;

    if ( node == NULL ) {
        return NULL;
    }
    if ( node->text != NULL ) {
        copy = xl6AstNewText(node->kind, node->text, strlen(node->text), NULL);
    } else {
        copy = xl6AstNew(node->kind, NULL);
    }
    if ( copy == NULL ) {
        return NULL;
    }
    copy->line = node->line;
    copy->column = node->column;
    copy->flags = node->flags;
    for ( i = 0; i < node->childCount; ++i ) {
        XL6_AstNode* childCopy = xl6CgenCloneTypeNode(node->children[i]);
        if ( childCopy == NULL ) {
            xl6AstFree(copy);
            return NULL;
        }
        xl6AstAddChild(copy, childCopy);
    }
    return copy;
}

static XL6_AstNode* xl6CgenCloneExprTypeForInference(XL6_CgenContext* ctx, const XL6_AstNode* expr);

static XL6_AstNode* xl6CgenNewSimpleType(const char* name, const XL6_AstNode* at)
{
    XL6_AstNode* node;

    if ( name == NULL ) {
        return NULL;
    }
    node = xl6AstNewText(XL6_AST_TYPE, name, strlen(name), NULL);
    if ( node != NULL && at != NULL ) {
        node->line = at->line;
        node->column = at->column;
    }
    return node;
}

static int xl6CgenTypeHasImplicitToString(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* name;
    XL6_AstNode* classNode;

    concreteType = xl6CgenResolveAliasType(ctx, typeNode);
    if ( xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
        concreteType = xl6CgenClassHandleTargetType(ctx, concreteType);
    }
    name = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6CTextEquals(name, "null") ||
         xl6CTextEquals(name, "var") ||
         xl6CTextEquals(name, "bool") ||
         xl6CgenIsIntegerTypeName(name) ||
         xl6CgenIsFloatTypeName(name) ||
         xl6CTextEquals(name, "string") ||
         xl6CTextEquals(name, "time") ||
         xl6CTextEquals(name, "point") ) {
        return 1;
    }
    classNode = name != NULL ? xl6CgenFindClass(ctx, name) : NULL;
    return classNode != NULL &&
           ((!xl6CgenClassHasTypeParams(classNode) && xl6CgenClassHasDerive(classNode, "string")) ||
            xl6CgenFindClassFunc(classNode, "__toString__") != NULL);
}

static int xl6CgenIsToStringMemberCallSyntax(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER || !xl6CTextEquals(callee->text, "toString") ) {
        return 0;
    }
    args = node->childCount > 1 ? node->children[1] : NULL;
    if ( args != NULL && args->childCount != 0 ) {
        return 0;
    }
    return 1;
}

static int xl6CgenIsImplicitToStringCall(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    int result;

    if ( !xl6CgenIsToStringMemberCallSyntax(node) ) {
        return 0;
    }

    callee = node->children[0];
    receiver = callee->childCount > 0 ? callee->children[0] : NULL;
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    result = xl6CgenTypeHasImplicitToString(ctx, receiverType);
    xl6AstFree(receiverType);
    return result;
}

static XL6_AstNode* xl6CgenCloneCanonicalTypeNode(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* concrete;
    XL6_AstNode* copy;
    size_t i;

    if ( node == NULL ) {
        return NULL;
    }

    concrete = xl6CgenSubstituteGenericType(ctx, node);
    concrete = xl6CgenResolveAliasType(ctx, concrete);
    if ( concrete == NULL ) {
        return NULL;
    }

    if ( concrete->text != NULL ) {
        copy = xl6AstNewText(concrete->kind, concrete->text, strlen(concrete->text), NULL);
    } else {
        copy = xl6AstNew(concrete->kind, NULL);
    }
    if ( copy == NULL ) {
        return NULL;
    }
    copy->line = concrete->line;
    copy->column = concrete->column;
    /*
        规范类型克隆必须完整保留语义身份。
        nullable、protocol、enum 等规范类型都可能通过 flags 或稳定类型 ID
        区分于同名的普通注册类型；只复制文本会让后端重新猜测类型。
    */
    copy->flags = concrete->flags;
    copy->semanticTypeId = concrete->semanticTypeId;
    copy->semanticTypeAbiId = concrete->semanticTypeAbiId;
    /*
        变量表中仍可能保存语法阶段创建的类型节点。协议身份来自当前模块的
        规范类型注册表；克隆时补写一次，后续所有 xvalue* 路径便无需按名称猜测。
    */
    if ( copy->text != NULL &&
         xl6CgenFindProtocol(ctx, copy->text) != NULL ) {
        copy->flags |= XL6_AST_FLAG_PROTOCOL_HANDLE;
    }
    if ( copy->text != NULL &&
         xl6CgenIsTaggedEnumType(ctx, copy) ) {
        copy->flags |= XL6_AST_FLAG_TAGGED_ENUM;
    }

    for ( i = 0; i < concrete->childCount; ++i ) {
        XL6_AstNode* childCopy = xl6CgenCloneCanonicalTypeNode(ctx, concrete->children[i]);
        if ( childCopy == NULL ) {
            xl6AstFree(copy);
            return NULL;
        }
        xl6AstAddChild(copy, childCopy);
    }
    return copy;
}

static XL6_AstNode* xl6CgenNewGenericTypeNode(const char* name, XL6_AstNode* typeArg, const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* args;

    if ( typeArg == NULL ) {
        return xl6CgenNewSimpleType(name, at);
    }

    typeNode = xl6CgenNewSimpleType(name, at);
    args = xl6AstNew(XL6_AST_LIST, NULL);
    if ( typeNode == NULL || args == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(args);
        xl6AstFree(typeArg);
        return NULL;
    }

    xl6AstAddChild(args, typeArg);
    xl6AstAddChild(typeNode, args);
    return typeNode;
}

static int xl6CgenTypedContainerBinaryInfo(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    XL6_AstNode** outResultType,
    int* outBoolResult)
{
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    XL6_AstNode* leftCanonical;
    XL6_AstNode* rightCanonical;
    const XL6_AstNode* leftConcrete;
    const char* kind;
    const char* op;
    int sameType;
    int ok;
    int boolResult;

    if ( outResultType != NULL ) {
        *outResultType = NULL;
    }
    if ( outBoolResult != NULL ) {
        *outBoolResult = 0;
    }
    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_BINARY ||
         expr->childCount < 2 || expr->text == NULL ) {
        return 0;
    }

    op = expr->text;
    leftType = xl6CgenCloneExprTypeForInference(ctx, expr->children[0]);
    rightType = xl6CgenCloneExprTypeForInference(ctx, expr->children[1]);
    leftCanonical = xl6CgenCloneCanonicalTypeNode(ctx, leftType);
    rightCanonical = xl6CgenCloneCanonicalTypeNode(ctx, rightType);
    leftConcrete = xl6CgenTypedContainerConcreteType(ctx, leftCanonical);
    kind = leftConcrete != NULL ? leftConcrete->text : NULL;
    sameType = leftCanonical != NULL && rightCanonical != NULL &&
               xl6CgenTypeNodeEqual(leftCanonical, rightCanonical);
    ok = 0;
    boolResult = 0;

    if ( kind != NULL ) {
        if ( sameType &&
             (xl6CTextEquals(op, "==") || xl6CTextEquals(op, "!=")) ) {
            ok = 1;
            boolResult = 1;
        } else if ( sameType && xl6CTextEquals(kind, "array") &&
                    xl6CTextEquals(op, "+") ) {
            ok = 1;
        } else if ( sameType && xl6CTextEquals(kind, "list") &&
                    xl6CTextEquals(op, "+") ) {
            ok = 1;
        } else if ( xl6CTextEquals(kind, "list") && xl6CTextEquals(op, "-") &&
                    rightCanonical != NULL && xl6CgenIsIntegerTypeName(rightCanonical->text) ) {
            ok = 1;
        } else if ( sameType && xl6CTextEquals(kind, "dict") &&
                    xl6CTextEquals(op, "+") ) {
            ok = 1;
        } else if ( xl6CTextEquals(kind, "dict") && xl6CTextEquals(op, "-") &&
                    rightCanonical != NULL && xl6CTextEquals(rightCanonical->text, "string") ) {
            ok = 1;
        } else if ( sameType && xl6CTextEquals(kind, "set") &&
                    (xl6CTextEquals(op, "|") || xl6CTextEquals(op, "&") ||
                     xl6CTextEquals(op, "-") || xl6CTextEquals(op, "^")) ) {
            ok = 1;
        } else if ( sameType && xl6CTextEquals(kind, "set") &&
                    (xl6CTextEquals(op, "<") || xl6CTextEquals(op, "<=") ||
                     xl6CTextEquals(op, ">") || xl6CTextEquals(op, ">=")) ) {
            ok = 1;
            boolResult = 1;
        }
    }

    if ( ok && outResultType != NULL ) {
        *outResultType = boolResult ? xl6CgenNewSimpleType("bool", expr) : xl6CgenCloneTypeNode(leftCanonical);
    }
    if ( ok && outBoolResult != NULL ) {
        *outBoolResult = boolResult;
    }

    xl6AstFree(leftType);
    xl6AstFree(rightType);
    xl6AstFree(leftCanonical);
    xl6AstFree(rightCanonical);
    return ok;
}

static int xl6CgenIsFutureLikeType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL &&
           (xl6CTextEquals(typeNode->text, "future") ||
            xl6CTextEquals(typeNode->text, "task"));
}

static int xl6CgenIsChannelType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && xl6CTextEquals(typeNode->text, "channel");
}

static int xl6CgenIsThreadType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && xl6CTextEquals(typeNode->text, "thread");
}

static int xl6CgenIsGeneratorType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && xl6CTextEquals(typeNode->text, "generator");
}

static int xl6CgenIsSyncHandleTypeName(const char* name)
{
    return xl6CTextEquals(name, "mutex") ||
           xl6CTextEquals(name, "sem") ||
           xl6CTextEquals(name, "cond") ||
           xl6CTextEquals(name, "rwlock");
}

static int xl6CgenIsSyncHandleType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && xl6CgenIsSyncHandleTypeName(typeNode->text);
}
