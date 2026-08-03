#include "src/cgen/xl6_cgen_async_func_args.c"

static void xl6CgenAsyncArgsFreeSymbol(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    xl6CgenAsyncArgsStructName(ctx, node);
    xl6OutputPuts("_free", ctx->out);
}

static void xl6CgenAsyncArgFieldPath(char* buffer, size_t capacity, const XL6_AstNode* param)
{
    char fieldName[512];

    xl6CgenCIdentText(fieldName, sizeof(fieldName), param != NULL ? param->text : NULL);
    snprintf(buffer, capacity, "__xl6_args->%s", fieldName);
}

/*
    异步参数帧跨线程存在，必须独立持有所有托管参数。
    这个析构函数同时服务于启动失败和尚未移交给 worker 的错误路径。
*/
static void xl6CgenAsyncArgsFreeFunc(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* params = xl6CgenAsyncParams(node);
    size_t i;

    if ( !xl6CgenAsyncIsClassMethod(ctx) &&
         (params == NULL || params->childCount == 0) ) {
        return;
    }

    xl6OutputPuts("static void ", ctx->out);
    xl6CgenAsyncArgsFreeSymbol(ctx, node);
    xl6OutputPutc('(', ctx->out);
    xl6CgenAsyncArgsStructName(ctx, node);
    xl6OutputPuts("* __xl6_args)\n{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( __xl6_args == 0 ) { return; }\n", ctx->out);
    if ( xl6CgenAsyncIsClassMethod(ctx) ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts(
            "if (__xl6_args->__xl6_self != 0) { "
            "xvoUnref(__xl6_args->__xl6_self); "
            "__xl6_args->__xl6_self = 0; }\n",
            ctx->out);
    }

    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* typeNode = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
        XL6_AstNode* classNode;
        char fieldPath[1024];

        if ( typeNode == NULL ) {
            continue;
        }
        xl6CgenAsyncArgFieldPath(fieldPath, sizeof(fieldPath), param);
        if ( xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtFree((void*)%s); %s = 0; }\n", fieldPath, fieldPath, fieldPath);
        } else if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", fieldPath);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6CgenTypedContainerDestroyValue(ctx, typeNode, fieldPath);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        } else if ( xl6CgenIsSpecialCollectionType(ctx, typeNode) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s != 0) { ", fieldPath);
            xl6CgenSpecialCollectionReleaseCExpr(ctx, typeNode, fieldPath);
            xl6OutputPrintf(ctx->out, "; %s = 0; }\n", fieldPath);
        } else if ( xl6CgenIsWeakType(typeNode) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoWeakUnit(&%s);\n", fieldPath);
        } else if ( xl6CgenIsXValueType(typeNode) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xvoUnref(%s); %s = 0; }\n", fieldPath, fieldPath, fieldPath);
        } else if ( xl6CgenIsFunctionType(typeNode) ) {
            xl6CgenReleaseFunctionPath(ctx, fieldPath);
        } else if ( xl6CgenNullableHasManagedInner(ctx, typeNode) ) {
            char dropName[64];
            xl6CgenNullableDropHelperName(dropName, sizeof(dropName), typeNode);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s(&%s);\n", dropName, fieldPath);
        } else if ( xl6CgenIsTaggedEnumType(ctx, typeNode) ) {
            xl6CgenIndent(ctx);
            xl6OutputPuts("xrtTypeDropValue(", ctx->out);
            xl6CgenEmitXrtTypeDescRef(ctx, typeNode);
            xl6OutputPrintf(ctx->out, ", &%s);\n", fieldPath);
        } else {
            classNode = xl6CgenClassNodeForType(ctx, typeNode);
            if ( classNode != NULL ) {
                xl6CgenDeclDestroyTemporaryClassValue(ctx, classNode, typeNode, fieldPath);
            }
        }
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("xrtFree(__xl6_args);\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenAsyncCopyArgToFrame(XL6_CgenContext* ctx, const XL6_AstNode* param)
{
    const XL6_AstNode* typeNode = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
    XL6_AstNode* classNode;
    char fieldPath[1024];
    char paramName[512];

    if ( typeNode == NULL ) {
        return;
    }
    xl6CgenAsyncArgFieldPath(fieldPath, sizeof(fieldPath), param);
    xl6CgenCIdentText(paramName, sizeof(paramName), param->text);
    if ( xl6CgenIsTaggedEnumType(ctx, typeNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (!xrtTypeCopyValue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, typeNode);
        xl6OutputPrintf(
            ctx->out,
            ", &%s, &%s)) { xl6_function_fatal(2); }\n",
            fieldPath,
            paramName);
        return;
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts(fieldPath, ctx->out);
    xl6OutputPuts(" = ", ctx->out);
    if ( xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
        xl6OutputPrintf(
            ctx->out,
            xl6CgenIsNullableStringTypeNode(ctx, typeNode) ?
                "xl6_nullable_string_copy(%s)" :
                "xrtCopyStr(%s, 0u)",
            paramName);
    } else if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        xl6CgenTypedContainerCopyCExprMode(ctx, typeNode, paramName, "XRT_OBJMODE_SHARED");
    } else if ( xl6CgenIsSpecialCollectionType(ctx, typeNode) ) {
        xl6CgenSpecialCollectionRetainCExpr(ctx, typeNode, paramName);
    } else if ( xl6CgenIsWeakType(typeNode) ) {
        xl6OutputPrintf(ctx->out, "xvoWeakClone(&%s)", paramName);
    } else if ( xl6CgenIsXValueType(typeNode) ) {
        xl6OutputPrintf(ctx->out, "xl6_xrt_retain(%s)", paramName);
    } else if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6OutputPuts(paramName, ctx->out);
    } else if ( xl6CgenNullableHasManagedInner(ctx, typeNode) ) {
        char copyName[64];
        xl6CgenNullableCopyHelperName(copyName, sizeof(copyName), typeNode);
        xl6OutputPrintf(ctx->out, "%s(%s)", copyName, paramName);
    } else {
        classNode = xl6CgenClassNodeForType(ctx, typeNode);
        if ( classNode != NULL && !xl6CgenClassHasTypeParams(classNode) ) {
            xl6CgenClassCopyBuiltinHelperSymbol(ctx->out, classNode, 0);
            xl6OutputPrintf(ctx->out, "(%s, 1)", paramName);
        } else {
            xl6OutputPuts(paramName, ctx->out);
        }
    }
    xl6OutputPuts(";\n", ctx->out);

    if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6CgenRetainFunctionPath(ctx, fieldPath);
    } else {
        classNode = xl6CgenClassNodeForType(ctx, typeNode);
        if ( classNode != NULL && xl6CgenClassHasTypeParams(classNode) ) {
            xl6CgenRetainClassFunctionFields(ctx, classNode, fieldPath);
        }
    }
}

