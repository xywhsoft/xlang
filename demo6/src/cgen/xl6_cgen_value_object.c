static void xl6CgenTypedContainerFillFromLiteral(XL6_CgenContext* ctx, const char* cName, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr);

static void xl6CgenTypedContainerStorageToXValueExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* storageName)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const XL6_AstNode* itemRecordType;
    XL6_AstNode* itemClass;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    itemRecordType = xl6CgenClassHandleTargetType(ctx, itemType);
    itemClass = itemRecordType != NULL && itemRecordType->text != NULL ?
                xl6CgenFindClass(ctx, itemRecordType->text) :
                (itemType != NULL && itemType->text != NULL ? xl6CgenFindClass(ctx, itemType->text) : NULL);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    xl6OutputPuts("xl6_xrt_bind_type(", ctx->out);
    if ( xl6CTextEquals(typeName, "array") &&
         itemClass != NULL &&
         xl6CgenClassHasValueDerive(itemClass) ) {
        xl6OutputPuts("xl6_xrt_typed_array_to_xvalue_map(", ctx->out);
        xl6OutputPuts(storageName != NULL ? storageName : "0", ctx->out);
        xl6OutputPuts(", (xl6_xrt_typed_item_box_fn)", ctx->out);
        xl6CgenClassAutoToValueFormatSymbol(ctx->out, itemClass, NULL);
        xl6OutputPuts(itemRecordType != NULL ? ", true" : ", false", ctx->out);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6_xrt_typed_array_to_xvalue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPuts(storageName != NULL ? storageName : "0", ctx->out);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6_xrt_typed_list_to_xvalue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPuts(storageName != NULL ? storageName : "0", ctx->out);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6_xrt_typed_set_to_xvalue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPuts(storageName != NULL ? storageName : "0", ctx->out);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6_xrt_typed_dict_to_xvalue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPuts(storageName != NULL ? storageName : "0", ctx->out);
        xl6OutputPutc(')', ctx->out);
    } else {
        xl6CgenError(ctx, typeNode, "typed container boxing requires array/list/set/dict");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        return;
    }
    xl6OutputPuts(", ", ctx->out);
    xl6CgenEmitRuntimeTypeDescRef(ctx, typeNode);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenTypedContainerLocalDestroyStmt(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* storageName)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    xl6CgenIndent(ctx);
    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPrintf(ctx->out, "xrtTypedArrayDestroy(%s);\n", storageName);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPrintf(ctx->out, "xrtTypedListDestroy(%s);\n", storageName);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPrintf(ctx->out, "xrtTypedSetDestroy(%s);\n", storageName);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPrintf(ctx->out, "xrtTypedDictDestroy(%s);\n", storageName);
    } else {
        xl6CgenError(ctx, typeNode, "typed container destroy requires array/list/set/dict");
    }
#include "xl6_cgen_value_object_xvalue_stmt.c"

static int xl6CgenTypedFutureExprIsOwnedDirect(const XL6_AstNode* valueExpr)
{
    return valueExpr != NULL && valueExpr->kind == XL6_AST_CALL;
}

static int xl6CgenTypedFutureExprNeedsRelease(const XL6_AstNode* valueExpr)
{
    return xl6CgenTypedFutureExprIsOwnedDirect(valueExpr) ||
           (valueExpr != NULL && valueExpr->kind == XL6_AST_CONDITIONAL && valueExpr->childCount > 2);
}

static void xl6CgenTypedFutureOwnedExpr(XL6_CgenContext* ctx, const XL6_AstNode* valueExpr)
{
    if ( valueExpr != NULL && valueExpr->kind == XL6_AST_CONDITIONAL && valueExpr->childCount > 2 ) {
        xl6OutputPuts("(", ctx->out);
        xl6CgenExpr(ctx, valueExpr->children[0]);
        xl6OutputPuts(" ? ", ctx->out);
        xl6CgenTypedFutureOwnedExpr(ctx, valueExpr->children[1]);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenTypedFutureOwnedExpr(ctx, valueExpr->children[2]);
        xl6OutputPuts(")", ctx->out);
        return;
    }
    if ( xl6CgenTypedFutureExprIsOwnedDirect(valueExpr) ) {
        xl6CgenExpr(ctx, valueExpr);
        return;
    }
    xl6OutputPuts("xl6_future_add_ref_if_needed(", ctx->out);
    xl6CgenExpr(ctx, valueExpr);
    xl6OutputPuts(")", ctx->out);
}

