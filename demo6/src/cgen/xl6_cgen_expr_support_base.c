static void xl6CgenExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);

static int xl6CgenIsXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "var") ||
            xl6CTextEquals(typeNode->text, "__class_handle") ||
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
    const XL6_AstNode* classType;
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
    classType = NULL;
    ownedReceiverType = NULL;
    if ( receiver->kind == XL6_AST_MEMBER || receiver->kind == XL6_AST_INDEX || receiver->kind == XL6_AST_CALL ) {
        receiverType = receiver->kind == XL6_AST_MEMBER
            ? xl6CgenCloneMemberFieldTypeForInference(ctx, receiver)
            : xl6CgenCloneExprTypeForInference(ctx, receiver);
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
            if ( xl6CgenIsStringTypeNode(ctx, typeNode) ) {
                return 1;
            }
            if ( xl6CgenIsFunctionType(typeNode) ) {
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

static const char* xl6CgenTypedContainerCloneFuncName(const char* typeName)
{
    if ( xl6CTextEquals(typeName, "array") ) {
        return "xrtTypedArrayClone";
    }
    if ( xl6CTextEquals(typeName, "list") ) {
        return "xrtTypedListClone";
    }
    if ( xl6CTextEquals(typeName, "set") ) {
        return "xrtTypedSetClone";
    }
    if ( xl6CTextEquals(typeName, "dict") ) {
        return "xrtTypedDictClone";
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
    const char* typeName;
    const char* funcName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( typeName == NULL || fieldPath == NULL ) {
        return;
    }

    xl6CgenIndent(ctx);
    if ( retain ) {
        /*
            class 值复制必须继承源容器的对象模式。硬编码 LOCAL 会让已经发布到
            future/全局对象中的 SHARED 字段在下一次复制时重新变成线程私有对象。
        */
        funcName = xl6CgenTypedContainerCloneFuncName(typeName);
        if ( funcName == NULL ) {
            xl6CgenError(ctx, typeNode, "typed container copy requires array/list/set/dict");
            return;
        }
        xl6OutputPuts(fieldPath, ctx->out);
        xl6OutputPuts(" = ", ctx->out);
        xl6OutputPuts(funcName, ctx->out);
        xl6OutputPutc('(', ctx->out);
        xl6OutputPuts(fieldPath, ctx->out);
        xl6OutputPuts(");\n", ctx->out);
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
    int deep,
    int copyXValue)
{
    const XL6_AstNode* body;
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;
    size_t i;

    if ( ctx == NULL || classNode == NULL || valuePath == NULL ) {
        return;
    }

    if ( xl6CgenClassHasLocalVirtualSlots(classNode) ) {
        char selfRefPath[512];

        snprintf(selfRefPath, sizeof(selfRefPath), "%s.__xl6_self_ref", valuePath);
        xl6CgenIndent(ctx);
        if ( retain ) {
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xvoAddRef(%s); }\n", selfRefPath, selfRefPath);
        } else {
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xvoUnref(%s); %s = 0; }\n", selfRefPath, selfRefPath, selfRefPath);
        }
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        char basePath[512];

        baseClass = xl6CgenFindClass(ctx, base->text);
        snprintf(basePath, sizeof(basePath), "%s.base", valuePath);
        xl6CgenClassFunctionFieldRefs(ctx, baseClass, basePath, retain, deep, copyXValue);
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
        if ( xl6CgenIsStringTypeNode(ctx, typeNode) ) {
            xl6CgenIndent(ctx);
            if ( retain ) {
                xl6OutputPrintf(ctx->out, "%s = xrtCopyStr(%s, 0u);\n", fieldPath, fieldPath);
            } else {
                xl6OutputPrintf(ctx->out, "xrtFree((void*)%s); %s = 0;\n", fieldPath, fieldPath);
            }
            continue;
        }
        if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
            xl6CgenTypedContainerFieldRef(ctx, typeNode, fieldPath, retain);
            continue;
        }
        if ( xl6CgenIsXValueType(typeNode) ) {
            xl6CgenIndent(ctx);
            if ( retain ) {
                const char* copyFunc = deep ? "xvoDeepCopy" : (copyXValue ? "xvoCopy" : "xl6_xrt_retain");
                xl6OutputPrintf(ctx->out, "%s = %s(%s);\n", fieldPath, copyFunc, fieldPath);
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
            xl6CgenClassFunctionFieldRefs(ctx, nestedClass, fieldPath, retain, deep, copyXValue);
        }
    }
}

static void xl6CgenRetainClassFunctionFields(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* valuePath)
{
    xl6CgenClassFunctionFieldRefs(ctx, classNode, valuePath, 1, 0, 0);
}

static void xl6CgenCopyClassFunctionFields(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* valuePath)
{
    /*
        copy() 是值级拷贝：var 容器字段需要复制容器对象，不能仅增加引用。
        普通 retain 路径仍保持共享引用，用于返回/赋值等所有权修正。
    */
    xl6CgenClassFunctionFieldRefs(ctx, classNode, valuePath, 1, 0, 1);
}

static void xl6CgenDeepCopyClassFunctionFields(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* valuePath)
{
    xl6CgenClassFunctionFieldRefs(ctx, classNode, valuePath, 1, 1, 0);
}

static void xl6CgenReleaseClassFunctionFields(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* valuePath)
{
    xl6CgenClassFunctionFieldRefs(ctx, classNode, valuePath, 0, 0, 0);
}

static int xl6CgenClassHasVirtualSlotsInChain(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;

    if ( classNode == NULL ) {
        return 0;
    }
    if ( xl6CgenClassHasLocalVirtualSlots(classNode) ) {
        return 1;
    }
    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return 0;
    }
    baseClass = xl6CgenFindClass(ctx, base->text);
    return xl6CgenClassHasVirtualSlotsInChain(ctx, baseClass);
}

static void xl6CgenClassClearVirtualSelfRefs(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* valuePath)
{
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;
    char basePath[512];

    if ( ctx == NULL || classNode == NULL || valuePath == NULL ) {
        return;
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        baseClass = xl6CgenFindClass(ctx, base->text);
        snprintf(basePath, sizeof(basePath), "%s.base", valuePath);
        xl6CgenClassClearVirtualSelfRefs(ctx, baseClass, basePath);
    }

    if ( xl6CgenClassHasLocalVirtualSlots(classNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s.__xl6_self_ref = 0;\n", valuePath);
    }
}

static void xl6CgenClassAssignVirtualSelfRefs(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* valuePath,
    const char* refName)
{
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;
    char basePath[512];

    if ( ctx == NULL || classNode == NULL || valuePath == NULL || refName == NULL ) {
        return;
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        baseClass = xl6CgenFindClass(ctx, base->text);
        snprintf(basePath, sizeof(basePath), "%s.base", valuePath);
        xl6CgenClassAssignVirtualSelfRefs(ctx, baseClass, basePath, refName);
    }

    if ( xl6CgenClassHasLocalVirtualSlots(classNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s.__xl6_self_ref = %s;\n", valuePath, refName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s.__xl6_self_ref != 0) { xvoAddRef(%s.__xl6_self_ref); }\n", valuePath, valuePath);
    }
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
    concreteType = xl6CgenClassHandleTargetType(ctx, resolvedType);
    if ( concreteType != NULL ) {
        resolvedType = xl6CgenResolveAliasType(ctx, concreteType);
    }
    if ( resolvedType == NULL || resolvedType->text == NULL ) {
        return NULL;
    }
    return xl6CgenFindClass(ctx, resolvedType->text);
}

#include "xl6_cgen_expr_support_upcast.c"

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

    /* class 对外是 xvalue 句柄；借用读取只需保留对象本身，字段随对象共同存活。 */
    if ( xl6CgenClassHandleTargetType(ctx, typeNode) != NULL ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xvoAddRef(%s); }\n", valuePath, valuePath);
        return;
    }
    classNode = xl6CgenClassNodeForType(ctx, typeNode);
    if ( classNode != NULL ) {
        xl6CgenRetainClassFunctionFields(ctx, classNode, valuePath);
    }
}