static void xl6CgenAsyncTakeArgFromFrame(XL6_CgenContext* ctx, const XL6_AstNode* param)
{
    const XL6_AstNode* typeNode = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
    char fieldPath[1024];
    char paramName[512];

    xl6CgenAsyncArgFieldPath(fieldPath, sizeof(fieldPath), param);
    xl6CgenCIdentText(paramName, sizeof(paramName), param != NULL ? param->text : NULL);
    xl6CgenIndent(ctx);
    xl6OutputPuts(typeNode != NULL ? xl6CgenAsyncParamCType(ctx, typeNode) : "int", ctx->out);
    xl6OutputPutc(' ', ctx->out);
    xl6OutputPuts(paramName, ctx->out);
    xl6OutputPuts(" = ", ctx->out);
    if ( typeNode != NULL && xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
        if ( xl6CgenIsNullableStringTypeNode(ctx, typeNode) ) {
            xl6OutputPrintf(
                ctx->out,
                "%s != 0 ? xrtTempStringTake((char*)%s, 0u) : 0",
                fieldPath,
                fieldPath);
        } else {
            xl6OutputPrintf(
                ctx->out,
                "xrtTempStringTake((char*)%s, 0u)",
                fieldPath);
        }
    } else {
        xl6OutputPuts(fieldPath, ctx->out);
    }
    xl6OutputPuts(";\n", ctx->out);

    if ( typeNode == NULL || (!xl6CgenIsOwnedStringTypeNode(ctx, typeNode) &&
                              !xl6CgenIsTypedContainerType(ctx, typeNode) &&
                              !xl6CgenIsSpecialCollectionType(ctx, typeNode) &&
                               !xl6CgenIsWeakType(typeNode) &&
                               !xl6CgenIsXValueType(typeNode) &&
                               !xl6CgenIsFunctionType(typeNode) &&
                               !xl6CgenNullableHasManagedInner(ctx, typeNode) &&
                               !xl6CgenIsTaggedEnumType(ctx, typeNode) &&
                               !xl6CgenIsClassType(ctx, typeNode)) ) {
        return;
    }
    xl6CgenIndent(ctx);
    if ( xl6CgenIsWeakType(typeNode) ||
         xl6CgenIsFunctionType(typeNode) ||
         xl6CgenNullableHasManagedInner(ctx, typeNode) ||
         xl6CgenIsTaggedEnumType(ctx, typeNode) ||
         xl6CgenIsClassType(ctx, typeNode) ) {
        xl6OutputPrintf(ctx->out, "memset(&%s, 0, sizeof(%s));\n", fieldPath, fieldPath);
    } else {
        xl6OutputPrintf(ctx->out, "%s = 0;\n", fieldPath);
    }
}