static void xl6CgenTypedArrayFutureStmt(XL6_CgenContext* ctx, const char* cName, const XL6_AstNode* valueExpr)
{
    int id;
    int shouldRelease;

    id = ctx->tempId++;
    shouldRelease = xl6CgenTypedFutureExprNeedsRelease(valueExpr);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xfuture* __xl6_typed_future_%d = ", id);
    if ( valueExpr != NULL && valueExpr->kind == XL6_AST_CONDITIONAL && valueExpr->childCount > 2 ) {
        xl6CgenTypedFutureOwnedExpr(ctx, valueExpr);
    } else {
        xl6CgenExpr(ctx, valueExpr);
    }
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (!xrtTypedArrayAppend(%s, &__xl6_typed_future_%d)) {\n", cName, id);
    ctx->indent += 1;
    if ( shouldRelease ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xFutureRelease(__xl6_typed_future_%d);\n", id);
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts("xl6_runtime_fatal(3);\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    if ( shouldRelease ) {
        xl6OutputPuts("} else {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xFutureRelease(__xl6_typed_future_%d);\n", id);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    } else {
        xl6OutputPuts("}\n", ctx->out);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenTypedContainerFillFromLiteral(XL6_CgenContext* ctx, const char* cName, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr)
{
    const XL6_AstNode* literal;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;
    size_t i;

    if ( initExpr == NULL ) {
        return;
    }

    literal = xl6CgenTypedContainerInitLiteral(ctx, typeNode, initExpr);
    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( literal == NULL ) {
        xl6CgenError(ctx, initExpr, "静态泛型容器目前只支持同类容器字面量初始化");
        return;
    }

    if ( xl6CTextEquals(typeName, "array") ) {
        for ( i = 0; i < literal->childCount; ++i ) {
            if ( xl6CgenIsFutureLikeType(itemType) ) {
                xl6CgenTypedArrayFutureStmt(ctx, cName, literal->children[i]);
            } else {
                xl6CgenTypedContainerValueStmt(ctx, cName, "xrtTypedArrayAppendValue(", itemType, literal->children[i]);
            }
        }
        return;
    }

    if ( xl6CTextEquals(typeName, "set") ) {
        for ( i = 0; i < literal->childCount; ++i ) {
            xl6CgenTypedContainerValueStmt(ctx, cName, "xrtTypedSetAddValue(", itemType, literal->children[i]);
        }
        return;
    }

    if ( xl6CTextEquals(typeName, "list") ) {
        for ( i = 0; i < literal->childCount; ++i ) {
            const XL6_AstNode* pair = literal->children[i];
            const XL6_AstNode* key = pair != NULL && pair->childCount > 0 ? pair->children[0] : NULL;
            const XL6_AstNode* value = pair != NULL && pair->childCount > 1 ? pair->children[1] : NULL;
            int id = ctx->tempId++;
            char valueName[64];

            snprintf(valueName, sizeof(valueName), "__xl6_typed_value_%d", id);
            xl6CgenIndent(ctx);
            xl6OutputPuts("{\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvalue %s = 0;\n", valueName);
            if ( !xl6CgenTypedContainerNestedLiteralValueStmt(ctx, itemType, valueName, value) ) {
                xl6CgenXValueAssignStmt(ctx, itemType, valueName, value);
            }
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (!xrtTypedListSetValue(%s, (long long)(", cName);
            xl6CgenExpr(ctx, key);
            xl6OutputPrintf(ctx->out, "), %s)) {\n", valueName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("xl6_runtime_fatal(3);\n", ctx->out);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", valueName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        }
        return;
    }

    if ( xl6CTextEquals(typeName, "dict") ) {
        for ( i = 0; i < literal->childCount; ++i ) {
            const XL6_AstNode* pair = literal->children[i];
            const XL6_AstNode* key = pair != NULL && pair->childCount > 0 ? pair->children[0] : NULL;
            const XL6_AstNode* value = pair != NULL && pair->childCount > 1 ? pair->children[1] : NULL;
            int id = ctx->tempId++;
            char valueName[64];

            if ( !xl6CgenExprStaticTypeIs(ctx, key, "string") ) {
                xl6CgenError(ctx, key, "静态 dict 字面量键必须是 string");
                continue;
            }

            snprintf(valueName, sizeof(valueName), "__xl6_typed_value_%d", id);
            xl6CgenIndent(ctx);
            xl6OutputPuts("{\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvalue %s = 0;\n", valueName);
            if ( !xl6CgenTypedContainerNestedLiteralValueStmt(ctx, itemType, valueName, value) ) {
                xl6CgenXValueAssignStmt(ctx, itemType, valueName, value);
            }
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (!xrtTypedDictSetValue(%s, ", cName);
            xl6CgenExpr(ctx, key);
            xl6OutputPrintf(ctx->out, ", 0, %s)) {\n", valueName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("xl6_runtime_fatal(3);\n", ctx->out);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", valueName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        }
    }
}

static int xl6CgenFuncSingleReturnUsesTypedContainerStorage(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* returns;
    XL6_AstNode* returnType;
    int result;

    if ( ctx == NULL || funcNode == NULL ||
         (funcNode->kind != XL6_AST_FUNC && funcNode->kind != XL6_AST_GET) ) {
        return 0;
    }

    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    if ( returns == NULL || returns->childCount != 1 ) {
        return 0;
    }

    /*
        泛型实例化时，T 可能已经替换成 array<int> 这类静态容器。
        判断返回 ABI 前必须使用具体类型，否则声明会退回 xvalue。
    */
    returnType = xl6CgenCloneCanonicalTypeNode(ctx, returns->children[0]);
    result = xl6CgenIsTypedContainerType(ctx, returnType);
    xl6AstFree(returnType);
    return result;
}

static const char* xl6CgenFuncSingleReturnCType(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* returns;
    XL6_AstNode* returnType;
    const char* cType;

    if ( xl6CgenFuncSingleReturnUsesTypedContainerStorage(ctx, funcNode) ) {
        returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
        returnType = xl6CgenCloneCanonicalTypeNode(ctx, returns != NULL ? returns->children[0] : NULL);
        cType = xl6CgenTypedContainerCType(ctx, returnType);
        xl6AstFree(returnType);
        return cType;
    }

    returns = funcNode != NULL && funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    if ( returns != NULL && returns->childCount == 1 ) {
        returnType = xl6CgenCloneCanonicalTypeNode(ctx, returns->children[0]);
        cType = xl6CgenTypeName(ctx, returnType);
        xl6AstFree(returnType);
        return cType;
    }
    return "void";
}

static int xl6CgenParamTypeUsesTypedContainerStorage(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    if ( ctx == NULL ||
         typeNode == NULL ||
         ctx->currentNativeClass != NULL ) {
        return 0;
    }

    /*
     * 泛型函数实例化时，array<T>/list<T>/set<T>/dict<T> 的 T 已经可以通过
     * currentGenericParams/currentGenericArgs 替换成具体类型。只要替换后的
     * 容器元素不是 var，就继续使用 XRT typed-container 句柄作为 ABI；
     * 只有真正无法静态确定的容器才退回 xvalue。
     */
    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    return ctx != NULL &&
           concreteType != NULL;
}

static const char* xl6CgenParamCType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    static char pointerType[768];
    const char* baseType;

    if ( xl6CgenParamTypeUsesTypedContainerStorage(ctx, typeNode) ) {
        baseType = xl6CgenTypedContainerCType(ctx, typeNode);
    } else {
        baseType = xl6CgenTypeName(ctx, typeNode);
    }
    if ( typeNode != NULL &&
         (typeNode->flags & (XL6_AST_FLAG_BYREF | XL6_AST_FLAG_OUT)) != 0 ) {
        snprintf(pointerType, sizeof(pointerType), "%s*", baseType);
        return pointerType;
    }
    return baseType;
}

static void xl6CgenTypedContainerValueFromExprMode(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr, const char* mode)
{
    const XL6_AstNode* literal;

    literal = xl6CgenTypedContainerInitLiteral(ctx, typeNode, expr);
    if ( literal != NULL ) {
        xl6CgenTypedContainerCreateExprMode(ctx, typeNode, mode);
        return;
    }

    if ( xl6CgenTypedContainerCanMoveFromExpr(ctx, typeNode, expr) ) {
        xl6CgenExpr(ctx, expr);
        return;
    }
    if ( xl6CgenExprUsesTypedContainerStorage(ctx, expr) ) {
        if ( mode != NULL && xl6CTextEquals(mode, "XRT_OBJMODE_SHARED") ) {
            xl6CgenTypedContainerCopyExprMode(ctx, typeNode, expr, mode);
        } else {
        xl6CgenTypedContainerRetainExpr(ctx, typeNode, expr);
        }
        return;
    }

    xl6CgenExpr(ctx, expr);
}

static void xl6CgenTypedContainerValueFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr)
{
    xl6CgenTypedContainerValueFromExprMode(ctx, typeNode, expr, "XRT_OBJMODE_LOCAL");
}

static void xl6CgenTypedContainerReturnStmt(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr, int id)
{
    char returnName[64];

    snprintf(returnName, sizeof(returnName), "__xl6_return_%d", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, typeNode), returnName);
    xl6CgenTypedContainerValueFromExpr(ctx, typeNode, expr);
    xl6OutputPuts(";\n", ctx->out);
    if ( xl6CgenTypedContainerInitLiteral(ctx, typeNode, expr) != NULL ) {
        xl6CgenTypedContainerFillFromLiteral(ctx, returnName, typeNode, expr);
    }
}
static int xl6CgenIsCurrentNativeClassSelfReceiver(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    const XL6_AstNode* selfType;

    if ( ctx == NULL ||
         receiver == NULL ||
         receiver->kind != XL6_AST_NAME ||
         !xl6CTextEquals(receiver->text, "self") ) {
        return 0;
    }
    if ( ctx->currentNativeClass != NULL ) {
        return 1;
    }

    selfType = xl6CgenFindVarType(ctx, "self");
    return selfType != NULL && xl6CgenFindNativeBackedClass(ctx, selfType->text) != NULL;
}
static void xl6CgenNamedValueReference(XL6_CgenContext* ctx, const char* name)
{
    const XL6_AstNode* externalVar;
    unsigned int flags;

    if ( ctx == NULL || name == NULL ) {
        return;
    }
    if ( xl6CTextEquals(name, "self") && ctx->currentClass != NULL ) {
        xl6OutputPuts("xvoShare(xvoGetRecordOwner(self))", ctx->out);
        return;
    }
    if ( xl6CgenAnonCaptureIndex(ctx->currentAnonFunc, name) >= 0 ) {
        xl6OutputPuts("__env->", ctx->out);
        xl6CgenFputsCIdent(ctx->out, name);
        return;
    }
    if ( xl6CgenEmitImplicitSelfFieldReference(ctx, name) ) {
        return;
    }
    if ( xl6CgenFindTrackedVarType(ctx, name) == NULL ) {
        externalVar = xl6CgenFindExternalVar(ctx, name);
        if ( externalVar != NULL ) {
            xl6CgenExternalVarStorageName(ctx->out, externalVar);
            return;
        }
    }
    flags = xl6CgenFindTrackedVarFlags(ctx, name);
    if ( (flags & (XL6_CGEN_VAR_BYREF | XL6_CGEN_VAR_OUT)) != 0 ) {
        xl6OutputPuts("(*", ctx->out);
        xl6CgenFputsCIdent(ctx->out, name);
        xl6OutputPutc(')', ctx->out);
    } else {
        xl6CgenFputsValueStorageName(ctx, name);
    }
}

static void xl6CgenNativeReceiverHandleExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    if ( xl6CgenIsCurrentNativeClassSelfReceiver(ctx, receiver) ) {
        xl6OutputPuts("self->handle", ctx->out);
        return;
    }
    if ( receiver != NULL && receiver->kind == XL6_AST_NAME ) {
        xl6CgenNamedValueReference(ctx, receiver->text);
        xl6OutputPuts(".handle", ctx->out);
        return;
    }

    xl6CgenExpr(ctx, receiver);
    xl6OutputPuts(".handle", ctx->out);
}

static void xl6CgenNativeReceiverPtrExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    if ( xl6CgenIsCurrentNativeClassSelfReceiver(ctx, receiver) ) {
        xl6OutputPuts("self", ctx->out);
        return;
    }

    xl6OutputPutc('&', ctx->out);
    if ( receiver != NULL && receiver->kind == XL6_AST_NAME ) {
        xl6CgenNamedValueReference(ctx, receiver->text);
    } else {
        xl6CgenExpr(ctx, receiver);
    }
}

static int xl6CgenNativeClassMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* nativeClassNode;
    const XL6_AstNode* helperMethod;
    const XL6_AstNode* externMethod;
    const XL6_AstNode* method;
    const XL6_AstNode* params;
    const XL6_AstNode* args;
    const char* receiverName;
    int copyExternString;

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || (receiver->kind != XL6_AST_NAME && receiver->kind != XL6_AST_MEMBER) ) {
        return 0;
    }

    receiverName = receiver->kind == XL6_AST_NAME ? receiver->text : NULL;
    receiverType = receiver->kind == XL6_AST_NAME ? xl6CgenFindVarType(ctx, receiverName) : xl6CgenMemberFieldType(ctx, receiver);
    nativeClassNode = receiverType != NULL ? xl6CgenFindNativeBackedClass(ctx, receiverType->text) : NULL;
    if ( nativeClassNode == NULL ) {
        return 0;
    }

    helperMethod = xl6CgenResolveNativeClassFunc(ctx, nativeClassNode, call, member->text, 0);
    externMethod = xl6CgenResolveNativeExternFuncWithReport(ctx, nativeClassNode, call, member->text, 0);
    if ( helperMethod != NULL && externMethod != NULL ) {
        xl6CgenError(ctx, member, "native-backed class method call is ambiguous between helper and extern");
        xl6OutputPuts("0", ctx->out);
        return 1;
    }

    method = helperMethod != NULL ? helperMethod : externMethod;
    if ( method == NULL ) {
        xl6CgenError(ctx, member, "native extern method not found");
        xl6OutputPuts("0", ctx->out);
        return 1;
    }

    copyExternString = externMethod != NULL && xl6CgenCallableReturnsString(ctx, externMethod);
    if ( copyExternString ) {
        xl6OutputPuts("xl6_xrt_external_string_copy(", ctx->out);
    }
    if ( helperMethod != NULL ) {
        xl6CgenClassCallableSymbol(ctx, nativeClassNode, method);
    } else {
        xl6CgenCStringSymbol(ctx->out, method->text);
    }
    xl6OutputPutc('(', ctx->out);
    if ( helperMethod != NULL ) {
        xl6CgenNativeReceiverPtrExpr(ctx, receiver);
    } else {
        xl6CgenNativeReceiverHandleExpr(ctx, receiver);
    }
    args = call->childCount > 1 ? call->children[1] : NULL;
    params = xl6CgenCallableParams(method);
    if ( params != NULL && params->childCount > 0 ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenKnownCallArgs(ctx, method, args);
    }
    xl6OutputPutc(')', ctx->out);
    if ( copyExternString ) {
        xl6OutputPutc(')', ctx->out);
    }
    return 1;
}

static int xl6CgenCallIsExternalStringReturn(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* callee;

    if ( ctx == NULL || call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return 0;
    }
    callee = call->children[0];
    if ( callee == NULL ) {
        return 0;
    }
    if ( callee->kind == XL6_AST_NAME ) {
        return xl6CgenCallableReturnsString(ctx, xl6CgenFindExternFunc(ctx, callee->text));
    }
    if ( callee->kind == XL6_AST_MEMBER && callee->childCount > 0 ) {
        const XL6_AstNode* receiver = callee->children[0];
        XL6_AstNode* receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
        const XL6_AstNode* nativeClass = receiverType != NULL
            ? xl6CgenFindNativeBackedClass(ctx, receiverType->text)
            : NULL;
        const XL6_AstNode* method = nativeClass != NULL
            ? xl6CgenResolveNativeExternFunc(ctx, nativeClass, call, callee->text)
            : NULL;
        int result = xl6CgenCallableReturnsString(ctx, method);

        xl6AstFree(receiverType);
        return result;
    }
    return 0;
}

static int xl6CgenImplicitClassToStringCall(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    const XL6_AstNode* concreteType;
    XL6_AstNode* classNode;
    const XL6_AstNode* method;

    receiver = member != NULL && member->childCount > 0 ? member->children[0] : NULL;
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    concreteType = xl6CgenResolveAliasType(ctx, receiverType);
    if ( xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
        concreteType = xl6CgenClassHandleTargetType(ctx, concreteType);
    }
    classNode = concreteType != NULL && concreteType->text != NULL ? xl6CgenFindClass(ctx, concreteType->text) : NULL;
    if ( classNode == NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }

    method = xl6CgenFindClassFunc(classNode, "__toString__");
    if ( method != NULL ) {
        if ( xl6CgenClassProtocolCallPrefix(ctx, receiver, classNode, classNode, method, receiverType) ) {
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6OutputPuts("\"\"", ctx->out);
        }
        xl6AstFree(receiverType);
        return 1;
    }
    if ( !xl6CgenClassHasTypeParams(classNode) && xl6CgenClassHasDerive(classNode, "string") ) {
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, "__toString__");
        xl6OutputPutc('(', ctx->out);
        xl6CgenClassToStringSelfExpr(ctx, receiver, receiverType, concreteType);
        xl6OutputPutc(')', ctx->out);
        xl6AstFree(receiverType);
        return 1;
    }
    xl6AstFree(receiverType);
    return 0;
}

static void xl6CgenImplicitToStringCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;

    (void)call;
    if ( xl6CgenImplicitClassToStringCall(ctx, member) ) {
        return;
    }
    receiver = member != NULL && member->childCount > 0 ? member->children[0] : NULL;
    xl6OutputPuts("xl6_xrt_value_to_string_release(", ctx->out);
    xl6CgenXValueFromExpr(ctx, receiver);
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenClassMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* classType;
    XL6_AstNode* ownedReceiverType;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* args;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    char basePath[256];
    int fixedCount;
    int baseReceiver;

    receiverType = NULL;
    classType = NULL;
    ownedReceiverType = NULL;
    baseReceiver = 0;
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL ) {
        return 0;
    }

    if ( receiver->kind == XL6_AST_NAME && xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        /*
            self 在泛型类实例中带有具体类型参数。后续不仅用它解析重载，
            还要用同一类型生成实例化后的 C 符号名，不能只保留 class 声明。
        */
        receiverType = xl6CgenFindVarType(ctx, "self");
        classType = receiverType;
        classNode = ctx->currentClass;
    } else if ( receiver->kind == XL6_AST_NAME &&
                xl6CTextEquals(receiver->text, "base") &&
                ctx->currentClass != NULL ) {
        const XL6_AstNode* baseType = ctx->currentClass->childCount > 0
            ? ctx->currentClass->children[0]
            : NULL;

        baseReceiver = 1;
        ownedReceiverType = xl6CgenCloneCanonicalTypeNode(ctx, baseType);
        receiverType = ownedReceiverType;
        classType = receiverType;
        classNode = xl6CgenBaseClass(ctx, ctx->currentClass);
    } else if ( receiver->kind == XL6_AST_MEMBER ) {
        ownedReceiverType = xl6CgenCloneMemberFieldTypeForInference(ctx, receiver);
        receiverType = ownedReceiverType;
        classType = xl6CgenClassHandleTargetType(ctx, receiverType);
        if ( classType == NULL ) {
            classType = receiverType;
        }
        classNode = classType != NULL ? xl6CgenFindClass(ctx, classType->text) : NULL;
    } else {
        ownedReceiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
        receiverType = ownedReceiverType;
        classType = xl6CgenClassHandleTargetType(ctx, receiverType);
        if ( classType == NULL ) {
            classType = receiverType;
        }
        classNode = classType != NULL ? xl6CgenFindClass(ctx, classType->text) : NULL;
    }
    if ( receiver->kind != XL6_AST_NAME && receiver->kind != XL6_AST_MEMBER &&
         xl6CgenClassHandleTargetType(ctx, receiverType) == NULL ) {
        xl6AstFree(ownedReceiverType);
        return 0;
    }
    if ( classNode == NULL ) {
        xl6AstFree(ownedReceiverType);
        return 0;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( classType != NULL && classType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = classType->children[0];
    }

    ownerClass = NULL;
    method = xl6CgenResolveClassFunc(ctx, classNode, call, member->text, &ownerClass);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    if ( method == NULL ) {
        xl6CgenError(ctx, member, "找不到 class 方法");
        xl6OutputPuts("0", ctx->out);
        xl6AstFree(ownedReceiverType);
        return 1;
    }
    if ( ownerClass == NULL ) {
        ownerClass = classNode;
    }

    /*
        base.method() 明确要求静态调用父类实现，不能再次进入虚派发。
        普通 self/实例调用仍保持原有 virtual/override 语义。
    */
    if ( !baseReceiver &&
         xl6CgenVirtualClassMethodCall(ctx, call, member, receiver, classNode, ownerClass, method) ) {
        xl6AstFree(ownedReceiverType);
        return 1;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( classType != NULL && classType->childCount > 0 && xl6CgenClassHasTypeParams(ownerClass) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(ownerClass);
        ctx->currentGenericArgs = classType->children[0];
        xl6CgenGenericClassCallableSymbol(ctx, ownerClass, classType, method);
    } else {
        xl6CgenClassCallableSymbol(ctx, ownerClass, method);
    }
    xl6OutputPutc('(', ctx->out);
    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(ctx, classNode, ownerClass, basePath, sizeof(basePath)) ) {
        xl6CgenError(ctx, member, "无法生成继承方法调用的 base 路径");
        basePath[0] = '\0';
    }
    if ( baseReceiver ) {
        xl6OutputPuts("&self->base", ctx->out);
        xl6OutputPuts(basePath, ctx->out);
    } else if ( receiver->kind == XL6_AST_NAME && xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        if ( basePath[0] == '\0' ) {
            xl6OutputPuts("self", ctx->out);
        } else {
            xl6OutputPuts("&self->base", ctx->out);
            if ( strlen(basePath) > 5 ) {
                xl6OutputPuts(basePath + 5, ctx->out);
            }
        }
    } else if ( xl6CgenClassHandleTargetType(ctx, receiverType) != NULL ) {
        char receiverTypeName[512];
        xl6CgenClassTypeRefName(receiverTypeName, sizeof(receiverTypeName), classType);
        if ( basePath[0] == '\0' ) {
            xl6OutputPrintf(ctx->out, "((%s*)xl6_class_data(", receiverTypeName);
        } else {
            xl6OutputPrintf(ctx->out, "&((%s*)xl6_class_data(", receiverTypeName);
        }
        if ( receiver->kind == XL6_AST_NAME ) {
            xl6CgenNamedValueReference(ctx, receiver->text);
        } else {
            xl6CgenExpr(ctx, receiver);
        }
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d))", receiver->line, receiver->column);
        if ( basePath[0] != '\0' ) {
            xl6OutputPuts("->base", ctx->out);
            if ( strlen(basePath) > 5 ) {
                xl6OutputPuts(basePath + 5, ctx->out);
            }
        }
    } else {
        xl6OutputPutc('&', ctx->out);
        if ( receiver->kind == XL6_AST_NAME ) {
            xl6CgenNamedValueReference(ctx, receiver->text);
        } else {
            xl6CgenExpr(ctx, receiver);
        }
        xl6OutputPuts(basePath, ctx->out);
    }
    args = call->childCount > 1 ? call->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(method->childCount > 1 ? method->children[1] : NULL);
    if ( fixedCount > 0 ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenKnownCallArgs(ctx, method, args);
    }
    xl6OutputPutc(')', ctx->out);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    xl6AstFree(ownedReceiverType);
    return 1;
}

static XL6_AstNode* xl6CgenBaseClass(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* base;

    if ( classNode == NULL ) {
        return NULL;
    }
    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return NULL;
    }
    return xl6CgenFindClass(ctx, base->text);
}

static void xl6CgenEmitClassCreateCall(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* createNode, const char* selfExpr, const XL6_AstNode* args)
{
    const XL6_AstNode* params;
    int fixedCount;

    xl6CgenClassCallableSymbol(ctx, classNode, createNode);
    xl6OutputPutc('(', ctx->out);
    xl6OutputPuts(selfExpr != NULL ? selfExpr : "self", ctx->out);
    params = xl6CgenCallableParams(createNode);
    fixedCount = xl6CgenFixedParamCount(params);
    if ( fixedCount > 0 ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenParamNodeArgs(ctx, params, args);
    }
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenNativeClassCtorHasPointBindingArgs(const XL6_AstNode* args)
{
    const XL6_AstNode* arg;

    if ( args == NULL || args->childCount != 1 ) {
        return 0;
    }
    arg = args->children[0];
    return arg != NULL && arg->kind != XL6_AST_NAMED_ARG && arg->childCount > 0;
}

static int xl6CgenEmitNativeClassHandleBinding(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* varPath, const XL6_AstNode* args)
{
    const XL6_AstNode* arg;

    if ( !xl6CgenClassHasNativeHandle(classNode) || !xl6CgenNativeClassCtorHasPointBindingArgs(args) ) {
        return 0;
    }

    arg = args->children[0];
    xl6CgenIndent(ctx);
    xl6OutputPuts(varPath != NULL ? varPath : "self", ctx->out);
    xl6OutputPuts(".handle = ", ctx->out);
    xl6CgenNativeBackedPointBindingValue(ctx, classNode, arg->children[0]);
    xl6OutputPuts(";\n", ctx->out);
    return 1;
}

static int xl6CgenEmitClassCtorInto(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* targetExpr)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    XL6_AstNode* classNode;
    XL6_AstNode* ownedClassType;
    const XL6_AstNode* classType;
    const XL6_AstNode* createNode;
    const XL6_AstNode* params;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    char selfExpr[256];
    int canNativeHandleBind;
    int isGeneric;

    if ( ctx == NULL || call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 || targetExpr == NULL ) {
        return 0;
    }

    callee = call->children[0];
    classNode = NULL;
    ownedClassType = NULL;
    classType = NULL;
    isGeneric = 0;
    if ( callee != NULL && callee->kind == XL6_AST_NAME ) {
        classNode = xl6CgenFindClass(ctx, callee->text);
        if ( classNode == NULL || xl6CgenClassHasTypeParams(classNode) ) {
            return 0;
        }
        classType = classNode;
    } else if ( callee != NULL && callee->kind == XL6_AST_GENERIC_INSTANT ) {
        ownedClassType = xl6CgenGenericClassCtorType(ctx, call, &classNode);
        if ( ownedClassType == NULL || classNode == NULL ) {
            xl6AstFree(ownedClassType);
            return 0;
        }
        classType = ownedClassType;
        isGeneric = 1;
    } else {
        return 0;
    }

    args = call->childCount > 1 ? call->children[1] : NULL;
    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( isGeneric ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = classType->children[0];
    }
    canNativeHandleBind = xl6CgenClassHasNativeHandle(classNode) &&
                          xl6CgenNativeClassCtorHasPointBindingArgs(args);
    createNode = xl6CgenResolveClassCreateMode(ctx, classNode, call, !canNativeHandleBind);

    xl6CgenIndent(ctx);
    xl6OutputPuts(targetExpr, ctx->out);
    xl6OutputPuts(" = (", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, classType), ctx->out);
    xl6OutputPuts("){0};\n", ctx->out);

    xl6CgenClassDefaultInitFields(ctx, classNode, targetExpr);
    if ( createNode != NULL ) {
        snprintf(selfExpr, sizeof(selfExpr), "&%s", targetExpr);
        xl6CgenIndent(ctx);
        if ( isGeneric ) {
            xl6CgenGenericClassCallableSymbol(ctx, classNode, classType, createNode);
            xl6OutputPutc('(', ctx->out);
            xl6OutputPuts(selfExpr, ctx->out);
            params = xl6CgenCallableParams(createNode);
            if ( xl6CgenFixedParamCount(params) > 0 ) {
                xl6OutputPuts(", ", ctx->out);
                xl6CgenParamNodeArgs(ctx, params, args);
            }
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6CgenEmitClassCreateCall(ctx, classNode, createNode, selfExpr, args);
        }
        xl6OutputPuts(";\n", ctx->out);
    } else {
        if ( args != NULL && args->childCount > 0 &&
             !xl6CgenEmitNativeClassHandleBinding(ctx, classNode, targetExpr, args) ) {
            xl6CgenError(ctx, call, "class constructor has arguments but no matching create method");
        }
        xl6CgenEmitAutoBaseCreateForValue(ctx, classNode, targetExpr, call);
    }
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    xl6AstFree(ownedClassType);
    return 1;
}

