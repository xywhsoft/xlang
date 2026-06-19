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
    const char* name;

    (void)ctx;
    name = typeNode != NULL ? typeNode->text : NULL;
    return xl6CTextEquals(name, "null") ||
           xl6CTextEquals(name, "var") ||
           xl6CTextEquals(name, "bool") ||
           xl6CgenIsIntegerTypeName(name) ||
           xl6CgenIsFloatTypeName(name) ||
           xl6CTextEquals(name, "string") ||
           xl6CTextEquals(name, "time") ||
           xl6CTextEquals(name, "point");
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

static XL6_AstNode* xl6CgenCloneCommonLiteralElementType(XL6_CgenContext* ctx, const XL6_AstNode* literal, int usePairValue)
{
    XL6_AstNode* common;
    size_t i;

    if ( literal == NULL || literal->childCount == 0 ) {
        return NULL;
    }

    common = NULL;
    for ( i = 0; i < literal->childCount; ++i ) {
        const XL6_AstNode* item = literal->children[i];
        const XL6_AstNode* value = usePairValue ?
            (item != NULL && item->childCount > 1 ? item->children[1] : NULL) :
            item;
        XL6_AstNode* valueType = xl6CgenCloneExprTypeForInference(ctx, value);
        XL6_AstNode* canonicalType;

        if ( valueType == NULL ) {
            xl6AstFree(common);
            return NULL;
        }
        canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, valueType);
        xl6AstFree(valueType);
        if ( canonicalType == NULL ) {
            xl6AstFree(common);
            return NULL;
        }

        if ( common == NULL ) {
            common = canonicalType;
        } else if ( xl6CgenTypeNodeEqual(common, canonicalType) ) {
            xl6AstFree(canonicalType);
        } else {
            xl6AstFree(common);
            xl6AstFree(canonicalType);
            return NULL;
        }
    }
    return common;
}

static const char* xl6CgenContainerLiteralName(const XL6_AstNode* literal)
{
    if ( literal == NULL ) {
        return NULL;
    }
    switch ( literal->kind ) {
    case XL6_AST_ARRAY_LITERAL:
        return "array";
    case XL6_AST_LIST_LITERAL:
        return "list";
    case XL6_AST_SET_LITERAL:
        return "set";
    case XL6_AST_DICT_LITERAL:
        return "dict";
    default:
        return NULL;
    }
}

static XL6_AstNode* xl6CgenCloneContainerLiteralType(
    XL6_CgenContext* ctx,
    const char* forcedName,
    const XL6_AstNode* literal,
    const XL6_AstNode* at)
{
    const char* name;
    int usePairValue;
    XL6_AstNode* elementType;

    name = forcedName != NULL ? forcedName : xl6CgenContainerLiteralName(literal);
    if ( name == NULL ) {
        return NULL;
    }

    if ( literal == NULL || literal->childCount == 0 ) {
        return xl6CgenNewSimpleType(name, at);
    }

    usePairValue = xl6CTextEquals(name, "list") || xl6CTextEquals(name, "dict");
    elementType = xl6CgenCloneCommonLiteralElementType(ctx, literal, usePairValue);
    if ( elementType == NULL ) {
        return xl6CgenNewSimpleType(name, at);
    }
    return xl6CgenNewGenericTypeNode(name, elementType, at);
}

static XL6_AstNode* xl6CgenNewFunctionTypeFromParts(const XL6_AstNode* params, const XL6_AstNode* returns, const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;
    XL6_AstNode* paramTypes;
    XL6_AstNode* returnTypes;
    size_t i;

    typeNode = xl6CgenNewSimpleType("function", at);
    paramTypes = xl6AstNew(XL6_AST_LIST, NULL);
    returnTypes = returns != NULL ? xl6CgenCloneTypeNode(returns) : xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    if ( typeNode == NULL || paramTypes == NULL || returnTypes == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(paramTypes);
        xl6AstFree(returnTypes);
        return NULL;
    }

    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            XL6_AstNode* typeCopy;
            const XL6_AstNode* param = params->children[i];
            if ( param == NULL || param->childCount == 0 ) {
                continue;
            }
            typeCopy = xl6CgenCloneTypeNode(param->children[0]);
            if ( typeCopy == NULL ) {
                xl6AstFree(typeNode);
                xl6AstFree(paramTypes);
                xl6AstFree(returnTypes);
                return NULL;
            }
            xl6AstAddChild(paramTypes, typeCopy);
        }
    }

    xl6AstAddChild(typeNode, paramTypes);
    xl6AstAddChild(typeNode, returnTypes);
    return typeNode;
}