static void xl6CgenRegisterAsyncParamOwnerCleanups(XL6_CgenContext* ctx, const XL6_AstNode* params)
{
    size_t i;

    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* typeNode = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
        XL6_AstNode* classNode;

        if ( typeNode == NULL ) {
            continue;
        }
        if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
            char ownerName[512];
            xl6CgenTypedParamOwnerName(ownerName, sizeof(ownerName), param->text);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = 1;\n", ownerName);
        } else if ( xl6CgenIsSpecialCollectionType(ctx, typeNode) ) {
            xl6CgenAddSpecialCollectionCleanup(ctx, param->text, typeNode);
        } else if ( xl6CgenIsXValueType(typeNode) ) {
            /* var 与 class handle 已由普通参数清理栈统一持有，不能重复注册 class 析构。 */
            continue;
        } else if ( xl6CgenIsFunctionType(typeNode) ||
                    xl6CgenNullableHasManagedInner(ctx, typeNode) ) {
            /* 函数值的所有权由统一参数注册路径处理，不能在 async 帧里重复登记。 */
            continue;
        } else {
            classNode = xl6CgenClassNodeForType(ctx, typeNode);
            if ( classNode != NULL ) {
                xl6CgenAddClassCleanup(ctx, param->text, classNode, typeNode->childCount > 0 ? typeNode : NULL);
            }
        }
    }
}

