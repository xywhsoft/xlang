static void xl6CgenExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);

static int xl6CgenIsXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "var") ||
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

    if ( xl6CTextEquals(typeName, "bool") ) {
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
    const XL6_AstNode* field;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    XL6_AstNode* classNode;
    XL6_AstNode* result;
    XL6_AstNode* ownedReceiverType;

    if ( ctx == NULL || member == NULL || member->kind != XL6_AST_MEMBER ) {
        return NULL;
    }

    result = xl6CgenCloneStaticClassFieldType(ctx, member);
    if ( result != NULL ) {
        return result;
    }

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || (receiver->kind != XL6_AST_NAME &&
                              receiver->kind != XL6_AST_MEMBER &&
                              receiver->kind != XL6_AST_INDEX &&
                              receiver->kind != XL6_AST_CALL) ) {
        return NULL;
    }

    receiverType = NULL;
    ownedReceiverType = NULL;
    if ( receiver->kind == XL6_AST_MEMBER || receiver->kind == XL6_AST_INDEX || receiver->kind == XL6_AST_CALL ) {
        receiverType = receiver->kind == XL6_AST_MEMBER
            ? xl6CgenCloneMemberFieldTypeForInference(ctx, receiver)
            : xl6CgenCloneExprTypeForInference(ctx, receiver);
        if ( receiverType != NULL && xl6CgenIsUnionType(ctx, receiverType) ) {
            field = xl6CgenFindUnionFieldNode(xl6CgenFindUnion(ctx, receiverType->text), member->text);
            result = field != NULL && field->childCount > 0 ? xl6CgenCloneCanonicalTypeNode(ctx, field->children[0]) : NULL;
            xl6AstFree((XL6_AstNode*)receiverType);
            return result;
        }
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
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
            if ( receiverType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
                ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
                ctx->currentGenericArgs = receiverType->children[0];
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
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    }
    if ( receiverType != NULL && xl6CgenIsUnionType(ctx, receiverType) ) {
        field = xl6CgenFindUnionFieldNode(xl6CgenFindUnion(ctx, receiverType->text), member->text);
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
    if ( receiverType != NULL && receiverType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = receiverType->children[0];
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

    body = xl6CgenClassBody(classNode);
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            const XL6_AstNode* typeNode;
            XL6_AstNode* customNode;
            XL6_AstNode* nestedClass;

            if ( !xl6CgenIsInstanceField(member) || member->childCount == 0 ) {
                continue;
            }
            typeNode = member->children[0];
            if ( xl6CgenIsFunctionType(typeNode) ) {
                return 1;
            }
            if ( xl6CgenIsXValueType(typeNode) ) {
                return 1;
            }
            if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
                return 1;
            }
            customNode = xl6CgenFindNativeBackedClass(ctx, typeNode != NULL ? typeNode->text : NULL);
            if ( customNode != NULL && xl6CgenNativeBackedClassHasDestroy(customNode) ) {
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

static void xl6CgenReleaseFunctionPath(XL6_CgenContext* ctx, const char* path)
{
    if ( path == NULL ) {
        return;
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s.release != 0 && %s.env != 0) {\n", path, path);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.release(%s.env);\n", path, path);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.env = 0;\n", path);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.release = 0;\n", path);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenRetainFunctionPath(XL6_CgenContext* ctx, const char* path)
{
    if ( path == NULL ) {
        return;
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s.retain != 0 && %s.env != 0) {\n", path, path);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s.retain(%s.env);\n", path, path);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static const char* xl6CgenTypedContainerCopyFuncName(const char* typeName)
{
    if ( xl6CTextEquals(typeName, "array") ) {
        return "xl6_xrt_typed_array_copy_mode";
    }
    if ( xl6CTextEquals(typeName, "list") ) {
        return "xl6_xrt_typed_list_copy_mode";
    }
    if ( xl6CTextEquals(typeName, "set") ) {
        return "xl6_xrt_typed_set_copy_mode";
    }
    if ( xl6CTextEquals(typeName, "dict") ) {
        return "xl6_xrt_typed_dict_copy_mode";
    }
    return NULL;
}

static const char* xl6CgenTypedContainerDestroyFuncName(const char* typeName)
{
    if ( xl6CTextEquals(typeName, "array") ) {
        return "xrtTypedArrayDestroy";
    }
    if ( xl6CTextEquals(typeName, "list") ) {
        return "xrtTypedListDestroy";
    }
    if ( xl6CTextEquals(typeName, "set") ) {
        return "xrtTypedSetDestroy";
    }
    if ( xl6CTextEquals(typeName, "dict") ) {
        return "xrtTypedDictDestroy";
    }
    return NULL;
}

static void xl6CgenTypedContainerFieldRef(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* fieldPath,
    int retain)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;
    const char* funcName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( typeName == NULL || fieldPath == NULL ) {
        return;
    }

    xl6CgenIndent(ctx);
    if ( retain ) {
        funcName = xl6CgenTypedContainerCopyFuncName(typeName);
        if ( funcName == NULL ) {
            xl6CgenError(ctx, typeNode, "typed container copy requires array/list/set/dict");
            return;
        }
        xl6OutputPuts(fieldPath, ctx->out);
        xl6OutputPuts(" = ", ctx->out);
        xl6OutputPuts(funcName, ctx->out);
        xl6OutputPutc('(', ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPuts(fieldPath, ctx->out);
        xl6OutputPuts(", XRT_OBJMODE_LOCAL);\n", ctx->out);
        return;
    }

    funcName = xl6CgenTypedContainerDestroyFuncName(typeName);
    if ( funcName == NULL ) {
        xl6CgenError(ctx, typeNode, "typed container destroy requires array/list/set/dict");
        return;
    }
    xl6OutputPuts("if (", ctx->out);
    xl6OutputPuts(fieldPath, ctx->out);
    xl6OutputPuts(" != 0) { ", ctx->out);
    xl6OutputPuts(funcName, ctx->out);
    xl6OutputPutc('(', ctx->out);
    xl6OutputPuts(fieldPath, ctx->out);
    xl6OutputPuts("); ", ctx->out);
    xl6OutputPuts(fieldPath, ctx->out);
    xl6OutputPuts(" = 0; }\n", ctx->out);
}

static void xl6CgenClassFunctionFieldRefs(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* valuePath,
    int retain,
    int deep)
{
    const XL6_AstNode* body;
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;
    size_t i;

    if ( ctx == NULL || classNode == NULL || valuePath == NULL ) {
        return;
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        char basePath[512];

        baseClass = xl6CgenFindClass(ctx, base->text);
        snprintf(basePath, sizeof(basePath), "%s.base", valuePath);
        xl6CgenClassFunctionFieldRefs(ctx, baseClass, basePath, retain, deep);
    }

    body = xl6CgenClassBody(classNode);
    if ( body == NULL ) {
        return;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* typeNode;
        XL6_AstNode* nestedClass;
        char cFieldName[512];
        char fieldPath[512];

        if ( !xl6CgenIsInstanceField(member) || member->childCount == 0 || member->text == NULL ) {
            continue;
        }
        typeNode = member->children[0];
        xl6CgenCIdentText(cFieldName, sizeof(cFieldName), member->text);
        snprintf(fieldPath, sizeof(fieldPath), "%s.%s", valuePath, cFieldName);
        if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
            xl6CgenTypedContainerFieldRef(ctx, typeNode, fieldPath, retain);
            continue;
        }
        if ( xl6CgenIsXValueType(typeNode) ) {
            xl6CgenIndent(ctx);
            if ( retain ) {
                xl6OutputPrintf(ctx->out, "%s = %s(%s);\n", fieldPath, deep ? "xvoDeepCopy" : "xvoCopy", fieldPath);
            } else {
                xl6OutputPrintf(ctx->out, "if (%s != 0) { xvoUnref(%s); %s = 0; }\n", fieldPath, fieldPath, fieldPath);
            }
            continue;
        }
        if ( xl6CgenIsFunctionType(typeNode) ) {
            if ( retain ) {
                xl6CgenRetainFunctionPath(ctx, fieldPath);
            } else {
                xl6CgenReleaseFunctionPath(ctx, fieldPath);
            }
            continue;
        }
        nestedClass = xl6CgenFindClass(ctx, typeNode != NULL ? typeNode->text : NULL);
        if ( nestedClass != NULL ) {
            xl6CgenClassFunctionFieldRefs(ctx, nestedClass, fieldPath, retain, deep);
        }
    }
}

static void xl6CgenRetainClassFunctionFields(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* valuePath)
{
    xl6CgenClassFunctionFieldRefs(ctx, classNode, valuePath, 1, 0);
}

static void xl6CgenDeepCopyClassFunctionFields(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* valuePath)
{
    xl6CgenClassFunctionFieldRefs(ctx, classNode, valuePath, 1, 1);
}

static void xl6CgenReleaseClassFunctionFields(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* valuePath)
{
    xl6CgenClassFunctionFieldRefs(ctx, classNode, valuePath, 0, 0);
}

static XL6_AstNode* xl6CgenClassNodeForType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* resolvedType;

    if ( ctx == NULL || typeNode == NULL ) {
        return NULL;
    }
    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    resolvedType = xl6CgenResolveAliasType(ctx, concreteType);
    if ( resolvedType == NULL || resolvedType->text == NULL ) {
        return NULL;
    }
    return xl6CgenFindClass(ctx, resolvedType->text);
}

static int xl6CgenClassNeedsAutoCopyProtocol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* deriveName,
    const char* protocolName)
{
    if ( ctx == NULL || classNode == NULL || deriveName == NULL || protocolName == NULL ) {
        return 0;
    }
    if ( xl6CgenClassHasTypeParams(classNode) ) {
        return 0;
    }
    if ( !xl6CgenClassHasDerive(classNode, deriveName) ) {
        return 0;
    }
    return xl6CgenFindClassFunc(classNode, protocolName) == NULL;
}

static void xl6CgenRetainClassFunctionFieldsForType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* valuePath)
{
    XL6_AstNode* classNode;

    classNode = xl6CgenClassNodeForType(ctx, typeNode);
    if ( classNode != NULL ) {
        xl6CgenRetainClassFunctionFields(ctx, classNode, valuePath);
    }
}

