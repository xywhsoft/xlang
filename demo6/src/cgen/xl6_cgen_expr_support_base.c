static void xl6CgenExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);

static XL6_AstNode* xl6CgenClassHandleTypeForClassType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classType)
{
    XL6_AstNode* handleType;
    XL6_AstNode* args;
    XL6_AstNode* target;

    target = xl6CgenCloneCanonicalTypeNode(ctx, classType);
    handleType = xl6AstNewText(XL6_AST_TYPE, "__class_handle", 14, NULL);
    args = xl6AstNew(XL6_AST_LIST, NULL);
    if ( target == NULL || handleType == NULL || args == NULL ) {
        xl6AstFree(target);
        xl6AstFree(handleType);
        xl6AstFree(args);
        return NULL;
    }
    handleType->flags |= XL6_AST_FLAG_CLASS_HANDLE;
    xl6AstAddChild(args, target);
    xl6AstAddChild(handleType, args);
    return handleType;
}

static int xl6CgenIsXValueType(const XL6_AstNode* typeNode)
{
    if ( xl6CgenNullableUsesXValue(typeNode) ) {
        return 1;
    }
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "var") ||
            xl6CTextEquals(typeNode->text, "__class_handle") ||
            (typeNode->flags & XL6_AST_FLAG_PROTOCOL_HANDLE) != 0 ||
            xl6CgenIsArrayXValueType(typeNode) ||
            xl6CgenIsListXValueType(typeNode) ||
            xl6CgenIsSetXValueType(typeNode) ||
            xl6CgenIsDictXValueType(typeNode));
}

static const char* xl6CgenValueStorageCType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    if ( xl6CgenIsTypedContainerType(ctx, concreteType) ) {
        return xl6CgenTypedContainerCType(ctx, concreteType);
    }
    return xl6CgenTypeName(ctx, concreteType != NULL ? concreteType : typeNode);
}

static const char* xl6CgenXValueRuntimeTypeConst(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CgenIsBoolTypeName(typeName) ) {
        return "XL6_XVO_DT_BOOL";
    }
    if ( xl6CgenIsIntegerTypeName(typeName) ) {
        return "XL6_XVO_DT_INT";
    }
    if ( xl6CgenIsFloatTypeName(typeName) ) {
        return "XL6_XVO_DT_FLOAT";
    }
    if ( xl6CTextEquals(typeName, "string") ) {
        return "XL6_XVO_DT_TEXT";
    }
    if ( xl6CTextEquals(typeName, "time") ) {
        return "XL6_XVO_DT_TIME";
    }
    if ( xl6CTextEquals(typeName, "point") ) {
        return "XL6_XVO_DT_POINT";
    }
    if ( xl6CgenIsArrayXValueType(concreteType) ) {
        return "XL6_XVO_DT_ARRAY";
    }
    if ( xl6CgenIsListXValueType(concreteType) ) {
        return "XL6_XVO_DT_LIST";
    }
    if ( xl6CgenIsSetXValueType(concreteType) ) {
        return "XL6_XVO_DT_COLL";
    }
    if ( xl6CgenIsDictXValueType(concreteType) ) {
        return "XL6_XVO_DT_TABLE";
    }
    return NULL;
}

static int xl6CgenTypeSupportsVarInvokeValue(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    if ( concreteType == NULL || concreteType->text == NULL ) {
        return 0;
    }
    if ( xl6CTextEquals(concreteType->text, "var") ) {
        return 1;
    }
    if ( xl6CTextEquals(concreteType->text, "error") ) {
        return 1;
    }
    /* class 的静态存储和 XCall 表示均为同一个受管理 xvalue* 句柄。 */
    if ( xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
        return 1;
    }
    /*
        专用数据结构通过 XRT typed handle 进入 xvalue*。
        它们不是 array/list 这类普通 typed container，动态调用时使用运行时类型描述符校验。
    */
    if ( xl6CgenIsSpecialCollectionType(ctx, concreteType) ) {
        return 1;
    }
    if ( xl6CgenIsTypedContainerType(ctx, concreteType) ) {
        return 0;
    }
    return xl6CgenXValueRuntimeTypeConst(ctx, concreteType) != NULL;
}