static XL6_AstNode* xl6CgenAnonFunctionType(const XL6_AstNode* anon)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    if ( anon == NULL || anon->kind != XL6_AST_ANON_FUNC ) {
        return NULL;
    }
    params = anon->childCount > 0 ? anon->children[0] : NULL;
    returns = anon->childCount > 1 ? anon->children[1] : NULL;
    return xl6CgenNewFunctionTypeFromParts(params, returns, anon);
}

static XL6_AstNode* xl6CgenGenericClassCtorType(XL6_CgenContext* ctx, const XL6_AstNode* call, XL6_AstNode** outClass)
{
    const XL6_AstNode* instant;
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArgs;
    XL6_AstNode* classNode;
    XL6_AstNode* typeNode;
    XL6_AstNode* argsCopy;
    const XL6_AstNode* classParams;

    if ( outClass != NULL ) {
        *outClass = NULL;
    }
    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return NULL;
    }
    instant = call->children[0];
    if ( instant == NULL || instant->kind != XL6_AST_GENERIC_INSTANT || instant->childCount < 2 ) {
        return NULL;
    }
    nameNode = instant->children[0];
    typeArgs = instant->children[1];
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME || typeArgs == NULL ) {
        return NULL;
    }

    classNode = xl6CgenFindClass(ctx, nameNode->text);
    if ( classNode == NULL || !xl6CgenClassHasTypeParams(classNode) ) {
        return NULL;
    }
    classParams = xl6CgenClassTypeParams(classNode);
    if ( classParams == NULL || classParams->childCount != typeArgs->childCount ) {
        xl6CgenError(ctx, call, "泛型 class 构造类型参数数量不匹配");
        return NULL;
    }

    typeNode = xl6CgenNewSimpleType(nameNode->text, nameNode);
    argsCopy = xl6CgenCloneCanonicalTypeNode(ctx, typeArgs);
    if ( typeNode == NULL || argsCopy == NULL ) {
        xl6AstFree(typeNode);
        xl6AstFree(argsCopy);
        xl6CgenError(ctx, call, "泛型 class 构造类型节点内存不足");
        return NULL;
    }
    xl6AstAddChild(typeNode, argsCopy);
    if ( outClass != NULL ) {
        *outClass = classNode;
    }
    return typeNode;
}