static void xl6CgenAsyncResultFreeFunc(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* returns;
    const XL6_AstNode* resultType;
    XL6_AstNode* classNode;
    int isGenericClass;

    returns = node != NULL && node->childCount > 2 ? node->children[2] : NULL;
    resultType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    if ( xl6CgenIsTaggedEnumType(ctx, resultType) ) {
        xl6OutputPuts("static void ", ctx->out);
        xl6CgenAsyncResultFreeSymbol(ctx, node);
        xl6OutputPuts("(void* pValue)\n{\n", ctx->out);
        xl6OutputPuts("    if ( pValue != 0 ) { xrtTypeDropValue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, resultType);
        xl6OutputPuts(", pValue); }\n", ctx->out);
        xl6OutputPuts("    xrtFree(pValue);\n}\n\n", ctx->out);
        return;
    }
    if ( xl6CgenNullableHasManagedInner(ctx, resultType) ) {
        char dropName[64];
        char cTypeName[64];

        xl6CgenNullableDropHelperName(dropName, sizeof(dropName), resultType);
        xl6CgenNullableCTypeName(cTypeName, sizeof(cTypeName), resultType);
        xl6OutputPuts("static void ", ctx->out);
        xl6CgenAsyncResultFreeSymbol(ctx, node);
        xl6OutputPuts("(void* pValue)\n{\n", ctx->out);
        xl6OutputPrintf(
            ctx->out,
            "    if ( pValue != 0 ) { %s((%s*)pValue); }\n",
            dropName,
            cTypeName);
        xl6OutputPuts("    xrtFree(pValue);\n}\n\n", ctx->out);
        return;
    }
    if ( !xl6CgenClassTypeNeedsFunctionFieldCleanup(ctx, resultType) ) {
        return;
    }

    classNode = xl6CgenClassNodeForType(ctx, resultType);
    if ( classNode == NULL ) {
        return;
    }
    isGenericClass = resultType != NULL && resultType->childCount > 0 && xl6CgenClassHasTypeParams(classNode);

    xl6OutputPuts("static void ", ctx->out);
    xl6CgenAsyncResultFreeSymbol(ctx, node);
    xl6OutputPuts("(void* pValue)\n{\n", ctx->out);
    xl6OutputPuts("    if ( pValue != 0 ) {\n", ctx->out);
    xl6OutputPuts("        ", ctx->out);
    if ( isGenericClass ) {
        xl6CgenGenericClassMemberSymbol(ctx, classNode, resultType, "destroy");
    } else {
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
    }
    xl6OutputPuts("((", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, resultType), ctx->out);
    xl6OutputPuts("*)pValue);\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    xrtFree(pValue);\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}

static const char* xl6CgenAsyncTypedContainerFreeHelper(XL6_CgenContext* ctx, const XL6_AstNode* resultType)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, resultType);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6CTextEquals(typeName, "array") ) {
        return "xl6_async_free_typed_array_value";
    }
    if ( xl6CTextEquals(typeName, "list") ) {
        return "xl6_async_free_typed_list_value";
    }
    if ( xl6CTextEquals(typeName, "set") ) {
        return "xl6_async_free_typed_set_value";
    }
    if ( xl6CTextEquals(typeName, "dict") ) {
        return "xl6_async_free_typed_dict_value";
    }
    return "xrtFree";
}

static const char* xl6CgenAsyncSpecialCollectionFreeHelper(XL6_CgenContext* ctx, const XL6_AstNode* resultType)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, resultType));
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6CTextEquals(typeName, "Stack") ) {
        return "xl6_async_free_typed_stack_value";
    }
    if ( xl6CTextEquals(typeName, "AvlTree") ) {
        return "xl6_async_free_typed_avl_value";
    }
    return "xl6_async_free_typed_queue_value";
}

static void xl6CgenAsyncResultFreeValue(XL6_CgenContext* ctx, const XL6_AstNode* funcNode, const XL6_AstNode* resultType)
{
    if ( xl6CgenIsTypedContainerType(ctx, resultType) ) {
        xl6OutputPuts(xl6CgenAsyncTypedContainerFreeHelper(ctx, resultType), ctx->out);
        return;
    }
    if ( xl6CgenIsSpecialCollectionType(ctx, resultType) ) {
        xl6OutputPuts(xl6CgenAsyncSpecialCollectionFreeHelper(ctx, resultType), ctx->out);
        return;
    }
    if ( xl6CgenIsWeakType(resultType) ) {
        xl6OutputPuts("xl6_async_free_weak_value", ctx->out);
        return;
    }
    if ( xl6CgenIsXValueType(resultType) ) {
        xl6OutputPuts("xl6_async_free_xvalue_value", ctx->out);
        return;
    }
    if ( xl6CgenIsFunctionType(resultType) ) {
        xl6OutputPuts("xl6_async_free_function_value", ctx->out);
        return;
    }
    if ( xl6CgenNullableHasManagedInner(ctx, resultType) ) {
        xl6CgenAsyncResultFreeSymbol(ctx, funcNode);
        return;
    }
    if ( xl6CgenIsTaggedEnumType(ctx, resultType) ) {
        xl6CgenAsyncResultFreeSymbol(ctx, funcNode);
        return;
    }
    if ( xl6CgenIsOwnedStringTypeNode(ctx, resultType) ) {
        xl6OutputPuts("xl6_async_free_string_value", ctx->out);
        return;
    }
    if ( xl6CgenClassTypeNeedsFunctionFieldCleanup(ctx, resultType) ) {
        xl6CgenAsyncResultFreeSymbol(ctx, funcNode);
        return;
    }
    xl6OutputPuts("xrtFree", ctx->out);
}