static int xl6CgenAnonSupportsVarInvoke(XL6_CgenContext* ctx, const XL6_CgenAnonFunc* info)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int fixedCount;
    size_t i;

    if ( ctx == NULL || info == NULL || info->node == NULL ) {
        return 0;
    }

    params = info->node->childCount > 0 ? info->node->children[0] : NULL;
    returns = info->node->childCount > 1 ? info->node->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    if ( params != NULL && params->childCount > (size_t)fixedCount &&
         !xl6CgenParamIsArrayVararg(params->children[fixedCount]) ) {
        return 0;
    }
    if ( returns != NULL && returns->childCount > 1 ) {
        return 0;
    }
    if ( returns != NULL && returns->childCount == 1 &&
         !xl6CgenTypeSupportsVarInvokeValue(ctx, returns->children[0]) ) {
        return 0;
    }
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( param == NULL || param->childCount == 0 ||
                 !xl6CgenTypeSupportsVarInvokeValue(ctx, param->children[0]) ) {
                return 0;
            }
        }
    }
    return 1;
}

static const XL6_AstNode* xl6CgenFindDirectClassFieldNode(const XL6_AstNode* classNode, const char* fieldName)
{
    const XL6_AstNode* body;
    size_t i;

    if ( classNode == NULL || fieldName == NULL ) {
        return NULL;
    }
    body = xl6CgenClassBody(classNode);
    if ( body == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( xl6CgenIsInstanceField(member) && xl6CTextEquals(member->text, fieldName) ) {
            return member;
        }
        if ( member != NULL && member->kind == XL6_AST_UNION && member->text == NULL ) {
            const XL6_AstNode* field = xl6CgenFindUnionFieldNode(member, fieldName);
            if ( field != NULL ) {
                return field;
            }
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindClassFieldNode(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* fieldName)
{
    const XL6_AstNode* field;
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;

    field = xl6CgenFindDirectClassFieldNode(classNode, fieldName);
    if ( field != NULL ) {
        return field;
    }

    base = classNode != NULL && classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return NULL;
    }
    baseClass = xl6CgenFindClass(ctx, base->text);
    return xl6CgenFindClassFieldNode(ctx, baseClass, fieldName);
}

static const XL6_AstNode* xl6CgenFindImplicitSelfFieldNode(XL6_CgenContext* ctx, const char* fieldName)
{
    if ( ctx == NULL || fieldName == NULL || ctx->currentClass == NULL ||
         xl6CgenFindTrackedVarType(ctx, fieldName) != NULL ||
         xl6CTextEquals(fieldName, "self") ||
         xl6CTextEquals(fieldName, "base") ) {
        return NULL;
    }
    return xl6CgenFindClassFieldNode(ctx, ctx->currentClass, fieldName);
}

static int xl6CgenImplicitSelfFieldPathRec(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* fieldName,
    char* buffer,
    size_t capacity,
    const char* sep)
{
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;
    char cField[256];
    size_t len;

    if ( ctx == NULL || classNode == NULL || fieldName == NULL ||
         buffer == NULL || capacity == 0 || sep == NULL ) {
        return 0;
    }
    if ( xl6CgenFindDirectClassFieldNode(classNode, fieldName) != NULL ) {
        xl6CgenCIdentText(cField, sizeof(cField), fieldName);
        len = strlen(buffer);
        return snprintf(buffer + len, capacity - len, "%s%s", sep, cField) > 0;
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return 0;
    }
    baseClass = xl6CgenFindClass(ctx, base->text);
    if ( baseClass == NULL ) {
        return 0;
    }
    len = strlen(buffer);
    if ( snprintf(buffer + len, capacity - len, "->base") <= 0 ) {
        return 0;
    }
    return xl6CgenImplicitSelfFieldPathRec(ctx, baseClass, fieldName, buffer, capacity, ".");
}

static int xl6CgenImplicitSelfFieldPath(XL6_CgenContext* ctx, const char* fieldName, char* buffer, size_t capacity)
{
    if ( buffer == NULL || capacity == 0 ) {
        return 0;
    }
    buffer[0] = '\0';
    if ( xl6CgenFindImplicitSelfFieldNode(ctx, fieldName) == NULL ) {
        return 0;
    }
    if ( snprintf(buffer, capacity, "self") <= 0 ) {
        return 0;
    }
    return xl6CgenImplicitSelfFieldPathRec(ctx, ctx->currentClass, fieldName, buffer, capacity, "->");
}

static int xl6CgenEmitImplicitSelfFieldReference(XL6_CgenContext* ctx, const char* fieldName)
{
    char path[512];

    if ( !xl6CgenImplicitSelfFieldPath(ctx, fieldName, path, sizeof(path)) ) {
        return 0;
    }
    xl6OutputPuts(path, ctx->out);
    return 1;
}

static XL6_AstNode* xl6CgenCloneImplicitSelfFieldType(XL6_CgenContext* ctx, const char* fieldName)
{
    const XL6_AstNode* field;

    field = xl6CgenFindImplicitSelfFieldNode(ctx, fieldName);
    return field != NULL && field->childCount > 0 ? xl6CgenCloneCanonicalTypeNode(ctx, field->children[0]) : NULL;
}

static const XL6_AstNode* xl6CgenFindClassProtocolFunc(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* protocolName,
    const XL6_AstNode** outOwnerClass)
{
    const XL6_AstNode* body;
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;
    size_t i;

    if ( outOwnerClass != NULL ) {
        *outOwnerClass = NULL;
    }
    if ( ctx == NULL || classNode == NULL || protocolName == NULL ) {
        return NULL;
    }

    body = xl6CgenClassBody(classNode);
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            const XL6_AstNode* nameNode;

            if ( member == NULL || member->kind != XL6_AST_FUNC || member->childCount == 0 ) {
                continue;
            }
            nameNode = member->children[0];
            if ( nameNode != NULL && xl6CTextEquals(nameNode->text, protocolName) ) {
                if ( outOwnerClass != NULL ) {
                    *outOwnerClass = classNode;
                }
                return member;
            }
        }
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return NULL;
    }
    baseClass = xl6CgenFindClass(ctx, base->text);
    return xl6CgenFindClassProtocolFunc(ctx, baseClass, protocolName, outOwnerClass);
}