static XL6_AstNode* xl6CgenCloneExprTypeForInference(XL6_CgenContext* ctx, const XL6_AstNode* expr)
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
            return xl6CgenCloneContainerLiteralType(
                ctx,
                expr->text,
                expr->childCount > 0 ? expr->children[0] : NULL,
                expr);
        }
        return xl6CgenNewSimpleType(expr->text != NULL ? expr->text : "var", expr);
    case XL6_AST_ANON_FUNC:
        return xl6CgenAnonFunctionType(expr);
    case XL6_AST_NAME:
        varType = xl6CgenFindVarType(ctx, expr->text);
        if ( varType != NULL && !xl6CTextEquals(varType->text, "auto") ) {
            return xl6CgenCloneTypeNode(varType);
        }
        if ( xl6CgenIsKnownTypeName(ctx, expr->text) ) {
            return xl6CgenNewSimpleType("type", expr);
        }
        return NULL;
    case XL6_AST_BINARY:
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
        if ( xl6CTextEquals(expr->text, "+") ) {
            XL6_AstNode* leftType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
            XL6_AstNode* rightType = expr->childCount > 1 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[1]) : NULL;
            int isString = leftType != NULL && rightType != NULL &&
                           xl6CTextEquals(leftType->text, "string") &&
                           xl6CTextEquals(rightType->text, "string");
            xl6AstFree(leftType);
            xl6AstFree(rightType);
            if ( isString ) {
                return xl6CgenNewSimpleType("string", expr);
            }
        }
        if ( xl6CTextEquals(expr->text, "**") && xl6CgenExprIntPowKind(ctx, expr) ) {
            return xl6CgenNewSimpleType("int", expr);
        }
        return NULL;
    case XL6_AST_MEMBER:
        {
            XL6_AstNode* fieldType = xl6CgenCloneMemberFieldTypeForInference(ctx, expr);
            if ( fieldType != NULL ) {
                return fieldType;
            }
        }
        return NULL;
    case XL6_AST_SAFE_MEMBER:
        receiverType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
        if ( expr->text != NULL && xl6CTextEquals(expr->text, "length") && xl6CgenIsXValueType(receiverType) ) {
            xl6AstFree(receiverType);
            return xl6CgenNewSimpleType("var", expr);
        }
        xl6AstFree(receiverType);
        return NULL;
    case XL6_AST_INDEX:
    case XL6_AST_SAFE_INDEX:
        receiverType = expr->childCount > 0 ? xl6CgenCloneExprTypeForInference(ctx, expr->children[0]) : NULL;
        if ( expr->childCount > 0 &&
             xl6CgenExprUsesTypedContainerStorage(ctx, expr->children[0]) &&
             xl6CgenIsTypedContainerType(ctx, receiverType) ) {
            const XL6_AstNode* concreteType = xl6CgenTypedContainerConcreteType(ctx, receiverType);
            const XL6_AstNode* itemType = xl6CgenTypedContainerItemType(ctx, receiverType);
            XL6_AstNode* result = NULL;

            if ( concreteType != NULL && !xl6CTextEquals(concreteType->text, "set") && itemType != NULL ) {
                result = xl6CgenCloneCanonicalTypeNode(ctx, itemType);
            }
            xl6AstFree(receiverType);
            return result;
        }
        if ( xl6CgenIsArrayXValueType(receiverType) || xl6CgenIsListXValueType(receiverType) ||
             xl6CgenIsDictXValueType(receiverType) ||
             (expr->kind == XL6_AST_SAFE_INDEX && receiverType != NULL && xl6CTextEquals(receiverType->text, "var")) ) {
            xl6AstFree(receiverType);
            return xl6CgenNewSimpleType("var", expr);
        }
        xl6AstFree(receiverType);
        return NULL;
    case XL6_AST_CALL:
        if ( xl6CgenIsImplicitToStringCall(ctx, expr) ) {
            return xl6CgenNewSimpleType("string", expr);
        }
        if ( expr->childCount > 0 && expr->children[0] != NULL &&
             expr->children[0]->kind == XL6_AST_GENERIC_INSTANT ) {
            XL6_AstNode* genericClassType = xl6CgenGenericClassCtorType(ctx, expr, NULL);
            if ( genericClassType != NULL ) {
                return genericClassType;
            }
            {
                XL6_AstNode* instant = expr->children[0];
                XL6_AstNode* nameNode = instant->childCount > 0 ? instant->children[0] : NULL;
                XL6_AstNode* typeArgs = instant->childCount > 1 ? instant->children[1] : NULL;
                XL6_AstNode* func = NULL;

                if ( nameNode != NULL && nameNode->kind == XL6_AST_NAME ) {
                    func = xl6CgenResolveExplicitGenericCallFunc(ctx, expr, nameNode->text, typeArgs, 0);
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
            if ( xl6CgenIsBuiltinConvertTypeName(expr->children[0]->text) &&
                 xl6CgenIsKnownTypeName(ctx, expr->children[0]->text) ) {
                return xl6CgenNewSimpleType(expr->children[0]->text, expr);
            }
            {
                XL6_AstNode* customCtor = NULL;
                XL6_AstNode* classCtor = NULL;
                if ( xl6CgenCallIsCustomCtor(ctx, expr, &customCtor) && customCtor != NULL ) {
                    return xl6CgenNewSimpleType(customCtor->text, expr);
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
        }
        if ( expr->childCount > 0 && expr->children[0] != NULL && expr->children[0]->kind == XL6_AST_NAME ) {
            XL6_AstNode* func = xl6CgenFindExternFunc(ctx, expr->children[0]->text);
            XL6_AstNode* inferredArgs = NULL;
            const XL6_AstNode* returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
            if ( returns == NULL ) {
                func = xl6CgenResolveCallFuncInternal(ctx, expr, expr->children[0]->text, &inferredArgs, 0);
                returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
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