static int xl6CgenClassConstructorExpr(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* nameNode;
    XL6_AstNode* classNode;

    if ( ctx == NULL || call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return 0;
    }

    callee = call->children[0];
    nameNode = callee;
    if ( callee != NULL && callee->kind == XL6_AST_GENERIC_INSTANT ) {
        nameNode = callee->childCount > 0 ? callee->children[0] : NULL;
    }
    if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME ) {
        return 0;
    }

    classNode = xl6CgenFindClass(ctx, nameNode->text);
    if ( classNode == NULL ) {
        return 0;
    }
    xl6CgenError(ctx, call, "class constructor expression requires statement-level lowering");
    xl6OutputPuts("0", ctx->out);
    return 1;
}

static int xl6CgenBaseCtorCall(XL6_CgenContext* ctx, const XL6_AstNode* call)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    XL6_AstNode* baseClass;
    const XL6_AstNode* createNode;

    if ( call == NULL || call->kind != XL6_AST_CALL || call->childCount == 0 ) {
        return 0;
    }
    callee = call->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME || !xl6CTextEquals(callee->text, "base") ) {
        return 0;
    }

    args = call->childCount > 1 ? call->children[1] : NULL;
    if ( ctx->currentClass == NULL ) {
        xl6CgenError(ctx, call, "base constructor call requires current class context");
        xl6OutputPuts("0", ctx->out);
        return 1;
    }

    baseClass = xl6CgenBaseClass(ctx, ctx->currentClass);
    if ( baseClass == NULL ) {
        xl6CgenError(ctx, call, "当前 class 没有父类，不能调用 base(...)");
        xl6OutputPuts("0", ctx->out);
        return 1;
    }

    createNode = xl6CgenResolveClassCreate(ctx, baseClass, call);
    if ( createNode == NULL ) {
        if ( args != NULL && args->childCount > 0 ) {
            xl6CgenError(ctx, call, "父类没有 create，base(...) 不能传参");
        }
        xl6OutputPuts("((void)0)", ctx->out);
        return 1;
    }

    xl6CgenEmitClassCreateCall(ctx, baseClass, createNode, "&self->base", args);
    return 1;
}

#include "xl6_cgen_value_object_fields.c"