static XL6_AstNode* xl6CgenCloneClassProtocolReturnType(XL6_CgenContext* ctx, const XL6_AstNode* receiverType, const char* protocolName)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* classNode;
    const XL6_AstNode* method;
    const XL6_AstNode* returns;

    concreteType = xl6CgenResolveAliasType(ctx, receiverType);
    if ( concreteType == NULL || concreteType->text == NULL || protocolName == NULL ) {
        return NULL;
    }

    classNode = xl6CgenFindClass(ctx, concreteType->text);
    if ( classNode == NULL ) {
        return NULL;
    }

    method = xl6CgenFindClassProtocolFunc(ctx, classNode, protocolName, NULL);
    returns = method != NULL && method->childCount > 2 ? method->children[2] : NULL;
    if ( returns == NULL || returns->childCount != 1 ) {
        return NULL;
    }
    return xl6CgenCloneCanonicalTypeNode(ctx, returns->children[0]);
}

static XL6_AstNode* xl6CgenCloneMemberFieldTypeForInference(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* classType;
    const XL6_AstNode* field;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    XL6_AstNode* classNode;
    XL6_AstNode* result;
    XL6_AstNode* ownedReceiverType;

    if ( ctx == NULL || member == NULL ||
         (member->kind != XL6_AST_MEMBER && member->kind != XL6_AST_SAFE_MEMBER) ) {
        return NULL;
    }

    result = xl6CgenCloneStaticClassFieldType(ctx, member);
    if ( result != NULL ) {
        return result;
    }

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || (receiver->kind != XL6_AST_NAME &&
                              receiver->kind != XL6_AST_MEMBER &&
                              receiver->kind != XL6_AST_SAFE_MEMBER &&
                              receiver->kind != XL6_AST_INDEX &&
                              receiver->kind != XL6_AST_SAFE_INDEX &&
                              receiver->kind != XL6_AST_CALL) ) {
        return NULL;
    }

    receiverType = NULL;
    classType = NULL;
    ownedReceiverType = NULL;
    if ( receiver->kind == XL6_AST_MEMBER || receiver->kind == XL6_AST_SAFE_MEMBER ||
         receiver->kind == XL6_AST_INDEX || receiver->kind == XL6_AST_SAFE_INDEX ||
         receiver->kind == XL6_AST_CALL ) {
        receiverType = receiver->kind == XL6_AST_MEMBER
            ? xl6CgenCloneMemberFieldTypeForInference(ctx, receiver)
            : xl6CgenCloneExprTypeForInference(ctx, receiver);
        if ( member->kind == XL6_AST_SAFE_MEMBER &&
             xl6CgenIsNullableType(receiverType) ) {
            XL6_AstNode* effectiveType = xl6CgenCloneCanonicalTypeNode(
                ctx,
                xl6CgenNullableInnerType(receiverType));
            xl6AstFree((XL6_AstNode*)receiverType);
            receiverType = effectiveType;
        }
        if ( receiverType != NULL && xl6CTextEquals(receiverType->text, "var") ) {
            result = xl6AstNewText(XL6_AST_TYPE, "var", 3, NULL);
            xl6AstFree((XL6_AstNode*)receiverType);
            return result;
        }
        if ( receiverType != NULL && xl6CgenIsPlainRecordType(ctx, receiverType) ) {
            field = xl6CgenFindPlainRecordFieldNode(ctx, receiverType, member->text);
            result = field != NULL && field->childCount > 0 ? xl6CgenCloneCanonicalTypeNode(ctx, field->children[0]) : NULL;
            xl6AstFree((XL6_AstNode*)receiverType);
            return result;
        }
        classType = xl6CgenClassHandleTargetType(ctx, receiverType);
        if ( classType == NULL ) {
            classType = receiverType;
        }
        classNode = classType != NULL ? xl6CgenFindClass(ctx, classType->text) : NULL;
        if ( classNode != NULL ) {
            field = xl6CgenFindClassFieldNode(ctx, classNode, member->text);
            if ( field == NULL || field->childCount == 0 ) {
                if ( xl6CTextEquals(member->text, "length") ) {
                    result = xl6CgenCloneClassProtocolReturnType(ctx, receiverType, "__len__");
                    if ( result != NULL ) {
                        xl6AstFree((XL6_AstNode*)receiverType);
                        return result;
                    }
                }
                if ( (result = xl6CgenCloneClassPropertyMemberType(ctx, member, NULL)) != NULL ) { xl6AstFree((XL6_AstNode*)receiverType); return result; }
                result = xl6CgenCloneClassProtocolReturnType(ctx, receiverType, "__field__");
                xl6AstFree((XL6_AstNode*)receiverType);
                return result;
            }

            prevParams = ctx->currentGenericParams;
            prevArgs = ctx->currentGenericArgs;
            if ( classType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
                ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
                ctx->currentGenericArgs = classType->children[0];
            }

            result = xl6CgenCloneCanonicalTypeNode(ctx, field->children[0]);

            ctx->currentGenericParams = prevParams;
            ctx->currentGenericArgs = prevArgs;
            xl6AstFree((XL6_AstNode*)receiverType);
            return result;
        }
        xl6AstFree((XL6_AstNode*)receiverType);
        return NULL;
    }

    if ( xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        classNode = (XL6_AstNode*)ctx->currentClass;
    } else {
        ownedReceiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
        receiverType = ownedReceiverType;
        if ( member->kind == XL6_AST_SAFE_MEMBER &&
             xl6CgenIsNullableType(receiverType) ) {
            ownedReceiverType = xl6CgenCloneCanonicalTypeNode(
                ctx,
                xl6CgenNullableInnerType(receiverType));
            xl6AstFree((XL6_AstNode*)receiverType);
            receiverType = ownedReceiverType;
        }
        if ( receiverType != NULL && xl6CTextEquals(receiverType->text, "var") ) {
            result = xl6AstNewText(XL6_AST_TYPE, "var", 3, NULL);
            xl6AstFree((XL6_AstNode*)ownedReceiverType);
            return result;
        }
        classType = xl6CgenClassHandleTargetType(ctx, receiverType);
        if ( classType == NULL ) {
            classType = receiverType;
        }
        classNode = classType != NULL ? xl6CgenFindClass(ctx, classType->text) : NULL;
    }
    if ( receiverType != NULL && xl6CgenIsPlainRecordType(ctx, receiverType) ) {
        field = xl6CgenFindPlainRecordFieldNode(ctx, receiverType, member->text);
        result = field != NULL && field->childCount > 0 ? xl6CgenCloneCanonicalTypeNode(ctx, field->children[0]) : NULL;
        xl6AstFree((XL6_AstNode*)ownedReceiverType);
        return result;
    }
    if ( classNode == NULL ) {
        xl6AstFree((XL6_AstNode*)ownedReceiverType);
        return NULL;
    }

    field = xl6CgenFindClassFieldNode(ctx, classNode, member->text);
    if ( field == NULL || field->childCount == 0 ) {
        if ( xl6CTextEquals(member->text, "length") ) {
            result = xl6CgenCloneClassProtocolReturnType(ctx, receiverType != NULL ? receiverType : classNode, "__len__");
            if ( result != NULL ) {
                xl6AstFree((XL6_AstNode*)ownedReceiverType);
                return result;
            }
        }
        if ( (result = xl6CgenCloneClassPropertyMemberType(ctx, member, NULL)) != NULL ) {
            xl6AstFree((XL6_AstNode*)ownedReceiverType);
            return result;
        }
        result = xl6CgenCloneClassProtocolReturnType(ctx, receiverType != NULL ? receiverType : classNode, "__field__");
        xl6AstFree((XL6_AstNode*)ownedReceiverType);
        return result;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( classType != NULL && classType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = classType->children[0];
    }

    result = xl6CgenCloneCanonicalTypeNode(ctx, field->children[0]);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    xl6AstFree((XL6_AstNode*)ownedReceiverType);
    return result;
}