static void xl6CgenReleaseClassFunctionFieldsForType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* valuePath)
{
    XL6_AstNode* classNode;

    if ( xl6CgenClassHandleTargetType(ctx, typeNode) != NULL ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xvoUnref(%s); %s = 0; }\n", valuePath, valuePath, valuePath);
        return;
    }
    classNode = xl6CgenClassNodeForType(ctx, typeNode);
    if ( classNode != NULL ) {
        xl6CgenReleaseClassFunctionFields(ctx, classNode, valuePath);
    }
}

static int xl6CgenClassTypeNeedsFunctionFieldCleanup(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    XL6_AstNode* classNode;

    /* class 句柄由 xvalue 类型操作统一管理内部字段，不能按结构体再次 retain/drop。 */
    if ( xl6CgenClassHandleTargetType(ctx, typeNode) != NULL ) {
        return 0;
    }
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
        条件 await 会在分支层把 borrowed future 复制成临时引用。
        class 结果还需要 retain 成员函数字段，不能只依赖 owned 标记。
    */
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
    if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        xl6CgenIndent(ctx);
        xl6CgenTypedContainerDestroyValue(ctx, typeNode, valuePath);
        return;
    }
    if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6CgenReleaseFunctionPath(ctx, valuePath);
        return;
    }
    if ( xl6CgenClassTypeNeedsFunctionFieldCleanup(ctx, typeNode) ) {
        xl6CgenReleaseClassFunctionFieldsForType(ctx, typeNode, valuePath);
    }
}

