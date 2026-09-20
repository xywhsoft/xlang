static void xl6CgenXValueFromExprForTarget(XL6_CgenContext* ctx, const XL6_AstNode* targetType, const XL6_AstNode* expr)
{
	/* xvalue 边界必须用显式 null 值，不能把 C 空指针交给容器和协议层。 */
	if ( expr == NULL || expr->kind == XL6_AST_NULL ) {
		xl6OutputPuts("xrtValueNull()", ctx->out);
		return;
	}
    if ( xl6CgenClassHandleTargetType(ctx, targetType) != NULL && expr != NULL ) {
        XL6_AstNode* ctorClass = NULL;
        XL6_AstNode* genericCtorType = NULL;
        size_t replacementIndex;
        int isCtor = xl6CgenCallIsClassCtor(ctx, expr, &ctorClass);

        if ( !isCtor ) {
            genericCtorType = xl6CgenGenericClassCtorType(ctx, expr, &ctorClass);
            isCtor = genericCtorType != NULL && ctorClass != NULL;
        }
        if ( !isCtor ) {
            XL6_HirOwnership ownership = xl6CgenHirOwnership(ctx, expr);
            int ownershipPlanned = xl6CgenHirOwnershipIsPlanned(ctx, expr);
            int needsRetain = ownershipPlanned
                ? ownership != XL6_HIR_OWNERSHIP_MOVE
                : xl6CgenExprIsXValueBorrow(ctx, expr);

            for ( replacementIndex = 0; replacementIndex < ctx->exprReplacementCount; ++replacementIndex ) {
                const XL6_CgenExprReplacement* replacement = &ctx->exprReplacements[replacementIndex];
                if ( replacement->expr == expr && replacement->cName != NULL &&
                     (replacement->flags & XL6_CGEN_EXPR_REPLACEMENT_XVALUE) != 0 ) {
                    /* 语句级 class 临时量仍由外层清理，当前目标必须取得独立引用。 */
                    xl6OutputPuts("xl6_xrt_retain(", ctx->out);
                    xl6CgenExpr(ctx, expr);
                    xl6OutputPutc(')', ctx->out);
                    xl6AstFree(genericCtorType);
                    return;
                }
            }
            /*
                普通 class 的 C 存储就是 xvalue* 句柄。self 已在表达式层返回共享引用，
                具名变量和字段则是借用值；二者都不能再次按透明结构体装箱。
            */
            if ( expr->kind != XL6_AST_NAME || !xl6CTextEquals(expr->text, "self") ) {
                /*
                 * class 的 COPY 是复制受管理句柄，而不是复制对象 payload。
                 * 因此 COPY/SHARED/BORROW 都需要 retain；只有明确 MOVE 的
                 * 构造或返回临时值可以直接转移给被调函数。
                 */
                if ( needsRetain ) {
                    xl6OutputPuts("xl6_xrt_retain(", ctx->out);
                    xl6CgenExpr(ctx, expr);
                    xl6OutputPutc(')', ctx->out);
                } else {
                    xl6CgenExpr(ctx, expr);
                }
            } else {
                xl6CgenExpr(ctx, expr);
            }
            xl6AstFree(genericCtorType);
            return;
        }
        xl6AstFree(genericCtorType);
    }
    if ( xl6CgenIsRuntimeContainerTypeDesc(ctx, targetType) ) {
        xl6OutputPuts("xl6_xrt_bind_type(", ctx->out);
        xl6CgenXValueFromExpr(ctx, expr);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitRuntimeTypeDescRef(ctx, targetType);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    xl6CgenXValueFromExpr(ctx, expr);
}

static void xl6CgenDefaultXValueForTarget(XL6_CgenContext* ctx, const XL6_AstNode* targetType)
{
    if ( xl6CgenIsRuntimeContainerTypeDesc(ctx, targetType) ) {
        xl6OutputPuts("xl6_xrt_bind_type(", ctx->out);
        xl6CgenDefaultValue(ctx, targetType);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitRuntimeTypeDescRef(ctx, targetType);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    xl6CgenDefaultValue(ctx, targetType);
}

static int xl6CgenAwaitExprReturnsOwnedTemporaryCtx(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static const XL6_AstNode* xl6CgenTypedContainerInitLiteral(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr);
static int xl6CgenTypedContainerCanCopyFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr);
static int xl6CgenTypedContainerCanMoveFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr);

static int xl6CgenDeclShouldUseTypedContainer(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr, int declaredAuto)
{
    if ( !xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        return 0;
    }
    (void)initExpr;
    (void)declaredAuto;
    /* typed HIR 已经确定为静态泛型容器，存储层不得再按 AST 形状降级为 xvalue*。 */
    return 1;
}

static void xl6CgenTypedContainerCreateExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6TypedArrayCreate(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6TypedListCreate(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6TypedSetCreate(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6TypedDictCreate(", ctx->out);
    } else {
        xl6OutputPuts("(void*)(", ctx->out);
    }
    xl6CgenEmitXrtTypeDescRef(ctx, itemType);
    xl6OutputPuts(")", ctx->out);
}

static void xl6CgenTypedContainerRetainExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6TypedArrayRetain(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6TypedListRetain(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6TypedSetRetain(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6TypedDictRetain(", ctx->out);
    } else {
        xl6CgenError(ctx, typeNode, "typed container retain requires array/list/set/dict");
        xl6OutputPuts("0", ctx->out);
        return;
    }
    xl6CgenExpr(ctx, expr);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenTypedContainerRetainCExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cExpr)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6TypedArrayRetain(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6TypedListRetain(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6TypedSetRetain(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6TypedDictRetain(", ctx->out);
    } else {
        xl6CgenError(ctx, typeNode, "typed container retain requires array/list/set/dict");
        xl6OutputPuts("0", ctx->out);
        return;
    }
    xl6OutputPuts(cExpr != NULL ? cExpr : "0", ctx->out);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenTypedContainerCloneCExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cExpr)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6TypedArrayClone(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6TypedListClone(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6TypedSetClone(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6TypedDictClone(", ctx->out);
    } else {
        xl6CgenError(ctx, typeNode, "typed container clone requires array/list/set/dict");
        xl6OutputPuts("0", ctx->out);
        return;
    }
    xl6OutputPuts(cExpr != NULL ? cExpr : "0", ctx->out);
    xl6OutputPutc(')', ctx->out);
}

static const XL6_AstNode* xl6CgenTypedContainerInitLiteral(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( initExpr == NULL || typeName == NULL ) {
        return NULL;
    }

    if ( initExpr->kind == XL6_AST_TYPED_LITERAL ) {
        if ( xl6CTextEquals(initExpr->text, typeName) ) {
            return initExpr->childCount > 0 ? initExpr->children[0] : NULL;
        }
        return NULL;
    }
    if ( xl6CTextEquals(typeName, "array") && initExpr->kind == XL6_AST_ARRAY_LITERAL ) {
        return initExpr;
    }
    if ( xl6CTextEquals(typeName, "list") && initExpr->kind == XL6_AST_LIST_LITERAL ) {
        return initExpr;
    }
    if ( xl6CTextEquals(typeName, "set") && initExpr->kind == XL6_AST_SET_LITERAL ) {
        return initExpr;
    }
    if ( xl6CTextEquals(typeName, "dict") && initExpr->kind == XL6_AST_DICT_LITERAL ) {
        return initExpr;
    }
    return NULL;
}

static int xl6CgenFuncSingleReturnUsesTypedContainerStorage(XL6_CgenContext* ctx, const XL6_AstNode* funcNode);

static int xl6CgenFunctionTypeSingleReturnUsesTypedContainerStorage(XL6_CgenContext* ctx, const XL6_AstNode* functionType)
{
    const XL6_AstNode* returns;

    if ( ctx == NULL || !xl6CgenIsFunctionType(functionType) ) {
        return 0;
    }

    returns = xl6CgenFunctionReturnTypes(functionType);
    return returns != NULL &&
           returns->childCount == 1 &&
           xl6CgenIsTypedContainerType(ctx, returns->children[0]);
}

static int xl6CgenTypedContainerExprTypeMatches(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr)
{
    XL6_AstNode* targetType;
    XL6_AstNode* exprType;
    int result;

    targetType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
    /* typed container 的复制兼容性由真实存储类型决定。 */
    exprType = xl6CgenCloneExprStorageType(ctx, expr);
    result = targetType != NULL &&
             exprType != NULL &&
             xl6CgenIsTypedContainerType(ctx, exprType) &&
             xl6CgenStorageTypeNodeEqual(targetType, exprType);
    xl6AstFree(targetType);
    xl6AstFree(exprType);
    return result;
}

static int xl6CgenExprIsTypedContainerReturnCall(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    XL6_AstNode* func;
    XL6_AstNode* inferredType;
    int result;

    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }

    callee = expr->children[0];
    if ( callee == NULL ) {
        return 0;
    }

    if ( callee->kind == XL6_AST_NAME ) {
        XL6_AstNode* inferredArgs = NULL;
        const XL6_AstNode* prevParams;
        const XL6_AstNode* prevArgs;

        func = xl6CgenResolveCallFuncInternal(ctx, expr, callee->text, &inferredArgs, 0);
        prevParams = ctx->currentGenericParams;
        prevArgs = ctx->currentGenericArgs;
        if ( func != NULL && xl6CgenFuncIsGeneric(func) && inferredArgs != NULL ) {
            ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
            ctx->currentGenericArgs = inferredArgs;
        }
        result = xl6CgenFuncSingleReturnUsesTypedContainerStorage(ctx, func);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        xl6AstFree(inferredArgs);
        if ( result ) {
            return 1;
        }
    }

    if ( callee->kind == XL6_AST_GENERIC_INSTANT && callee->childCount > 1 ) {
        const XL6_AstNode* nameNode = callee->children[0];
        const XL6_AstNode* typeArgs = callee->children[1];
        const XL6_AstNode* prevParams;
        const XL6_AstNode* prevArgs;

        func = nameNode != NULL && nameNode->kind == XL6_AST_NAME
            ? xl6CgenResolveExplicitGenericCallFunc(ctx, expr, nameNode->text, typeArgs, 0)
            : NULL;
        prevParams = ctx->currentGenericParams;
        prevArgs = ctx->currentGenericArgs;
        if ( func != NULL && typeArgs != NULL ) {
            ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
            ctx->currentGenericArgs = typeArgs;
        }
        result = xl6CgenFuncSingleReturnUsesTypedContainerStorage(ctx, func);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        if ( result ) {
            return 1;
        }
    }

    if ( callee->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* functionType = xl6CgenCloneMemberFieldTypeForInference(ctx, callee);
        result = xl6CgenFunctionTypeSingleReturnUsesTypedContainerStorage(ctx, functionType);

        xl6AstFree(functionType);
        if ( result ) {
            return 1;
        }
    }

    /*
        typed HIR 已经给整个调用表达式记录了规范返回类型。跨源文件、导入函数、
        成员函数和泛型实例化都应以这份类型为最终事实，不能要求后端再次从
        callee AST 还原声明，否则 package 内调用会错误退回字面量初始化路径。
    */
    inferredType = xl6CgenCloneExprTypeForInference(ctx, expr);
    result = xl6CgenIsTypedContainerType(ctx, inferredType);
    xl6AstFree(inferredType);
    return result;
}

static int xl6CgenExprIsTypedContainerCopyBuiltin(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    const XL6_AstNode* argNode;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }
    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ||
         !xl6CgenIsCopyBuiltinName(callee->text) ||
         xl6CgenCopyBuiltinNameIsShadowed(ctx, callee->text) ||
         xl6CgenFindExternFunc(ctx, callee->text) != NULL ||
         xl6CgenFindExportFunc(ctx, callee->text) != NULL ) {
        return 0;
    }
    args = expr->childCount > 1 ? expr->children[1] : NULL;
    argNode = args != NULL && args->childCount == 1 ? args->children[0] : NULL;
    return argNode != NULL && argNode->kind != XL6_AST_NAMED_ARG;
}

static int xl6CgenExprIsTypedContainerProtocolIndexReturn(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* expr)
{
    if ( expr == NULL ||
         (expr->kind != XL6_AST_INDEX && expr->kind != XL6_AST_SAFE_INDEX) ) {
        return 0;
    }
    if ( !xl6CgenClassProtocolIndexHasGetter(ctx, expr) ) {
        return 0;
    }
    return xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, expr);
}

static int xl6CgenTypedContainerCanCopyFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr)
{
    if ( initExpr == NULL || xl6CgenTypedContainerInitLiteral(ctx, typeNode, initExpr) != NULL ) {
        return 0;
    }
    if ( xl6CgenHirOwnershipIsPlanned(ctx, initExpr) &&
         xl6CgenHirOwnership(ctx, initExpr) == XL6_HIR_OWNERSHIP_MOVE ) {
        return 0;
    }
    /*
     * 静态容器索引返回容器内部保存的借用句柄。目标变量必须复制该句柄
     * 指向的容器，不能把借用值当成新所有者，也不能退回字面量初始化。
     * 这一条同时覆盖 array<array<T>>、list<dict<T>> 等所有嵌套组合。
     */
    if ( initExpr->kind == XL6_AST_INDEX &&
         initExpr->childCount > 0 &&
         xl6CgenExprUsesTypedContainerStorage(ctx, initExpr->children[0]) ) {
        return xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
    }
    if ( initExpr->kind == XL6_AST_AWAIT && !xl6CgenAwaitExprReturnsOwnedTemporaryCtx(ctx, initExpr) ) {
        return xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
    }
    if ( !xl6CgenExprUsesTypedContainerStorage(ctx, initExpr) ) {
        return 0;
    }
    return xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
}

static int xl6CgenTypedContainerCanMoveFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* initExpr)
{
    XL6_AstNode* binaryResultType;
    int canMoveBinary;

    if ( initExpr == NULL || xl6CgenTypedContainerInitLiteral(ctx, typeNode, initExpr) != NULL ) {
        return 0;
    }
    if ( xl6CgenHirOwnershipIsPlanned(ctx, initExpr) &&
         xl6CgenHirOwnership(ctx, initExpr) == XL6_HIR_OWNERSHIP_MOVE ) {
        return xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
    }
    binaryResultType = NULL;
    canMoveBinary = initExpr->kind == XL6_AST_BINARY &&
                    xl6CgenTypedContainerBinaryInfo(ctx, initExpr, &binaryResultType, NULL) &&
                    xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
    xl6AstFree(binaryResultType);
    if ( canMoveBinary ) {
        return 1;
    }
    if ( initExpr->kind == XL6_AST_AWAIT ) {
        if ( !xl6CgenAwaitExprReturnsOwnedTemporaryCtx(ctx, initExpr) ) {
            return 0;
        }
    } else if ( xl6CgenExprIsTypedContainerProtocolIndexReturn(ctx, typeNode, initExpr) ) {
        return 1;
    } else if ( xl6CgenExprIsTypedContainerCopyBuiltin(ctx, initExpr) ) {
        return xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
    } else if ( !xl6CgenExprIsTypedContainerReturnCall(ctx, initExpr) ) {
        return 0;
    }
    return xl6CgenTypedContainerExprTypeMatches(ctx, typeNode, initExpr);
}

/*
    动态容器和静态泛型容器共享 array/list/set/dict 四种基础形态，
    但只有目标元素类型明确时才允许执行逐项安全解箱。普通 var 不参与隐式转换。
*/
static int xl6CgenTypedContainerCanImportDynamicExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* targetType;
    XL6_AstNode* exprType;
    int result;

    if ( initExpr == NULL ||
         !xl6CgenIsTypedContainerType(ctx, typeNode) ||
         xl6CgenTypedContainerInitLiteral(ctx, typeNode, initExpr) != NULL ) {
        return 0;
    }
    targetType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    exprType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
    result = targetType != NULL &&
             exprType != NULL &&
             xl6CgenIsXValueType(exprType) &&
             !xl6CTextEquals(exprType->text, "var") &&
             xl6CTextEquals(targetType->text, exprType->text);
    xl6AstFree(exprType);
    return result;
}

static void xl6CgenTypedContainerImportDynamicExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6_xrt_typed_array_from_value_release(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6_xrt_typed_list_from_value_release(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6_xrt_typed_set_from_value_release(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6_xrt_typed_dict_from_value_release(", ctx->out);
    } else {
        xl6CgenError(ctx, expr, "dynamic container import requires array/list/set/dict");
        xl6OutputPuts("0", ctx->out);
        return;
    }
    xl6CgenEmitXrtTypeDescRef(ctx, itemType);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenXValueFromExpr(ctx, expr);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenEmitExceptionPropagate(XL6_CgenContext* ctx, const XL6_AstNode* node);

static void xl6CgenTypedContainerImportCheckStmt(
    XL6_CgenContext* ctx,
    const char* cName,
    const XL6_AstNode* at)
{
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s == 0) {\n", cName != NULL ? cName : "0");
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    if ( ctx->needsErrorRuntime ) {
        xl6OutputPrintf(ctx->out,
                        "xl6_error_raise(\"TypeError\", \"dynamic container contains an incompatible value\", \"\", %d, %d);\n",
                        at != NULL ? at->line : 0,
                        at != NULL ? at->column : 0);
        xl6CgenEmitExceptionPropagate(ctx, at);
    } else {
        xl6OutputPuts("xl6_runtime_fatal(3);\n", ctx->out);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenTypedContainerCopyExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6_xrt_typed_array_copy(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6_xrt_typed_list_copy(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6_xrt_typed_set_copy(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6_xrt_typed_dict_copy(", ctx->out);
    } else {
        xl6CgenError(ctx, expr, "typed container copy requires array/list/set/dict");
        xl6OutputPuts("0", ctx->out);
        return;
    }
    xl6CgenEmitXrtTypeDescRef(ctx, itemType);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPutc(')', ctx->out);
}


// 已经求值到 C 临时变量的容器，也必须能按目标对象模式复制
static void xl6CgenTypedContainerCopyCExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cExpr)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6_xrt_typed_array_copy(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6_xrt_typed_list_copy(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6_xrt_typed_set_copy(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6_xrt_typed_dict_copy(", ctx->out);
    } else {
        xl6OutputPuts("0", ctx->out);
        return;
    }
    xl6CgenEmitXrtTypeDescRef(ctx, itemType);
    xl6OutputPuts(", ", ctx->out);
    xl6OutputPuts(cExpr != NULL ? cExpr : "0", ctx->out);
    xl6OutputPutc(')', ctx->out);
}