static const XL6_AstNode* xl6CgenAsyncSingleResultType(const XL6_AstNode* node)
{
    const XL6_AstNode* returns;

    returns = node != NULL && node->childCount > 2 ? node->children[2] : NULL;
    return returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
}

static void xl6CgenAwaitClassReleaseHelperSymbol(XL6_CgenContext* ctx, const XL6_AstNode* funcNode)
{
    xl6OutputPuts("xl6_future_await_class_", ctx->out);
    xl6CgenFuncSymbol(ctx, funcNode);
    xl6OutputPuts("_release", ctx->out);
}

static int xl6CgenAsyncClassResultMatches(XL6_CgenContext* ctx, const XL6_AstNode* funcNode, const XL6_AstNode* resultType)
{
    const XL6_AstNode* funcResultType;
    const XL6_AstNode* left;
    const XL6_AstNode* right;

    if ( ctx == NULL || funcNode == NULL || resultType == NULL || !xl6CgenIsAsyncFunc(funcNode) ) {
        return 0;
    }

    funcResultType = xl6CgenAsyncSingleResultType(funcNode);
    left = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, funcResultType));
    right = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, resultType));
    return left != NULL && right != NULL && xl6CgenTypeSymbolEqual(ctx, left, right);
}

static XL6_AstNode* xl6CgenAsyncFuncFromCallExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    XL6_AstNode* func;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return NULL;
    }

    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return NULL;
    }

    func = xl6CgenResolveCallFunc(ctx, expr, callee->text);
    return func != NULL && func->kind == XL6_AST_ASYNC_FUNC ? func : NULL;
}

static XL6_AstNode* xl6CgenFindAsyncClassResultFunc(XL6_CgenContext* ctx, const XL6_AstNode* resultType)
{
    size_t i;

    if ( ctx == NULL || resultType == NULL ) {
        return NULL;
    }

    for ( i = 0; ctx->root != NULL && i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( node != NULL && xl6CgenAsyncClassResultMatches(ctx, node, resultType) ) {
            return node;
        }
    }

    for ( i = 0; i < ctx->genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
        XL6_AstNode* func = (XL6_AstNode*)ctx->genericInstances[i].func;
        int matched;

        ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
        ctx->currentGenericArgs = ctx->genericInstances[i].typeArgs;
        matched = xl6CgenAsyncClassResultMatches(ctx, func, resultType);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        if ( matched ) {
            return func;
        }
    }

    return NULL;
}

static XL6_AstNode* xl6CgenAwaitClassReleaseHelperFunc(
    XL6_CgenContext* ctx,
    const XL6_AstNode* resultType,
    const XL6_AstNode* expr)
{
    XL6_AstNode* func;

    func = xl6CgenAsyncFuncFromCallExpr(ctx, expr);
    if ( func != NULL && xl6CgenAsyncClassResultMatches(ctx, func, resultType) ) {
        return func;
    }

    return xl6CgenFindAsyncClassResultFunc(ctx, resultType);
}