static int xl6CgenClassNeedsDestroy(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;
    size_t i;

    if ( classNode == NULL ) {
        return 0;
    }
    if ( xl6CgenClassHasDestroy(classNode) ) {
        return 1;
    }
    if ( xl6CgenClassHasLocalVirtualSlots(classNode) ) {
        return 1;
    }

    body = xl6CgenClassBody(classNode);
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            const XL6_AstNode* typeNode;
            XL6_AstNode* nativeClassNode;
            XL6_AstNode* nestedClass;

            if ( !xl6CgenIsInstanceField(member) || member->childCount == 0 ) {
                continue;
            }
            typeNode = member->children[0];
            if ( xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
                return 1;
            }
            if ( xl6CgenIsErrorType(ctx, typeNode) ) {
                return 1;
            }
            if ( xl6CgenIsFunctionType(typeNode) ) {
                return 1;
            }
            if ( xl6CgenNullableHasManagedInner(ctx, typeNode) ) {
                return 1;
            }
            if ( xl6CgenIsWeakType(typeNode) ) {
                return 1;
            }
            if ( xl6CgenIsXValueType(typeNode) ) {
                return 1;
            }
            if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
                return 1;
            }
            nativeClassNode = xl6CgenFindNativeBackedClass(ctx, typeNode != NULL ? typeNode->text : NULL);
            if ( nativeClassNode != NULL && xl6CgenNativeBackedClassHasDestroy(nativeClassNode) ) {
                return 1;
            }
            nestedClass = xl6CgenFindClass(ctx, typeNode != NULL ? typeNode->text : NULL);
            if ( nestedClass != NULL && xl6CgenClassNeedsDestroy(ctx, nestedClass) ) {
                return 1;
            }
        }
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        baseClass = xl6CgenFindClass(ctx, base->text);
        if ( xl6CgenClassNeedsDestroy(ctx, baseClass) ) {
            return 1;
        }
    }
    return 0;
}

#include "xl6_cgen_expr_support_lifecycle.c"
