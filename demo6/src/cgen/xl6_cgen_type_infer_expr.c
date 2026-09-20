static XL6_AstNode* xl6CgenCloneExprTypeForInferenceRaw(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* varType;
    XL6_AstNode* receiverType;

    if ( expr == NULL ) {
        return NULL;
    }
    switch ( expr->kind ) {
    case XL6_AST_INT:
        return xl6CgenNewSimpleType("int", expr);
    case XL6_AST_FLOAT:
        return xl6CgenNewSimpleType("float", expr);
    case XL6_AST_STRING:
    case XL6_AST_INTERP_STRING:
        return xl6CgenNewSimpleType("string", expr);
    case XL6_AST_TIME:
        return xl6CgenNewSimpleType("time", expr);
    case XL6_AST_BOOL:
        return xl6CgenNewSimpleType("bool", expr);
    case XL6_AST_NULL:
        return xl6CgenNewSimpleType("null", expr);
    case XL6_AST_ARRAY_LITERAL:
        return xl6CgenCloneContainerLiteralType(ctx, "array", expr, expr);
    case XL6_AST_LIST_LITERAL:
        return xl6CgenCloneContainerLiteralType(ctx, "list", expr, expr);
    case XL6_AST_SET_LITERAL:
        return xl6CgenCloneContainerLiteralType(ctx, "set", expr, expr);
    case XL6_AST_DICT_LITERAL:
        return xl6CgenCloneContainerLiteralType(ctx, "dict", expr, expr);
    case XL6_AST_TYPED_LITERAL:
        if ( expr->text != NULL &&
             (xl6CTextEquals(expr->text, "array") || xl6CTextEquals(expr->text, "list") ||
              xl6CTextEquals(expr->text, "set") || xl6CTextEquals(expr->text, "dict")) ) {
            if ( expr->childCount > 1 && expr->children[1] != NULL ) {
                return xl6CgenCloneCanonicalTypeNode(ctx, expr->children[1]);
            }
            return xl6CgenCloneContainerLiteralType(
                ctx,
                expr->text,
                expr->childCount > 0 ? expr->children[0] : NULL,
                expr);
        }
        return xl6CgenNewSimpleType(expr->text != NULL ? expr->text : "var", expr);
    case XL6_AST_ANON_FUNC:
        return xl6CgenAnonFunctionType(expr);
    case XL6_AST_AWAIT:
        return xl6CgenCloneAwaitResultType(ctx, expr);
    case XL6_AST_TYPE_IS:
        return xl6CgenNewSimpleType("bool", expr);
    case XL6_AST_TYPE_AS:
        return expr->childCount > 1 ? xl6CgenCloneTypeNode(expr->children[1]) : NULL;
    case XL6_AST_NAME:
        varType = xl6CgenFindVarType(ctx, expr->text);
        if ( varType != NULL && xl6CgenIsXValueType(varType) ) {
            const XL6_AstNode* narrowType = xl6CgenFindVarNarrowType(ctx, expr->text);
            if ( narrowType != NULL ) {
                return xl6CgenCloneCanonicalTypeNode(ctx, narrowType);
            }
        }
        if ( varType != NULL && !xl6CTextEquals(varType->text, "auto") ) {
            /*
                泛型函数和泛型类的局部类型表保存声明形态，例如 T、array<T>。
                推断表达式类型时必须转换为当前实例的具体类型，否则语义检查
                已通过的 self.method(param) 会在 C 生成阶段再次匹配为 T != int。
            */
            return xl6CgenCloneCanonicalTypeNode(ctx, varType);
        }
        {
            XL6_AstNode* fieldType = xl6CgenCloneImplicitSelfFieldType(ctx, expr->text);
            if ( fieldType != NULL ) {
                return fieldType;
            }
        }
        if ( xl6CgenIsKnownTypeName(ctx, expr->text) ) {
            return xl6CgenNewSimpleType("type", expr);
        }
        return NULL;
    case XL6_AST_UNARY: {
        XL6_AstNode* valueType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
        int isXValue = xl6CgenIsXValueType(valueType);
        const char* protocolName = expr->childCount > 0 && expr->children[0] != NULL ?
                                   xl6CgenUnaryProtocolName(expr->text) :
                                   NULL;
        XL6_AstNode* protocolCall = protocolName != NULL ? xl6CgenProtocolTempCallNew(expr) : NULL;
        XL6_AstNode* protocolType = protocolCall != NULL ?
                                    xl6CgenCloneClassProtocolReturnTypeForCall(ctx, valueType, protocolName, protocolCall) :
                                    NULL;
        xl6CgenProtocolTempCallFree(protocolCall);

        if ( protocolType != NULL ) {
            xl6AstFree(valueType);
            return protocolType;
        }

        if ( xl6CTextEquals(expr->text, "not") || xl6CTextEquals(expr->text, "!") ) {
            xl6AstFree(valueType);
            return xl6CgenNewSimpleType("bool", expr);
        }
        if ( isXValue ) {
            xl6AstFree(valueType);
            return xl6CgenNewSimpleType("var", expr);
        }
        if ( valueType != NULL &&
             (xl6CTextEquals(expr->text, "+") || xl6CTextEquals(expr->text, "-")) &&
             xl6CgenIsFloatTypeName(valueType->text) ) {
            xl6AstFree(valueType);
            return xl6CgenNewSimpleType("float", expr);
        }
        if ( valueType != NULL &&
             (xl6CTextEquals(expr->text, "+") || xl6CTextEquals(expr->text, "-") ||
              xl6CTextEquals(expr->text, "~")) &&
             xl6CgenIsIntegerTypeName(valueType->text) ) {
            xl6AstFree(valueType);
            return xl6CgenNewSimpleType("int", expr);
        }
        xl6AstFree(valueType);
        return NULL;
    }
    case XL6_AST_BINARY:
        if ( xl6CTextEquals(expr->text, "+") ) {
            XL6_AstNode* leftType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
            XL6_AstNode* rightType = expr->childCount > 1 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[1]) : NULL;
            XL6_AstNode* leftCanonical = xl6CgenCloneCanonicalTypeNode(ctx, leftType);
            XL6_AstNode* rightCanonical = xl6CgenCloneCanonicalTypeNode(ctx, rightType);
            const XL6_AstNode* leftContainer = xl6CgenTypedContainerConcreteType(ctx, leftCanonical);
            const char* leftContainerName = leftContainer != NULL ? leftContainer->text : NULL;
            int sameType = leftCanonical != NULL && rightCanonical != NULL &&
                           xl6CgenTypeNodeEqual(leftCanonical, rightCanonical);
            int isString = leftType != NULL && rightType != NULL &&
                           xl6CTextEquals(leftType->text, "string") &&
                           xl6CTextEquals(rightType->text, "string");
            int isTypedContainerConcat = sameType &&
                                         (xl6CTextEquals(leftContainerName, "array") ||
                                          xl6CTextEquals(leftContainerName, "list") ||
                                          xl6CTextEquals(leftContainerName, "dict"));
            int isXValue = !isTypedContainerConcat &&
                            ((xl6CgenIsXValueType(leftType) && xl6CgenClassHandleTargetType(ctx, leftType) == NULL) ||
                             (xl6CgenIsXValueType(rightType) && xl6CgenClassHandleTargetType(ctx, rightType) == NULL));
            int isNumeric = leftType != NULL && rightType != NULL &&
                            (xl6CgenIsIntegerTypeName(leftType->text) || xl6CgenIsFloatTypeName(leftType->text)) &&
                            (xl6CgenIsIntegerTypeName(rightType->text) || xl6CgenIsFloatTypeName(rightType->text));
            int isFloat = isNumeric &&
                          (xl6CgenIsFloatTypeName(leftType->text) || xl6CgenIsFloatTypeName(rightType->text));
            if ( isTypedContainerConcat ) {
                XL6_AstNode* result = xl6CgenCloneTypeNode(leftCanonical);
                xl6AstFree(leftType);
                xl6AstFree(rightType);
                xl6AstFree(leftCanonical);
                xl6AstFree(rightCanonical);
                return result;
            }
            xl6AstFree(leftType);
            xl6AstFree(rightType);
            xl6AstFree(leftCanonical);
            xl6AstFree(rightCanonical);
            if ( isString ) {
                return xl6CgenNewSimpleType("string", expr);
            }
            if ( isXValue ) {
                return xl6CgenNewSimpleType("var", expr);
            }
            if ( isNumeric ) {
                return xl6CgenNewSimpleType(isFloat ? "float" : "int", expr);
            }
        }
        if ( expr->childCount > 1 ) {
            const char* protocolName = NULL;
            XL6_AstNode* receiverType = NULL;
            XL6_AstNode* protocolType = NULL;
            XL6_AstNode* protocolCall = NULL;
            const XL6_AstNode* protocolArg = NULL;
            if ( xl6CTextEquals(expr->text, "in") ) {
                protocolName = expr->children[1] != NULL ? "__contains__" : NULL;
                receiverType = protocolName != NULL ? xl6CgenCloneExprTypeForInference(ctx, expr->children[1]) : NULL;
                protocolArg = expr->children[0];
            } else {
                protocolName = expr->children[0] != NULL ?
                               xl6CgenBinaryProtocolName(expr->text) :
                               NULL;
                receiverType = protocolName != NULL ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
                protocolArg = expr->children[1];
            }
            protocolCall = protocolName != NULL ? xl6CgenProtocolTempCallNew(expr) : NULL;
            if ( protocolCall != NULL && protocolArg != NULL ) {
                xl6CgenProtocolTempCallAddArg(protocolCall, protocolArg);
            }
            protocolType = protocolCall != NULL ?
                           xl6CgenCloneClassProtocolReturnTypeForCall(ctx, receiverType, protocolName, protocolCall) :
                           NULL;
            xl6CgenProtocolTempCallFree(protocolCall);
            xl6AstFree(receiverType);
            if ( protocolType != NULL ) {
                return protocolType;
            }
        }
        {
            XL6_AstNode* containerResultType = NULL;
            if ( xl6CgenTypedContainerBinaryInfo(ctx, expr, &containerResultType, NULL) ) {
                return containerResultType;
            }
        }
        if ( xl6CTextEquals(expr->text, "and") || xl6CTextEquals(expr->text, "or") ||
             xl6CTextEquals(expr->text, "==") || xl6CTextEquals(expr->text, "!=") ||
             xl6CTextEquals(expr->text, "~=") ||
             xl6CTextEquals(expr->text, "<") || xl6CTextEquals(expr->text, "<=") ||
             xl6CTextEquals(expr->text, ">") || xl6CTextEquals(expr->text, ">=") ) {
            return xl6CgenNewSimpleType("bool", expr);
        }
        if ( xl6CTextEquals(expr->text, "in") ) {
            return xl6CgenNewSimpleType("bool", expr);
        }
        if ( xl6CTextEquals(expr->text, "??") ) {
            XL6_AstNode* leftType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
            XL6_AstNode* rightType = expr->childCount > 1 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[1]) : NULL;
            XL6_AstNode* result = NULL;

            if ( leftType != NULL && xl6CTextEquals(leftType->text, "null") && rightType != NULL ) {
                result = xl6CgenCloneTypeNode(rightType);
            } else if ( rightType != NULL && xl6CTextEquals(rightType->text, "null") && leftType != NULL ) {
                result = xl6CgenCloneTypeNode(leftType);
            } else if ( leftType != NULL && rightType != NULL && xl6CgenTypeNodeEqual(leftType, rightType) ) {
                result = xl6CgenCloneTypeNode(leftType);
            } else if ( (leftType != NULL && xl6CgenIsXValueType(leftType)) ||
                        (rightType != NULL && xl6CgenIsXValueType(rightType)) ) {
                result = xl6CgenNewSimpleType("var", expr);
            }
            xl6AstFree(leftType);
            xl6AstFree(rightType);
            return result;
        }
        if ( xl6CTextEquals(expr->text, "-") || xl6CTextEquals(expr->text, "*") ||
             xl6CTextEquals(expr->text, "/") ) {
            XL6_AstNode* leftType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
            XL6_AstNode* rightType = expr->childCount > 1 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[1]) : NULL;
            int isXValue = xl6CgenIsXValueType(leftType) || xl6CgenIsXValueType(rightType);
            int isNumeric = leftType != NULL && rightType != NULL &&
                            (xl6CgenIsIntegerTypeName(leftType->text) || xl6CgenIsFloatTypeName(leftType->text)) &&
                            (xl6CgenIsIntegerTypeName(rightType->text) || xl6CgenIsFloatTypeName(rightType->text));
            int isFloat = isNumeric &&
                          (xl6CgenIsFloatTypeName(leftType->text) || xl6CgenIsFloatTypeName(rightType->text));
            xl6AstFree(leftType);
            xl6AstFree(rightType);
            if ( isXValue ) {
                return xl6CgenNewSimpleType("var", expr);
            }
            if ( isNumeric ) {
                return xl6CgenNewSimpleType(isFloat ? "float" : "int", expr);
            }
        }
        if ( xl6CTextEquals(expr->text, "%") || xl6CTextEquals(expr->text, "&") || xl6CTextEquals(expr->text, "|") ||
             xl6CTextEquals(expr->text, "^") || xl6CTextEquals(expr->text, "<<") ||
             xl6CTextEquals(expr->text, ">>") ) {
            XL6_AstNode* leftType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
            XL6_AstNode* rightType = expr->childCount > 1 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[1]) : NULL;
            int isXValue = xl6CgenIsXValueType(leftType) || xl6CgenIsXValueType(rightType);
            int isInteger = leftType != NULL && rightType != NULL &&
                            xl6CgenIsIntegerTypeName(leftType->text) &&
                            xl6CgenIsIntegerTypeName(rightType->text);
            xl6AstFree(leftType);
            xl6AstFree(rightType);
            if ( isXValue ) {
                return xl6CgenNewSimpleType("var", expr);
            }
            if ( isInteger ) {
                return xl6CgenNewSimpleType("int", expr);
            }
        }
        if ( xl6CTextEquals(expr->text, "**") && xl6CgenExprIntPowKind(ctx, expr) ) {
            return xl6CgenNewSimpleType("int", expr);
        }
        return NULL;
    case XL6_AST_CONDITIONAL: {
        XL6_AstNode* thenType = expr->childCount > 1 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[1]) : NULL;
        XL6_AstNode* elseType = expr->childCount > 2 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[2]) : NULL;
        XL6_AstNode* result = NULL;

        if ( xl6CgenIsXValueType(thenType) || xl6CgenIsXValueType(elseType) ) {
            result = xl6CgenNewSimpleType("var", expr);
        } else if ( thenType != NULL && xl6CTextEquals(thenType->text, "null") && elseType != NULL ) {
            result = xl6CgenCloneTypeNode(elseType);
        } else if ( elseType != NULL && xl6CTextEquals(elseType->text, "null") && thenType != NULL ) {
            result = xl6CgenCloneTypeNode(thenType);
        } else if ( thenType != NULL && elseType != NULL && xl6CgenTypeNodeEqual(thenType, elseType) ) {
            result = xl6CgenCloneTypeNode(thenType);
        } else if ( thenType != NULL && elseType != NULL ) {
            result = xl6CgenNewSimpleType("var", expr);
        }

        xl6AstFree(thenType);
        xl6AstFree(elseType);
        return result;
    }
    case XL6_AST_MEMBER:
        receiverType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
        if ( receiverType != NULL && xl6CTextEquals(receiverType->text, "error") ) {
            int isTextField = xl6CTextEquals(expr->text, "kind") ||
                               xl6CTextEquals(expr->text, "message") ||
                               xl6CTextEquals(expr->text, "file") ||
                               xl6CTextEquals(expr->text, "domain") ||
                               xl6CTextEquals(expr->text, "operation") ||
                               xl6CTextEquals(expr->text, "data");
            int isIntField = xl6CTextEquals(expr->text, "line") ||
                              xl6CTextEquals(expr->text, "column") ||
                              xl6CTextEquals(expr->text, "code") ||
                              xl6CTextEquals(expr->text, "systemCode");
            int isCauseField = xl6CTextEquals(expr->text, "cause");
            xl6AstFree(receiverType);
            if ( isTextField ) {
                return xl6CgenNewSimpleType("string", expr);
            }
            if ( isIntField ) {
                return xl6CgenNewSimpleType("int", expr);
            }
            if ( isCauseField ) {
                return xl6CgenNewSimpleType("error", expr);
            }
            return NULL;
        }
        if ( xl6CgenIsXValueType(receiverType) &&
             xl6CgenClassHandleTargetType(ctx, receiverType) == NULL ) {
            int isLength = expr->text != NULL && xl6CTextEquals(expr->text, "length");
            int isDynamicKey = xl6CgenMemberKeyReceiverIsDynamic(ctx, receiverType);
            xl6AstFree(receiverType);
            if ( isLength ) {
                return xl6CgenNewSimpleType("int", expr);
            }
            return isDynamicKey ? xl6CgenNewSimpleType("var", expr) : NULL;
        }
        if ( expr->text != NULL &&
             xl6CTextEquals(expr->text, "length") &&
             expr->childCount > 0 &&
             xl6CgenExprUsesTypedContainerStorage(ctx, expr->children[0]) &&
             xl6CgenIsTypedContainerType(ctx, receiverType) ) {
            xl6AstFree(receiverType);
            return xl6CgenNewSimpleType("int", expr);
        }
        xl6AstFree(receiverType);
        {
            XL6_AstNode* fieldType = xl6CgenCloneMemberFieldTypeForInference(ctx, expr);
            if ( fieldType != NULL ) {
                return fieldType;
            }
        }
        return NULL;
    case XL6_AST_SAFE_MEMBER:
        receiverType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
        if ( xl6CgenIsXValueType(receiverType) &&
             xl6CgenClassHandleTargetType(ctx, receiverType) == NULL ) {
            int isLength = expr->text != NULL && xl6CTextEquals(expr->text, "length");
            int isAllowed = isLength || xl6CgenMemberKeyReceiverIsDynamic(ctx, receiverType);
            xl6AstFree(receiverType);
            return isAllowed ? xl6CgenNewSimpleType("var", expr) : NULL;
        }
        if ( expr->text != NULL &&
             xl6CTextEquals(expr->text, "length") &&
             expr->childCount > 0 &&
             xl6CgenExprUsesTypedContainerStorage(ctx, expr->children[0]) &&
             xl6CgenIsTypedContainerType(ctx, receiverType) ) {
            xl6AstFree(receiverType);
            return xl6CgenNewSimpleType("var", expr);
        }
        xl6AstFree(receiverType);
        return NULL;
    case XL6_AST_INDEX:
    case XL6_AST_SAFE_INDEX:
        if ( expr->childCount > 0 && expr->children[0] != NULL &&
             expr->children[0]->kind == XL6_AST_MEMBER ) {
            XL6_AstNode* propertyType = xl6CgenCloneClassPropertyMemberType(ctx, expr->children[0], expr);
            if ( propertyType != NULL ) {
                if ( expr->kind == XL6_AST_SAFE_INDEX ) {
                    xl6AstFree(propertyType);
                    return xl6CgenNewSimpleType("var", expr);
                }
                return propertyType;
            }
        }
        receiverType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
        if ( expr->childCount > 0 &&
             xl6CgenIndexArgCount(expr) == 1 &&
             xl6CgenExprUsesTypedContainerStorage(ctx, expr->children[0]) &&
             xl6CgenIsTypedContainerType(ctx, receiverType) ) {
            const XL6_AstNode* concreteType = xl6CgenTypedContainerConcreteType(ctx, receiverType);
            const XL6_AstNode* itemType = xl6CgenTypedContainerItemType(ctx, receiverType);
            XL6_AstNode* result = NULL;

            if ( concreteType != NULL && !xl6CTextEquals(concreteType->text, "set") && itemType != NULL ) {
                result = expr->kind == XL6_AST_SAFE_INDEX
                    ? xl6CgenNewSimpleType("var", expr)
                    : xl6CgenCloneCanonicalTypeNode(ctx, itemType);
            }
            xl6AstFree(receiverType);
            return result;
        }
        if ( xl6CgenIsArrayXValueType(receiverType) || xl6CgenIsListXValueType(receiverType) ||
              xl6CgenIsDictXValueType(receiverType) ||
              (receiverType != NULL && xl6CTextEquals(receiverType->text, "var")) ) {
            if ( xl6CgenIndexArgCount(expr) != 1 ) {
                xl6AstFree(receiverType);
                return NULL;
            }
            xl6AstFree(receiverType);
            return xl6CgenNewSimpleType("var", expr);
        }
        {
            XL6_AstNode* protocolType = xl6CgenCloneClassProtocolReturnTypeForIndex(ctx, expr, "__item__", NULL);
            if ( protocolType != NULL ) {
                xl6AstFree(receiverType);
                return protocolType;
            }
        }
        xl6AstFree(receiverType);
        return NULL;
    case XL6_AST_CALL:
        if ( expr->childCount > 0 && expr->children[0] != NULL &&
             expr->children[0]->kind == XL6_AST_NAME &&
             xl6CTextEquals(expr->children[0]->text, "xcall") ) {
            return xl6CgenNewSimpleType("var", expr);
        }
        if ( xl6CgenIsImplicitToStringCall(ctx, expr) ) {
            return xl6CgenNewSimpleType("string", expr);
        }
        {
            XL6_AstNode* threadMethodType = xl6CgenThreadMethodType(ctx, expr);
            if ( threadMethodType != NULL ) {
                return threadMethodType;
            }
        }
        {
            XL6_AstNode* syncMethodType = xl6CgenSyncMethodType(ctx, expr);
            if ( syncMethodType != NULL ) {
                return syncMethodType;
            }
        }
        {
            XL6_AstNode* specialCollectionMethodType = xl6CgenSpecialCollectionMethodType(ctx, expr);
            if ( specialCollectionMethodType != NULL ) {
                return specialCollectionMethodType;
            }
        }
        {
            XL6_AstNode* channelMethodType = xl6CgenChannelMethodType(ctx, expr);
            if ( channelMethodType != NULL ) {
                return channelMethodType;
            }
        }
        {
            XL6_AstNode* futureMethodType = xl6CgenFutureMethodType(ctx, expr);
            if ( futureMethodType != NULL ) {
                return futureMethodType;
            }
        }
        {
            XL6_AstNode* taskCallType = xl6CgenCloneTaskCallType(ctx, expr);
            if ( taskCallType != NULL ) {
                return taskCallType;
            }
        }
        if ( expr->childCount > 0 && expr->children[0] != NULL &&
             expr->children[0]->kind == XL6_AST_MEMBER ) {
            const XL6_AstNode* callee = expr->children[0];
            if ( callee->text != NULL && callee->childCount > 0 ) {
                XL6_AstNode* receiverType = xl6CgenCloneExprTypeForInference(ctx, callee->children[0]);
                const XL6_AstNode* typedContainer = receiverType != NULL
                    ? xl6CgenTypedContainerConcreteType(ctx, receiverType)
                    : NULL;
                int isVar = receiverType != NULL && xl6CTextEquals(receiverType->text, "var");
                int isArray = xl6CgenIsArrayXValueType(receiverType) ||
                              (typedContainer != NULL && xl6CTextEquals(typedContainer->text, "array"));
                int isList = xl6CgenIsListXValueType(receiverType) ||
                             (typedContainer != NULL && xl6CTextEquals(typedContainer->text, "list"));
                int isSet = xl6CgenIsSetXValueType(receiverType) ||
                            (typedContainer != NULL && xl6CTextEquals(typedContainer->text, "set"));
                int isDict = xl6CgenIsDictXValueType(receiverType) ||
                             (typedContainer != NULL && xl6CTextEquals(typedContainer->text, "dict"));
                int isContainerMethod = 0;

                /*
                    var 的真实运行时类型可能是 class，也可能是容器。
                    这里不能按方法名猜测容器，否则 class.add 等同名方法会被提前截获。
                    var 成员统一返回 var，由运行时先解析 class 方法，再回退到隐性容器方法。
                */
                if ( !isVar && isArray ) {
                    isContainerMethod = xl6CTextEquals(callee->text, "push") ||
                                        xl6CTextEquals(callee->text, "append") ||
                                        xl6CTextEquals(callee->text, "insert") ||
                                        xl6CTextEquals(callee->text, "remove") ||
                                        xl6CTextEquals(callee->text, "removeValue") ||
                                        xl6CTextEquals(callee->text, "pop") ||
                                        xl6CTextEquals(callee->text, "take") ||
                                        xl6CTextEquals(callee->text, "clear") ||
                                        xl6CTextEquals(callee->text, "reserve") ||
                                        xl6CTextEquals(callee->text, "swap") ||
                                        xl6CTextEquals(callee->text, "contains") ||
                                        xl6CTextEquals(callee->text, "indexOf") ||
                                        (typedContainer != NULL &&
                                         xl6CTextEquals(typedContainer->text, "array") &&
                                         xl6CTextEquals(callee->text, "reverse"));
                } else if ( !isVar && isList ) {
                    isContainerMethod = xl6CTextEquals(callee->text, "remove") ||
                                        xl6CTextEquals(callee->text, "take") ||
                                        xl6CTextEquals(callee->text, "clear") ||
                                        xl6CTextEquals(callee->text, "contains");
                } else if ( !isVar && isSet ) {
                    isContainerMethod = xl6CTextEquals(callee->text, "add") ||
                                        xl6CTextEquals(callee->text, "remove") ||
                                        xl6CTextEquals(callee->text, "contains") ||
                                        xl6CTextEquals(callee->text, "clear");
                } else if ( !isVar && isDict ) {
                    isContainerMethod = xl6CTextEquals(callee->text, "remove") ||
                                        xl6CTextEquals(callee->text, "contains") ||
                                        xl6CTextEquals(callee->text, "take") ||
                                        xl6CTextEquals(callee->text, "clear");
                }

                if ( isContainerMethod ) {
                    if ( xl6CTextEquals(callee->text, "pop") ||
                         xl6CTextEquals(callee->text, "take") ) {
                        const XL6_AstNode* itemType = (typedContainer != NULL &&
                                                       xl6CTextEquals(typedContainer->text, "array"))
                            ? xl6CgenTypedContainerItemType(ctx, receiverType)
                            : NULL;
                        const XL6_AstNode* concreteItemType = itemType != NULL
                            ? xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, itemType))
                            : NULL;
                        if ( concreteItemType != NULL &&
                             !(xl6CgenIsIntegerTypeName(concreteItemType->text) ||
                               xl6CgenIsFloatTypeName(concreteItemType->text) ||
                               xl6CgenIsBoolTypeName(concreteItemType->text) ||
                               xl6CTextEquals(concreteItemType->text, "string") ||
                               xl6CTextEquals(concreteItemType->text, "time") ||
                               xl6CTextEquals(concreteItemType->text, "point")) ) {
                            concreteItemType = NULL;
                        }
                        XL6_AstNode* result = concreteItemType != NULL
                            ? xl6CgenCloneCanonicalTypeNode(ctx, concreteItemType)
                            : xl6CgenNewSimpleType("var", expr);
                        xl6AstFree(receiverType);
                        return result;
                    }
                    if ( xl6CTextEquals(callee->text, "indexOf") ) {
                        xl6AstFree(receiverType);
                        return xl6CgenNewSimpleType("int", expr);
                    }
                    xl6AstFree(receiverType);
                    return xl6CgenNewSimpleType("bool", expr);
                }
                xl6AstFree(receiverType);
            }
            XL6_AstNode* classMethodType = xl6CgenCloneClassMethodCallType(ctx, expr);
            XL6_AstNode* memberType = xl6CgenCloneMemberFieldTypeForInference(ctx, expr->children[0]);
            if ( classMethodType != NULL ) {
                xl6AstFree(memberType);
                return classMethodType;
            }
            if ( xl6CgenIsFunctionType(memberType) ) {
                const XL6_AstNode* returns = xl6CgenFunctionReturnTypes(memberType);
                XL6_AstNode* result = NULL;

                if ( xl6CgenIsBareFunctionType(memberType) ) {
                    result = xl6CgenNewSimpleType("var", expr);
                } else if ( returns != NULL && returns->childCount == 1 ) {
                    result = xl6CgenCloneTypeNode(returns->children[0]);
                }
                xl6AstFree(memberType);
                return result;
            }
            if ( memberType != NULL && xl6CTextEquals(memberType->text, "var") ) {
                xl6AstFree(memberType);
                return xl6CgenNewSimpleType("var", expr);
            }
            xl6AstFree(memberType);
        }
        {
            XL6_AstNode* threadCtorType = xl6CgenThreadCtorType(ctx, expr);
            if ( threadCtorType != NULL ) {
                return threadCtorType;
            }
        }
        {
            XL6_AstNode* syncCtorType = xl6CgenSyncCtorType(ctx, expr);
            if ( syncCtorType != NULL ) {
                return syncCtorType;
            }
        }
        if ( expr->childCount > 0 && expr->children[0] != NULL &&
            expr->children[0]->kind != XL6_AST_NAME &&
            expr->children[0]->kind != XL6_AST_MEMBER ) {
            XL6_AstNode* calleeType = xl6CgenCloneExprTypeForInference(ctx, expr->children[0]);
            XL6_AstNode* protocolType = xl6CgenCloneClassProtocolReturnTypeForCall(ctx, calleeType, "__call__", expr);
            xl6AstFree(calleeType);
            if ( protocolType != NULL ) {
                return protocolType;
            }
        }
        if ( expr->childCount > 0 && expr->children[0] != NULL &&
             expr->children[0]->kind == XL6_AST_GENERIC_INSTANT ) {
            XL6_AstNode* persistenceType = xl6CgenClonePersistenceGenericCallType(ctx, expr);
            XL6_AstNode* genericClassType = xl6CgenGenericClassCtorType(ctx, expr, NULL);
            if ( persistenceType != NULL ) {
                return persistenceType;
            }
            if ( genericClassType != NULL ) {
                return genericClassType;
            }
            {
                XL6_AstNode* specialCollectionCtorType = xl6CgenSpecialCollectionCtorType(ctx, expr);
                if ( specialCollectionCtorType != NULL ) {
                    return specialCollectionCtorType;
                }
            }
            {
                XL6_AstNode* channelCtorType = xl6CgenChannelCtorType(ctx, expr);
                if ( channelCtorType != NULL ) {
                    return channelCtorType;
                }
            }
            {
                XL6_AstNode* instant = expr->children[0];
                XL6_AstNode* nameNode = instant->childCount > 0 ? instant->children[0] : NULL;
                XL6_AstNode* typeArgs = instant->childCount > 1 ? instant->children[1] : NULL;
                XL6_AstNode* func = NULL;

                if ( nameNode != NULL && nameNode->kind == XL6_AST_NAME ) {
                    func = xl6CgenResolveExplicitGenericCallFunc(ctx, expr, nameNode->text, typeArgs, 0);
                }
                if ( xl6CgenIsAsyncFunc(func) ) {
                    return xl6CgenCloneAsyncCallType(ctx, func, typeArgs, expr);
                }
                if ( func != NULL && func->childCount > 2 &&
                     func->children[2] != NULL && func->children[2]->childCount == 1 ) {
                    const XL6_AstNode* prevParams = ctx->currentGenericParams;
                    const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
                    XL6_AstNode* result;

                    /*
                        显式泛型调用已经给出完整类型实参，auto 推断时要用它们替换返回类型里的 T。
                        这里和普通泛型推断调用共用规范化类型克隆逻辑，确保嵌套类型也能继续访问成员。
                    */
                    ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
                    ctx->currentGenericArgs = typeArgs;
                    result = xl6CgenCloneCanonicalTypeNode(ctx, func->children[2]->children[0]);
                    ctx->currentGenericParams = prevParams;
                    ctx->currentGenericArgs = prevArgs;
                    return result;
                }
            }
        }
        if ( expr->childCount > 0 && expr->children[0] != NULL && expr->children[0]->kind == XL6_AST_NAME ) {
            if ( xl6CTextEquals(expr->children[0]->text, "type") ) {
                return xl6CgenNewSimpleType("type", expr);
            }
            if ( xl6CgenIsCopyBuiltinName(expr->children[0]->text) &&
                 !xl6CgenCopyBuiltinNameIsShadowed(ctx, expr->children[0]->text) &&
                 expr->childCount > 1 &&
                 expr->children[1] != NULL &&
                 expr->children[1]->childCount == 1 ) {
                const XL6_AstNode* argNode = expr->children[1]->children[0];
                const XL6_AstNode* arg = xl6CgenArgExpr(argNode);
                if ( argNode != NULL && argNode->kind != XL6_AST_NAMED_ARG ) {
                    return xl6CgenCloneExprTypeForInference(ctx, arg);
                }
            }
            if ( xl6CgenIsBuiltinConvertTypeName(expr->children[0]->text) &&
                 xl6CgenIsKnownTypeName(ctx, expr->children[0]->text) ) {
                return xl6CgenNewSimpleType(expr->children[0]->text, expr);
            }
            {
                XL6_AstNode* nativeClassCtor = NULL;
                XL6_AstNode* classCtor = NULL;
                if ( xl6CgenCallIsNativeBackedCtor(ctx, expr, &nativeClassCtor) && nativeClassCtor != NULL ) {
                    return xl6CgenNewSimpleType(nativeClassCtor->text, expr);
                }
                if ( xl6CgenCallIsClassCtor(ctx, expr, &classCtor) && classCtor != NULL ) {
                    return xl6CgenNewSimpleType(classCtor->text, expr);
                }
            }
            const XL6_AstNode* functionVarType = xl6CgenFindVarType(ctx, expr->children[0]->text);
            const XL6_AstNode* functionReturns = xl6CgenFunctionReturnTypes(functionVarType);
            if ( xl6CgenIsFunctionType(functionVarType) ) {
                if ( xl6CgenIsBareFunctionType(functionVarType) ) {
                    return xl6CgenNewSimpleType("var", expr);
                }
                if ( functionReturns != NULL && functionReturns->childCount == 1 ) {
                    return xl6CgenCloneTypeNode(functionReturns->children[0]);
                }
            }
            if ( functionVarType != NULL && xl6CTextEquals(functionVarType->text, "var") ) {
                return xl6CgenNewSimpleType("var", expr);
            }
            {
                XL6_AstNode* protocolType = xl6CgenCloneClassProtocolReturnTypeForCall(ctx, functionVarType, "__call__", expr);
                if ( protocolType != NULL ) {
                    return protocolType;
                }
            }
		}
		if ( expr->childCount > 0 && expr->children[0] != NULL && expr->children[0]->kind == XL6_AST_NAME ) {
			XL6_AstNode* inferredArgs = NULL;
			XL6_AstNode* func = xl6CgenResolveCallFuncInternal(
				ctx,
				expr,
				expr->children[0]->text,
				&inferredArgs,
				0
			);
			const XL6_AstNode* returns =
				func != NULL && func->childCount > 2 ? func->children[2] : NULL;
            if ( xl6CgenIsAsyncFunc(func) ) {
                XL6_AstNode* result = xl6CgenCloneAsyncCallType(ctx, func, inferredArgs, expr);
                xl6AstFree(inferredArgs);
                return result;
            }
            if ( returns != NULL && returns->childCount == 1 ) {
                XL6_AstNode* result;

                if ( inferredArgs != NULL ) {
                    const XL6_AstNode* prevParams = ctx->currentGenericParams;
                    const XL6_AstNode* prevArgs = ctx->currentGenericArgs;

                    ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
                    ctx->currentGenericArgs = inferredArgs;
                    result = xl6CgenCloneCanonicalTypeNode(ctx, returns->children[0]);
                    ctx->currentGenericParams = prevParams;
                    ctx->currentGenericArgs = prevArgs;
                    xl6AstFree(inferredArgs);
                    return result;
                }
                result = xl6CgenCloneTypeNode(returns->children[0]);
                xl6AstFree(inferredArgs);
                return result;
            }
            xl6AstFree(inferredArgs);
        }
        return NULL;
    default:
        return NULL;
    }
}
