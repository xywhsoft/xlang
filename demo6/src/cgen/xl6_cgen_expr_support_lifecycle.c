/* 泛型字段是否需要释放，必须在当前实例的类型参数环境中判断。 */
static int xl6CgenGenericClassNeedsDestroy(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    int result;

    if ( ctx == NULL || classNode == NULL || typeNode == NULL || typeNode->childCount == 0 ) {
        return xl6CgenClassNeedsDestroy(ctx, classNode);
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];
    result = xl6CgenClassNeedsDestroy(ctx, classNode);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    return result;
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
        return "xl6TypedArrayClone";
    }
    if ( xl6CTextEquals(typeName, "list") ) {
        return "xl6TypedListClone";
    }
    if ( xl6CTextEquals(typeName, "set") ) {
        return "xl6TypedSetClone";
    }
    if ( xl6CTextEquals(typeName, "dict") ) {
        return "xl6TypedDictClone";
    }
    return NULL;
}

static const char* xl6CgenTypedContainerDestroyFuncName(const char* typeName)
{
    if ( xl6CTextEquals(typeName, "array") ) {
        return "xl6TypedArrayRelease";
    }
    if ( xl6CTextEquals(typeName, "list") ) {
        return "xl6TypedListRelease";
    }
    if ( xl6CTextEquals(typeName, "set") ) {
        return "xl6TypedSetRelease";
    }
    if ( xl6CTextEquals(typeName, "dict") ) {
        return "xl6TypedDictRelease";
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
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtValueRetain(%s); }\n", selfRefPath, selfRefPath);
        } else {
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtValueRelease(%s); %s = 0; }\n", selfRefPath, selfRefPath, selfRefPath);
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
        typeNode = xl6CgenSubstituteGenericType(ctx, member->children[0]);
        xl6CgenCIdentText(cFieldName, sizeof(cFieldName), member->text);
        snprintf(fieldPath, sizeof(fieldPath), "%s.%s", valuePath, cFieldName);
        if ( xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
            xl6CgenIndent(ctx);
            if ( retain ) {
                if ( xl6CgenIsNullableStringTypeNode(ctx, typeNode) ) {
                    xl6OutputPrintf(
                        ctx->out,
                        "%s = xl6_nullable_string_copy(%s);\n",
                        fieldPath,
                        fieldPath);
                } else {
                    xl6OutputPrintf(
                        ctx->out,
                        "%s = xl6_xrt_string_dup(%s, 0u);\n",
                        fieldPath,
                        fieldPath);
                }
            } else {
                xl6OutputPrintf(ctx->out, "xrtFree((void*)%s); %s = 0;\n", fieldPath, fieldPath);
            }
            continue;
        }
        if ( xl6CgenIsErrorType(ctx, typeNode) ) {
            xl6CgenIndent(ctx);
            if ( retain ) {
                xl6OutputPrintf(ctx->out, "%s = xl6_error_retain(%s);\n", fieldPath, fieldPath);
            } else {
                xl6OutputPrintf(ctx->out, "if (%s != 0) { xl6_error_free(%s); %s = 0; }\n", fieldPath, fieldPath, fieldPath);
            }
            continue;
        }
        if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
            xl6CgenTypedContainerFieldRef(ctx, typeNode, fieldPath, retain);
            continue;
        }
        if ( xl6CgenIsSpecialCollectionType(ctx, typeNode) ) {
            xl6CgenIndent(ctx);
            if ( retain ) {
                xl6OutputPuts(fieldPath, ctx->out);
                xl6OutputPuts(" = ", ctx->out);
                xl6CgenSpecialCollectionRetainCExpr(ctx, typeNode, fieldPath);
                xl6OutputPuts(";\n", ctx->out);
            } else {
                xl6CgenSpecialCollectionReleaseCExpr(ctx, typeNode, fieldPath);
                xl6OutputPuts("; ", ctx->out);
                xl6OutputPuts(fieldPath, ctx->out);
                xl6OutputPuts(" = 0;\n", ctx->out);
            }
            continue;
        }
        if ( xl6CgenIsWeakType(typeNode) ) {
            xl6CgenIndent(ctx);
            if ( retain ) {
                xl6OutputPrintf(
                    ctx->out,
                    "%s = xl6WeakClone(&%s);\n",
                    fieldPath,
                    fieldPath);
            } else {
                xl6OutputPrintf(ctx->out, "xl6WeakUnit(&%s);\n", fieldPath);
            }
            continue;
        }
        if ( xl6CgenIsXValueType(typeNode) ) {
            xl6CgenIndent(ctx);
            if ( retain ) {
                const char* copyFunc = deep ? "xrtValueDeepClone" : (copyXValue ? "xrtValueClone" : "xl6_xrt_retain");
                xl6OutputPrintf(ctx->out, "%s = %s(%s);\n", fieldPath, copyFunc, fieldPath);
            } else {
                xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtValueRelease(%s); %s = 0; }\n", fieldPath, fieldPath, fieldPath);
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
        xl6OutputPrintf(ctx->out, "if (%s.__xl6_self_ref != 0) { xrtValueRetain(%s.__xl6_self_ref); }\n", valuePath, valuePath);
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

    /*
        所有以 xvalue* 保存的类型都遵守同一条借用提升规则。
        这里不能只识别 class：protocol、var、nullable 和部分泛型实例同样以
        xvalue* 保存。循环或 future 从容器中借用它们后，局部变量会在作用域结束时
        unref，因此必须先取得自己的强引用。
    */
    if ( xl6CTextEquals(xl6CgenValueStorageCType(ctx, typeNode), "xvalue*") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtValueRetain(%s); }\n", valuePath, valuePath);
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
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", valuePath);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "(void)xl6_temp_managed_disown((void*)%s, (XL6_TempManagedDrop)xrtValueRelease);\n",
            valuePath);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xrtValueRelease(%s);\n", valuePath);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", valuePath);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
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

    /* class 句柄由 xvalue* 类型操作统一管理内部字段，不能按结构体再次 retain/drop。 */
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
    if ( xl6CgenIsErrorType(ctx, typeNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xl6_error_free(%s);\n", valuePath);
        return;
    }
    if ( xl6CgenIsSpecialCollectionType(ctx, typeNode) ) {
        xl6CgenIndent(ctx);
        xl6CgenSpecialCollectionReleaseCExpr(ctx, typeNode, valuePath);
        xl6OutputPuts(";\n", ctx->out);
        return;
    }
    if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        xl6CgenIndent(ctx);
        xl6CgenTypedContainerDestroyValue(ctx, typeNode, valuePath);
        return;
    }
    if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6CgenReleaseFunctionPath(ctx, valuePath);
        return;
    }
    if ( xl6CgenNullableHasManagedInner(ctx, typeNode) ) {
        char dropName[64];
        xl6CgenNullableDropHelperName(dropName, sizeof(dropName), typeNode);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s(&%s);\n", dropName, valuePath);
        return;
    }
    if ( xl6CgenClassTypeNeedsFunctionFieldCleanup(ctx, typeNode) ) {
        xl6CgenReleaseClassFunctionFieldsForType(ctx, typeNode, valuePath);
    }
}