static void xl6CgenReleaseClassFunctionFieldsForType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* valuePath)
{
    XL6_AstNode* classNode;

    classNode = xl6CgenClassNodeForType(ctx, typeNode);
    if ( classNode != NULL ) {
        xl6CgenReleaseClassFunctionFields(ctx, classNode, valuePath);
    }
}

static int xl6CgenClassTypeNeedsFunctionFieldCleanup(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    XL6_AstNode* classNode;

    classNode = xl6CgenClassNodeForType(ctx, typeNode);
    return classNode != NULL && xl6CgenClassNeedsDestroy(ctx, classNode);
}

static int xl6CgenAwaitExprReturnsOwnedTemporary(const XL6_AstNode* expr)
{
    const XL6_AstNode* operand;

    if ( expr == NULL || expr->kind != XL6_AST_AWAIT || expr->childCount == 0 ) {
        return 0;
    }
    operand = expr->children[0];
    return operand != NULL && operand->kind == XL6_AST_CALL;
}

static int xl6CgenAwaitExprHasConditionalOperand(const XL6_AstNode* expr)
{
    const XL6_AstNode* operand;

    if ( expr == NULL || expr->kind != XL6_AST_AWAIT || expr->childCount == 0 ) {
        return 0;
    }
    operand = expr->children[0];
    return operand != NULL && operand->kind == XL6_AST_CONDITIONAL && operand->childCount > 2;
}