static void xl6CgenAsyncClassAwaitReleaseHelper(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* resultType;
    const char* cType;
    int prevIndent;

    resultType = xl6CgenAsyncSingleResultType(node);
    if ( !xl6CgenIsClassType(ctx, resultType) ) {
        return;
    }

    cType = xl6CgenTypeName(ctx, resultType);
    xl6OutputPuts("static ", ctx->out);
    xl6OutputPuts(cType, ctx->out);
    xl6OutputPutc(' ', ctx->out);
    xl6CgenAwaitClassReleaseHelperSymbol(ctx, node);
    xl6OutputPuts("(xfuture* pFuture, const char* file, int line, int column)\n{\n", ctx->out);
    xl6OutputPuts("    ", ctx->out);
    xl6OutputPuts(cType, ctx->out);
    xl6OutputPuts(" result;\n", ctx->out);
    xl6OutputPuts("    void* value;\n", ctx->out);
    xl6OutputPuts("    memset(&result, 0, sizeof(result));\n", ctx->out);
    xl6OutputPuts("    value = xl6_future_await_value(pFuture, file, line, column, 1, sizeof(result));\n", ctx->out);
    xl6OutputPuts("    if ( value != 0 && !xl6_error_has() ) {\n", ctx->out);
    /*
        future 的 OWN_VALUE 表示 future 拥有结果，不表示当前 await 可以窃取它。
        即使调用侧持有一个临时 future，也统一复制结果，保证 repeated await 语义。
    */
    xl6OutputPuts("        result = *(", ctx->out);
    xl6OutputPuts(cType, ctx->out);
    xl6OutputPuts("*)value;\n", ctx->out);
    prevIndent = ctx->indent;
    ctx->indent = 2;
    xl6CgenRetainClassFunctionFieldsForType(ctx, resultType, "result");
    ctx->indent = prevIndent;
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    xl6_future_release_if_needed(pFuture);\n", ctx->out);
    xl6OutputPuts("    return result;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenAsyncPendingResultCompletion(XL6_CgenContext* ctx, int targetId)
{
    const XL6_AstNode* resultType = ctx->currentReturnType;
    const char* cType;
    char resultName[96];

    if ( resultType == NULL || !xl6CgenFunctionHasReturnValue(ctx) ) {
        xl6CgenEmitCleanupsSince(ctx, 0);
        xl6CgenIndent(ctx);
        xl6OutputPuts("xl6_async_result_ok(pOut);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("return 0;\n", ctx->out);
        return;
    }
    snprintf(resultName, sizeof(resultName), "__xl6_pending_return_%d", targetId);
    cType = xl6CgenIsTypedContainerType(ctx, resultType) ?
            xl6CgenTypedContainerCType(ctx, resultType) :
            xl6CgenTypeName(ctx, resultType);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s* __xl6_pending_box_%d = (%s*)xrtMalloc(sizeof(%s));\n",
                    cType, targetId, cType, cType);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_pending_box_%d == 0) {\n", targetId);
    ctx->indent += 1;
    xl6CgenDiscardTemporaryResultByType(ctx, resultType, resultName);
    xl6CgenEmitCleanupsSince(ctx, 0);
    xl6CgenIndent(ctx);
    xl6OutputPuts("return xl6_async_result_error(pOut, \"out of memory while completing async task\");\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "*__xl6_pending_box_%d = %s;\n", targetId, resultName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "memset(&%s, 0, sizeof(%s));\n", resultName, resultName);
    xl6CgenIndent(ctx);
    xl6OutputPuts("pOut->iStatus = 0;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "pOut->pValue = __xl6_pending_box_%d;\n", targetId);
    xl6CgenIndent(ctx);
    xl6OutputPuts("pOut->sError = 0;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("pOut->pfnFreeValue = ", ctx->out);
    xl6CgenAsyncResultFreeValue(ctx, ctx->currentReturnFunc, resultType);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("pOut->pfnFreeError = 0;\n", ctx->out);
    xl6CgenEmitCleanupsSince(ctx, 0);
    xl6CgenIndent(ctx);
    xl6OutputPuts("return 0;\n", ctx->out);
}