static int xl6CgenAwaitExprReturnsOwnedTemporaryForType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* resultType)
{
    if ( xl6CgenAwaitExprReturnsOwnedTemporary(expr) ) {
        return 1;
    }
    if ( !xl6CgenAwaitExprHasConditionalOperand(expr) ) {
        return 0;
    }

    /*
        鏉′欢 await 浼氬湪鍒嗘敮灞傛妸 borrowed future 澶嶅埗鎴愪复鏃跺紩鐢ㄣ€?        class 缁撴灉闇€瑕侀澶?retain 鎴愬憳鍑芥暟瀛楁锛屼笉鑳藉彧闈犺繖涓?owned 鏍囪銆?    */
    return !xl6CgenIsClassType(ctx, resultType);
}

static int xl6CgenAwaitExprReturnsOwnedTemporaryCtx(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* resultType;
    int result;

    if ( expr == NULL || expr->kind != XL6_AST_AWAIT ) {
        return 0;
    }

    resultType = xl6CgenCloneExprTypeForInference(ctx, expr);
    result = xl6CgenAwaitExprReturnsOwnedTemporaryForType(ctx, expr, resultType);
    xl6AstFree(resultType);
    return result;
}

static int xl6CgenExprIsClassBorrow(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* typeNode)
{
    XL6_AstNode* exprType;
    const XL6_AstNode* resolvedExprType;
    const XL6_AstNode* resolvedType;
    int result;

    if ( ctx == NULL || expr == NULL || !xl6CgenClassTypeNeedsFunctionFieldCleanup(ctx, typeNode) ) {
        return 0;
    }
    if ( xl6CgenAwaitExprReturnsOwnedTemporaryForType(ctx, expr, typeNode) ) {
        return 0;
    }
    if ( expr->kind != XL6_AST_NAME &&
         expr->kind != XL6_AST_MEMBER &&
         expr->kind != XL6_AST_INDEX &&
         expr->kind != XL6_AST_AWAIT ) {
        return 0;
    }

    exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
    resolvedExprType = xl6CgenResolveAliasType(ctx, exprType);
    resolvedType = xl6CgenResolveAliasType(ctx, typeNode);
    result = resolvedExprType != NULL && resolvedType != NULL &&
             (xl6CgenTypeNodeEqual(resolvedExprType, resolvedType) ||
              (resolvedExprType->text != NULL && resolvedType->text != NULL &&
               xl6CTextEquals(resolvedExprType->text, resolvedType->text) &&
               xl6CgenClassNodeForType(ctx, resolvedExprType) != NULL &&
               xl6CgenClassNodeForType(ctx, resolvedType) != NULL));
    xl6AstFree(exprType);
    return result;
}

static void xl6CgenRetainClassFunctionFieldsForBorrowedExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* valuePath,
    const XL6_AstNode* expr)
{
    if ( xl6CgenExprIsClassBorrow(ctx, expr, typeNode) ) {
        xl6CgenRetainClassFunctionFieldsForType(ctx, typeNode, valuePath);
    }
}

static void xl6CgenReleaseReturnedValuePath(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* valuePath)
{
    if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6CgenReleaseFunctionPath(ctx, valuePath);
        return;
    }
    if ( xl6CgenClassTypeNeedsFunctionFieldCleanup(ctx, typeNode) ) {
        xl6CgenReleaseClassFunctionFieldsForType(ctx, typeNode, valuePath);
    }
}

