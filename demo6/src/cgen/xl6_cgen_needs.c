static void xl6CgenNeedsAddDeclVarType(
    XL6_CgenContext* ctx,
    const char* name,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* initExpr,
    int declaredAuto)
{
    unsigned int flags;

    flags = xl6CgenDeclShouldUseTypedContainer(ctx, typeNode, initExpr, declaredAuto)
        ? XL6_CGEN_VAR_TYPED_CONTAINER
        : 0u;
    xl6CgenAddVarTypeWithFlags(ctx, name, typeNode, flags);
}

static int xl6CgenNeedsTypeIsString(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    if ( ctx == NULL || typeNode == NULL ) {
        return 0;
    }
    return xl6CgenIsOwnedStringTypeNode(ctx, typeNode);
}



/*
	typed 容器循环会在 lowering 阶段复制 dict 键和 string 元素。
	这些复制不是源 AST 中的显式字符串表达式，必须在裁剪扫描阶段单独纳入依赖。
*/
static int xl6CgenNeedsTypedForStringCopy(
	XL6_CgenContext* ctx,
	const XL6_AstNode* node)
{
	const XL6_AstNode* concreteType;
	const XL6_AstNode* itemType;
	XL6_AstNode* iterableType;
	int result;

	if ( (ctx == NULL) ||
		(node == NULL) ||
		(node->kind != XL6_AST_FOR) ||
		(node->childCount < 2) ) {
		return 0;
	}

	iterableType = xl6CgenCloneExprTypeForInference(ctx, node->children[1]);
	if ( !xl6CgenIsTypedContainerType(ctx, iterableType) ) {
		xl6AstFree(iterableType);
		return 0;
	}

	concreteType = xl6CgenTypedContainerConcreteType(ctx, iterableType);
	itemType = xl6CgenTypedContainerItemType(ctx, iterableType);
	result = ((concreteType != NULL) &&
		xl6CTextEquals(concreteType->text, "dict")) ||
		xl6CgenIsStringTypeNode(ctx, itemType);
	xl6AstFree(iterableType);
	return result;
}

static int xl6CgenNeedsParamListHasString(
    XL6_CgenContext* ctx,
    const XL6_AstNode* params)
{
    size_t i;

    if ( ctx == NULL || params == NULL ) {
        return 0;
    }

    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];

        if ( param != NULL && param->childCount > 0 &&
             xl6CgenNeedsTypeIsString(ctx, param->children[0]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNeedsClassValueDeriveStringTempCopy(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    if ( ctx == NULL || classNode == NULL || !xl6CgenClassHasValueDerive(classNode) ) {
        return 0;
    }

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];

        if ( member == NULL ||
             member->kind != XL6_AST_FIELD ||
             xl6CgenIsStaticMember(member) ||
             member->childCount == 0 ) {
            continue;
        }

        /*
            自动 value/json/xson 协议会从 xvalue* 读回 string 字段。
            这里显式拉起 string runtime，避免协议生成代码依赖未输出的临时字符串拷贝 helper。
        */
        if ( xl6CgenNeedsTypeIsString(ctx, member->children[0]) ) {
            return 1;
        }
    }

    return 0;
}

static int xl6CgenNeedsReturnListHasString(XL6_CgenContext* ctx, const XL6_AstNode* returns)
{
    size_t i;

    if ( ctx == NULL || returns == NULL ) {
        return 0;
    }

    for ( i = 0; i < returns->childCount; ++i ) {
        if ( xl6CgenNeedsTypeIsString(ctx, returns->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNodeNeedsStringRuntime(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsTypedForStringCopy(ctx, node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_INTERP_STRING ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL &&
         (xl6CgenCallIsExternalStringReturn(ctx, node) ||
          (xl6CgenExprHasStaticValueType(ctx, node) &&
           xl6CgenExprStaticTypeIs(ctx, node, "string"))) ) {
        return 1;
    }
    if ( xl6CgenBinaryPlusIsString(ctx, node) ) {
        return 1;
    }
    if ( xl6CgenBinaryCompareIsString(ctx, node) || xl6CgenBinaryApproxIsString(ctx, node) ) {
        return 1;
    }
    if ( xl6CgenBinaryInIsString(ctx, node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_ASSIGN && xl6CTextEquals(node->text, "+=") &&
         node->childCount > 1 &&
         xl6CgenExprStaticTypeIs(ctx, node->children[0], "string") &&
         xl6CgenExprStaticTypeIs(ctx, node->children[1], "string") ) {
        return 1;
    }
    if ( (node->kind == XL6_AST_INDEX ||
          node->kind == XL6_AST_SAFE_INDEX ||
          node->kind == XL6_AST_MEMBER ||
          node->kind == XL6_AST_SAFE_MEMBER) &&
         xl6CgenExprHasStaticValueType(ctx, node) &&
         xl6CgenExprStaticTypeIs(ctx, node, "string") ) {
        return 1;
    }

    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        const XL6_AstNode* typeNode = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* initExpr = node->childCount > 1 ? node->children[1] : NULL;

        if ( initExpr != NULL && xl6CgenNodeNeedsStringRuntime(ctx, initExpr) ) {
            return 1;
        }
        if ( xl6CgenNeedsTypeIsString(ctx, typeNode) ) {
            return 1;
        }
        if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") ) {
            XL6_AstNode* inferredType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
            if ( inferredType != NULL ) {
                xl6CgenOwnTypeNode(ctx, inferredType);
                xl6CgenNeedsAddDeclVarType(ctx, node->text, inferredType, initExpr, 1);
            }
        } else if ( typeNode != NULL ) {
            xl6CgenNeedsAddDeclVarType(ctx, node->text, typeNode, initExpr, 0);
        }
        return 0;
    }

    if ( node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC || node->kind == XL6_AST_EXPORT_FUNC || node->kind == XL6_AST_ANON_FUNC ) {
        const XL6_AstNode* params = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 0 : 1)
                                        ? node->children[node->kind == XL6_AST_ANON_FUNC ? 0 : 1]
                                        : NULL;
        const XL6_AstNode* returns = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 1 : 2)
                                         ? node->children[node->kind == XL6_AST_ANON_FUNC ? 1 : 2]
                                         : NULL;
        const XL6_AstNode* body = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 2 : 3)
                                      ? node->children[node->kind == XL6_AST_ANON_FUNC ? 2 : 3]
                                      : NULL;
        XL6_CgenAnonFunc* anonInfo = node->kind == XL6_AST_ANON_FUNC ? xl6CgenFindAnonFuncInfo(ctx, node) : NULL;
        const XL6_CgenAnonFunc* prevAnon = ctx->currentAnonFunc;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;
        int returnNeedsString;

        if ( (node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC) && xl6CgenFuncIsGeneric(node) && ctx->currentGenericParams == NULL ) {
            return 0;
        }
        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentNativeClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentNativeClass);
        }
        returnNeedsString = xl6CgenNeedsParamListHasString(ctx, params) ||
                            xl6CgenNeedsReturnListHasString(ctx, returns);
        xl6CgenRegisterParamTypes(ctx, params);
        ctx->currentAnonFunc = anonInfo;
        xl6CgenRegisterAnonCaptureTypes(ctx, anonInfo);
        result = returnNeedsString || xl6CgenNodeNeedsStringRuntime(ctx, body);
        ctx->currentAnonFunc = prevAnon;
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_TRY ) {
        const XL6_AstNode* tryBlock = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* catchList = node->childCount > 1 ? node->children[1] : NULL;
        const XL6_AstNode* finallyBlock = node->childCount > 2 ? node->children[2] : NULL;
        size_t i;

        if ( xl6CgenNodeNeedsStringRuntime(ctx, tryBlock) ) {
            return 1;
        }
        for ( i = 0; catchList != NULL && catchList->kind == XL6_AST_LIST && i < catchList->childCount; ++i ) {
            const XL6_AstNode* catchNode = catchList->children[i];
            const XL6_AstNode* catchName;
            const XL6_AstNode* catchBlock;
            const XL6_AstNode* errorType;
            size_t varMark = ctx->varCount;
            size_t cleanupMark = ctx->cleanupCount;
            int result;

            if ( catchNode == NULL || catchNode->kind != XL6_AST_CATCH ) {
                continue;
            }
            catchName = catchNode->childCount == 3
                ? catchNode->children[1]
                : (catchNode->childCount > 0 ? catchNode->children[0] : NULL);
            catchBlock = catchNode->childCount == 3
                ? catchNode->children[2]
                : (catchNode->childCount > 1 ? catchNode->children[1] : NULL);
            errorType = xl6CgenOwnSimpleType(ctx, "error", catchName != NULL ? catchName : catchNode);
            xl6CgenAddVarType(ctx, catchName != NULL ? catchName->text : "__xl6_error", errorType);
            result = xl6CgenNodeNeedsStringRuntime(ctx, catchBlock);
            xl6CgenFreeCleanupsSince(ctx, cleanupMark);
            xl6CgenFreeVarsSince(ctx, varMark);
            if ( result ) {
                return 1;
            }
        }
        return xl6CgenNodeNeedsStringRuntime(ctx, finallyBlock);
    }
    if ( node->kind == XL6_AST_CREATE ) {
        const XL6_AstNode* params = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* body = node->childCount > 1 ? node->children[1] : NULL;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentNativeClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentNativeClass);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsStringRuntime(ctx, body);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_CLASS ) {
        const XL6_AstNode* prevClass = ctx->currentClass;
        const XL6_AstNode* prevNativeClass = ctx->currentNativeClass;
        int result = 0;

        ctx->currentClass = node;
        ctx->currentNativeClass = NULL;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsStringRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentNativeClass = prevNativeClass;
        return result;
    }


    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsStringRuntime(ctx, node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenProgramNeedsStringRuntime(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    size_t i;

    if ( xl6CgenNodeNeedsStringRuntime(ctx, root) ) {
        xl6CgenFreeCleanupsSince(ctx, 0);
        xl6CgenFreeVarsSince(ctx, 0);
        return 1;
    }
    xl6CgenFreeCleanupsSince(ctx, 0);
    xl6CgenFreeVarsSince(ctx, 0);
    for ( i = 0; i < ctx->genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
        int result;

        ctx->currentGenericParams = xl6CgenFuncTypeParams(ctx->genericInstances[i].func);
        ctx->currentGenericArgs = ctx->genericInstances[i].typeArgs;
        result = xl6CgenNodeNeedsStringRuntime(ctx, ctx->genericInstances[i].func);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        xl6CgenFreeCleanupsSince(ctx, 0);
        xl6CgenFreeVarsSince(ctx, 0);
        if ( result ) {
            return 1;
        }
    }
    return 0;
}




/*
	动态算术 helper 只服务于实际以 xvalue* 求值的二元表达式。
	类型描述和 class 句柄虽然也需要基础 XValue ABI，但不应因此带入字符串拼接路径。
*/
static int xl6CgenNodeNeedsXValueBinaryRuntime(
	XL6_CgenContext* ctx,
	const XL6_AstNode* node)
{
	size_t i;

	if ( (ctx == NULL) || (node == NULL) ) {
		return 0;
	}
	if ( (node->kind == XL6_AST_BINARY) &&
		xl6CgenBinaryArithmeticIsXValue(ctx, node) ) {
		return 1;
	}
	for ( i = 0; i < node->childCount; ++i ) {
		if ( xl6CgenNodeNeedsXValueBinaryRuntime(ctx, node->children[i]) ) {
			return 1;
		}
	}
	return 0;
}

static int xl6CgenTypeConvertCallNeedsRuntime(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* arg;
    XL6_AstNode* sourceType;
    const char* target;
    const char* source;
    int result;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ||
         node->children[0] == NULL || node->children[0]->kind != XL6_AST_NAME ) {
        return 0;
    }
    callee = node->children[0];
    target = callee->text;
    if ( !xl6CgenIsBuiltinConvertTypeName(target) || !xl6CgenIsKnownTypeName(ctx, target) ) {
        return 0;
    }

    arg = xl6CgenTypeConvertArg(node);
    sourceType = xl6CgenCloneExprTypeForInference(ctx, arg);
    source = sourceType != NULL ? sourceType->text : NULL;
    result = 0;

    /*
        var/xvalue* 到静态标量的转换会生成 xl6_xrt_value_to_* helper。
        needs 必须以真实存储类型判断，不能只枚举旧的字符串/数值转换组合。
    */
    if ( xl6CgenIsXValueType(sourceType) &&
         (xl6CgenIsIntegerTypeName(target) ||
          xl6CgenIsFloatTypeName(target) ||
          xl6CgenIsBoolTypeName(target) ||
          xl6CTextEquals(target, "string") ||
          xl6CTextEquals(target, "time") ||
          xl6CTextEquals(target, "point")) ) {
        result = 1;
    } else if ( xl6CgenIsIntegerTypeName(target) && xl6CgenIsFloatTypeName(source) ) {
        result = 1;
    } else if ( (xl6CgenIsIntegerTypeName(target) || xl6CgenIsFloatTypeName(target) || xl6CTextEquals(target, "time")) &&
                xl6CTextEquals(source, "string") ) {
        result = 1;
    } else if ( xl6CTextEquals(target, "string") &&
                (xl6CgenIsIntegerTypeName(source) || xl6CgenIsFloatTypeName(source) ||
                 xl6CTextEquals(source, "time") || xl6CTextEquals(source, "point") ||
                 xl6CTextEquals(source, "type") || xl6CTextEquals(source, "var")) ) {
        result = 1;
    }

    xl6AstFree(sourceType);
    return result;
}

static int xl6CgenCallNeedsXValueScalarBridge(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call)
{
    const XL6_AstNode* target;
    const XL6_AstNode* params;
    const XL6_AstNode* args;
    int fixedCount;
    int i;

    if ( ctx == NULL || call == NULL || call->kind != XL6_AST_CALL ) {
        return 0;
    }
    target = xl6CgenHirTarget(ctx, call);
    params = xl6CgenCallableParams(target);
    args = call->childCount > 1 ? call->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params != NULL && (size_t)i < params->childCount
            ? params->children[i]
            : NULL;
        const XL6_AstNode* targetType = param != NULL && param->childCount > 0
            ? param->children[0]
            : NULL;
        const XL6_AstNode* expr = param != NULL
            ? xl6CgenFindArgForParam(args, param->text, i)
            : NULL;

        if ( xl6CgenExprNeedsXValueScalarBridge(ctx, targetType, expr) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenAssignNeedsXValueScalarBridge(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    XL6_AstNode* targetType;
    int result;

    if ( ctx == NULL || node == NULL || node->kind != XL6_AST_ASSIGN ||
         node->childCount < 2 ) {
        return 0;
    }
    targetType = xl6CgenCloneExprTypeForInference(ctx, node->children[0]);
    result = xl6CgenExprNeedsXValueScalarBridge(
        ctx,
        targetType,
        node->children[1]);
    xl6AstFree(targetType);
    return result;
}

static int xl6CgenReturnNeedsXValueScalarBridge(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    size_t i;

    if ( ctx == NULL || node == NULL || node->kind != XL6_AST_RETURN ||
         ctx->currentReturns == NULL ||
         node->childCount != ctx->currentReturns->childCount ) {
        return 0;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenExprNeedsXValueScalarBridge(
                ctx,
                ctx->currentReturns->children[i],
                node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNodeNeedsTypeConvertRuntime(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenIsImplicitToStringCall(ctx, node) ) {
        return 1;
    }
    if ( xl6CgenTypeConvertCallNeedsRuntime(ctx, node) ) {
        return 1;
    }
    if ( xl6CgenCallNeedsXValueScalarBridge(ctx, node) ||
         xl6CgenAssignNeedsXValueScalarBridge(ctx, node) ||
         xl6CgenReturnNeedsXValueScalarBridge(ctx, node) ) {
        return 1;
    }
    if ( xl6CgenFStringNeedsTypeConvertRuntime(ctx, node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         xl6CTextEquals(node->children[0]->text, "__xl_std_runtime_take_cstr") ) {
        return 1;
    }

    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        const XL6_AstNode* typeNode = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* initExpr = node->childCount > 1 ? node->children[1] : NULL;

        if ( initExpr != NULL && xl6CgenNodeNeedsTypeConvertRuntime(ctx, initExpr) ) {
            return 1;
        }
        if ( xl6CgenExprNeedsXValueScalarBridge(ctx, typeNode, initExpr) ) {
            return 1;
        }
        if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") ) {
            XL6_AstNode* inferredType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
            if ( inferredType != NULL ) {
                xl6CgenOwnTypeNode(ctx, inferredType);
                xl6CgenNeedsAddDeclVarType(ctx, node->text, inferredType, initExpr, 1);
            }
        } else if ( typeNode != NULL ) {
            xl6CgenNeedsAddDeclVarType(ctx, node->text, typeNode, initExpr, 0);
        }
        return 0;
    }

    if ( node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC || node->kind == XL6_AST_EXPORT_FUNC || node->kind == XL6_AST_ANON_FUNC ) {
        const XL6_AstNode* params = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 0 : 1)
                                        ? node->children[node->kind == XL6_AST_ANON_FUNC ? 0 : 1]
                                        : NULL;
        const XL6_AstNode* body = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 2 : 3)
                                      ? node->children[node->kind == XL6_AST_ANON_FUNC ? 2 : 3]
                                      : NULL;
        XL6_CgenAnonFunc* anonInfo = node->kind == XL6_AST_ANON_FUNC ? xl6CgenFindAnonFuncInfo(ctx, node) : NULL;
        const XL6_CgenAnonFunc* prevAnon = ctx->currentAnonFunc;
        const XL6_AstNode* prevReturns = ctx->currentReturns;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( (node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC) && xl6CgenFuncIsGeneric(node) && ctx->currentGenericParams == NULL ) {
            return 0;
        }
        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentNativeClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentNativeClass);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        ctx->currentAnonFunc = anonInfo;
        ctx->currentReturns = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 1 : 2)
            ? node->children[node->kind == XL6_AST_ANON_FUNC ? 1 : 2]
            : NULL;
        xl6CgenRegisterAnonCaptureTypes(ctx, anonInfo);
        result = xl6CgenNodeNeedsTypeConvertRuntime(ctx, body);
        ctx->currentReturns = prevReturns;
        ctx->currentAnonFunc = prevAnon;
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_CREATE ) {
        const XL6_AstNode* params = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* body = node->childCount > 1 ? node->children[1] : NULL;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentNativeClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentNativeClass);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsTypeConvertRuntime(ctx, body);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_FIELD ) {
        const XL6_AstNode* typeNode = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* initExpr = node->childCount > 1 ? node->children[1] : NULL;

        if ( xl6CgenExprNeedsXValueScalarBridge(ctx, typeNode, initExpr) ) {
            return 1;
        }
    }

    if ( node->kind == XL6_AST_FOR ) {
        const XL6_AstNode* targets = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* iterable = node->childCount > 1 ? node->children[1] : NULL;
        const XL6_AstNode* body = node->childCount > 2 ? node->children[2] : NULL;
        XL6_AstNode* iterableType;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        iterableType = xl6CgenCloneExprTypeForInference(ctx, iterable);
        if ( targets != NULL ) {
            for ( i = 0; i < targets->childCount; ++i ) {
                const XL6_AstNode* storageType =
                    xl6CgenForTargetStorageType(ctx, node, i);
                xl6CgenRegisterLoopTargetTypeNode(
                    ctx,
                    targets->children[i],
                    storageType);
            }
        }
        result = xl6CgenNodeNeedsTypeConvertRuntime(ctx, body);
        xl6AstFree(iterableType);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_CLASS ) {
        const XL6_AstNode* prevClass = ctx->currentClass;
        const XL6_AstNode* prevNativeClass = ctx->currentNativeClass;
        int result = 0;

        ctx->currentClass = node;
        ctx->currentNativeClass = NULL;
        if ( xl6CgenNeedsClassValueDeriveStringTempCopy(ctx, node) ) {
            result = 1;
        }
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsTypeConvertRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentNativeClass = prevNativeClass;
        return result;
    }


    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTypeConvertRuntime(ctx, node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenProgramNeedsTypeConvertRuntime(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    size_t i;

    if ( xl6CgenNodeNeedsTypeConvertRuntime(ctx, root) ) {
        xl6CgenFreeCleanupsSince(ctx, 0);
        xl6CgenFreeVarsSince(ctx, 0);
        return 1;
    }
    xl6CgenFreeCleanupsSince(ctx, 0);
    xl6CgenFreeVarsSince(ctx, 0);
    for ( i = 0; i < ctx->genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
        int result;

        ctx->currentGenericParams = xl6CgenFuncTypeParams(ctx->genericInstances[i].func);
        ctx->currentGenericArgs = ctx->genericInstances[i].typeArgs;
        result = xl6CgenNodeNeedsTypeConvertRuntime(ctx, ctx->genericInstances[i].func);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        xl6CgenFreeCleanupsSince(ctx, 0);
        xl6CgenFreeVarsSince(ctx, 0);
        if ( result ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNodeNeedsPointMove(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CLASS &&
         xl6CgenClassHasNativeHandle(node) &&
         xl6CgenNativeBackedClassHasDestroy(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsPointMove(ctx, node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNodeNeedsDynamicForInErrorRuntime(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }

    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        const XL6_AstNode* typeNode = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* initExpr = node->childCount > 1 ? node->children[1] : NULL;

        if ( initExpr != NULL && xl6CgenNodeNeedsDynamicForInErrorRuntime(ctx, initExpr) ) {
            return 1;
        }
        if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") ) {
            XL6_AstNode* inferredType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
            if ( inferredType != NULL ) {
                xl6CgenOwnTypeNode(ctx, inferredType);
                xl6CgenNeedsAddDeclVarType(ctx, node->text, inferredType, initExpr, 1);
            }
        } else if ( typeNode != NULL ) {
            xl6CgenNeedsAddDeclVarType(ctx, node->text, typeNode, initExpr, 0);
        }
        return 0;
    }

    if ( node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC || node->kind == XL6_AST_EXPORT_FUNC || node->kind == XL6_AST_ANON_FUNC ) {
        const XL6_AstNode* params = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 0 : 1)
                                        ? node->children[node->kind == XL6_AST_ANON_FUNC ? 0 : 1]
                                        : NULL;
        const XL6_AstNode* body = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 2 : 3)
                                      ? node->children[node->kind == XL6_AST_ANON_FUNC ? 2 : 3]
                                      : NULL;
        XL6_CgenAnonFunc* anonInfo = node->kind == XL6_AST_ANON_FUNC ? xl6CgenFindAnonFuncInfo(ctx, node) : NULL;
        const XL6_CgenAnonFunc* prevAnon = ctx->currentAnonFunc;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( (node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC) && xl6CgenFuncIsGeneric(node) && ctx->currentGenericParams == NULL ) {
            return 0;
        }
        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentNativeClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentNativeClass);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        ctx->currentAnonFunc = anonInfo;
        xl6CgenRegisterAnonCaptureTypes(ctx, anonInfo);
        result = xl6CgenNodeNeedsDynamicForInErrorRuntime(ctx, body);
        ctx->currentAnonFunc = prevAnon;
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_CREATE ) {
        const XL6_AstNode* params = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* body = node->childCount > 1 ? node->children[1] : NULL;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentNativeClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentNativeClass);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsDynamicForInErrorRuntime(ctx, body);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_FOR ) {
        const XL6_AstNode* targets = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* iterable = node->childCount > 1 ? node->children[1] : NULL;
        const XL6_AstNode* body = node->childCount > 2 ? node->children[2] : NULL;
        XL6_AstNode* iterableType;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        iterableType = xl6CgenCloneExprTypeForInference(ctx, iterable);
        if ( iterableType != NULL && xl6CTextEquals(iterableType->text, "var") ) {
            xl6AstFree(iterableType);
            return 1;
        }
        if ( targets != NULL ) {
            for ( i = 0; i < targets->childCount; ++i ) {
                const XL6_AstNode* storageType =
                    xl6CgenForTargetStorageType(ctx, node, i);
                xl6CgenRegisterLoopTargetTypeNode(
                    ctx,
                    targets->children[i],
                    storageType);
            }
        }
        result = xl6CgenNodeNeedsDynamicForInErrorRuntime(ctx, body);
        xl6AstFree(iterableType);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_CLASS ) {
        const XL6_AstNode* prevClass = ctx->currentClass;
        const XL6_AstNode* prevNativeClass = ctx->currentNativeClass;
        int result = 0;

        ctx->currentClass = node;
        ctx->currentNativeClass = NULL;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsDynamicForInErrorRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentNativeClass = prevNativeClass;
        return result;
    }


    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsDynamicForInErrorRuntime(ctx, node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenProgramNeedsDynamicForInErrorRuntime(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    size_t i;

    if ( xl6CgenNodeNeedsDynamicForInErrorRuntime(ctx, root) ) {
        xl6CgenFreeCleanupsSince(ctx, 0);
        xl6CgenFreeVarsSince(ctx, 0);
        return 1;
    }
    xl6CgenFreeCleanupsSince(ctx, 0);
    xl6CgenFreeVarsSince(ctx, 0);
    for ( i = 0; i < ctx->genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
        int result;

        ctx->currentGenericParams = xl6CgenFuncTypeParams(ctx->genericInstances[i].func);
        ctx->currentGenericArgs = ctx->genericInstances[i].typeArgs;
        result = xl6CgenNodeNeedsDynamicForInErrorRuntime(ctx, ctx->genericInstances[i].func);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        xl6CgenFreeCleanupsSince(ctx, 0);
        xl6CgenFreeVarsSince(ctx, 0);
        if ( result ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNodeNeedsFunctionVarRuntime(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }

    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME &&
         xl6CTextEquals(node->children[0]->text, "xcall") ) {
        return 1;
    }

    if ( xl6CgenIsTaskNamespaceMemberCall(node, NULL) ) {
        const XL6_AstNode* argExpr = xl6CgenTaskCallArgExpr(node);
        XL6_AstNode* valueType = xl6CgenCloneTaskArrayItemFutureValueType(ctx, argExpr);
        int needsRuntime = xl6CgenIsFunctionType(valueType);
        xl6AstFree(valueType);
        if ( needsRuntime ) {
            return 1;
        }
    }

    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME ) {
        const XL6_AstNode* functionType = xl6CgenFindVarType(ctx, node->children[0]->text);
        const XL6_AstNode* callableType = xl6CgenResolveAliasType(ctx, functionType);

        if ( xl6CgenIsNullableType(callableType) ) {
            callableType = xl6CgenResolveAliasType(
                ctx,
                xl6CgenNullableInnerType(callableType)
            );
        }
        if ( xl6CgenIsFunctionType(callableType) ||
             (functionType != NULL && xl6CTextEquals(functionType->text, "var")) ) {
            return 1;
        }
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_MEMBER &&
         xl6CgenExprHasStaticValueType(ctx, node->children[0]) ) {
        XL6_AstNode* memberType = xl6CgenCloneMemberFieldTypeForInference(ctx, node->children[0]);
        const XL6_AstNode* callableType = xl6CgenResolveAliasType(ctx, memberType);
        int needsRuntime;

        if ( xl6CgenIsNullableType(callableType) ) {
            callableType = xl6CgenResolveAliasType(
                ctx,
                xl6CgenNullableInnerType(callableType)
            );
        }
        needsRuntime = xl6CgenIsFunctionType(callableType) ||
                       (memberType != NULL && xl6CTextEquals(memberType->text, "var"));
        xl6AstFree(memberType);
        if ( needsRuntime ) {
            return 1;
        }
    }

    /* CALL with bare function name argument passed to var parameter */
    if ( node->kind == XL6_AST_CALL && node->childCount > 1 && node->children[1] != NULL ) {
        const XL6_AstNode* args = node->children[1];
        size_t argIdx;
        for ( argIdx = 0; argIdx < args->childCount; ++argIdx ) {
            const XL6_AstNode* arg = args->children[argIdx];
            /* arguments are wrapped in XL6_AST_ARG nodes, unwrap to get the actual expression */
            if ( arg != NULL && arg->kind == XL6_AST_ARG && arg->childCount > 0 ) {
                arg = arg->children[0];
            }
            if ( arg != NULL && arg->kind == XL6_AST_NAME &&
                 xl6CgenFindVarType(ctx, arg->text) == NULL ) {
                int bareCount = 0;
                if ( xl6CgenFindUniqueBareFunctionValueFunc(ctx, arg->text, &bareCount) != NULL ) {
                    return 1;
                }
            }
        }
    }

    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        const XL6_AstNode* typeNode = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* initExpr = node->childCount > 1 ? node->children[1] : NULL;

        if ( initExpr != NULL && xl6CgenNodeNeedsFunctionVarRuntime(ctx, initExpr) ) {
            return 1;
        }
        if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") ) {
            XL6_AstNode* inferredType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
            if ( inferredType != NULL ) {
                xl6CgenOwnTypeNode(ctx, inferredType);
                xl6CgenNeedsAddDeclVarType(ctx, node->text, inferredType, initExpr, 1);
            }
        } else if ( typeNode != NULL ) {
            xl6CgenNeedsAddDeclVarType(ctx, node->text, typeNode, initExpr, 0);
            if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
                const XL6_AstNode* itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
                if ( xl6CgenTypeContainsFunctionType(itemType) ) {
                    return 1;
                }
            }
            if ( xl6CTextEquals(typeNode->text, "var") && initExpr != NULL ) {
                XL6_AstNode* initType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
                int needsRuntime = xl6CgenIsFunctionType(initType);
                xl6AstFree(initType);
                if ( needsRuntime ) {
                    return 1;
                }
                /* bare function name assigned to var: var f = someFunc */
                if ( initExpr->kind == XL6_AST_NAME && xl6CgenFindVarType(ctx, initExpr->text) == NULL ) {
                    int bareCount = 0;
                    if ( xl6CgenFindUniqueBareFunctionValueFunc(ctx, initExpr->text, &bareCount) != NULL ) {
                        return 1;
                    }
                }
            }
        }
        return 0;
    }

    if ( node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC || node->kind == XL6_AST_EXPORT_FUNC || node->kind == XL6_AST_ANON_FUNC ) {
        const XL6_AstNode* params = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 0 : 1)
                                        ? node->children[node->kind == XL6_AST_ANON_FUNC ? 0 : 1]
                                        : NULL;
        const XL6_AstNode* body = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 2 : 3)
                                      ? node->children[node->kind == XL6_AST_ANON_FUNC ? 2 : 3]
                                      : NULL;
        XL6_CgenAnonFunc* anonInfo = node->kind == XL6_AST_ANON_FUNC ? xl6CgenFindAnonFuncInfo(ctx, node) : NULL;
        const XL6_CgenAnonFunc* prevAnon = ctx->currentAnonFunc;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( (node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC) && xl6CgenFuncIsGeneric(node) && ctx->currentGenericParams == NULL ) {
            return 0;
        }
        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentNativeClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentNativeClass);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        ctx->currentAnonFunc = anonInfo;
        xl6CgenRegisterAnonCaptureTypes(ctx, anonInfo);
        result = xl6CgenNodeNeedsFunctionVarRuntime(ctx, body);
        ctx->currentAnonFunc = prevAnon;
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_CREATE ) {
        const XL6_AstNode* params = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* body = node->childCount > 1 ? node->children[1] : NULL;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentNativeClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentNativeClass);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsFunctionVarRuntime(ctx, body);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_FOR ) {
        const XL6_AstNode* targets = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* iterable = node->childCount > 1 ? node->children[1] : NULL;
        const XL6_AstNode* body = node->childCount > 2 ? node->children[2] : NULL;
        XL6_AstNode* iterableType;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        iterableType = xl6CgenCloneExprTypeForInference(ctx, iterable);
        if ( targets != NULL ) {
            for ( i = 0; i < targets->childCount; ++i ) {
                const XL6_AstNode* storageType =
                    xl6CgenForTargetStorageType(ctx, node, i);
                xl6CgenRegisterLoopTargetTypeNode(
                    ctx,
                    targets->children[i],
                    storageType);
            }
        }
        result = xl6CgenNodeNeedsFunctionVarRuntime(ctx, body);
        xl6AstFree(iterableType);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_CLASS ) {
        const XL6_AstNode* prevClass = ctx->currentClass;
        const XL6_AstNode* prevNativeClass = ctx->currentNativeClass;
        int result = 0;

        ctx->currentClass = node;
        ctx->currentNativeClass = NULL;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsFunctionVarRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentNativeClass = prevNativeClass;
        return result;
    }


    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsFunctionVarRuntime(ctx, node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenProgramNeedsFunctionVarRuntime(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    size_t i;

    if ( xl6CgenNodeNeedsFunctionVarRuntime(ctx, root) ) {
        xl6CgenFreeCleanupsSince(ctx, 0);
        xl6CgenFreeVarsSince(ctx, 0);
        return 1;
    }
    xl6CgenFreeCleanupsSince(ctx, 0);
    xl6CgenFreeVarsSince(ctx, 0);
    for ( i = 0; i < ctx->genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
        int result;

        ctx->currentGenericParams = xl6CgenFuncTypeParams(ctx->genericInstances[i].func);
        ctx->currentGenericArgs = ctx->genericInstances[i].typeArgs;
        result = xl6CgenNodeNeedsFunctionVarRuntime(ctx, ctx->genericInstances[i].func);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        xl6CgenFreeCleanupsSince(ctx, 0);
        xl6CgenFreeVarsSince(ctx, 0);
        if ( result ) {
            return 1;
        }
    }
    return 0;
}

/* Scan entire AST for CALL nodes with bare function name arguments.
 * This catches the case where a function name is passed directly to
 * an extern var parameter, e.g. xge.xgeTutorialRun(on_frame). */
static int xl6CgenNodeHasBareFuncCallArg(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }

    if ( node->kind == XL6_AST_CALL && node->childCount > 1 && node->children[1] != NULL ) {
        const XL6_AstNode* args = node->children[1];
        size_t argIdx;
        for ( argIdx = 0; argIdx < args->childCount; ++argIdx ) {
            const XL6_AstNode* arg = args->children[argIdx];
            /* arguments are wrapped in XL6_AST_ARG nodes, unwrap to get the actual expression */
            if ( arg != NULL && arg->kind == XL6_AST_ARG && arg->childCount > 0 ) {
                arg = arg->children[0];
            }
            if ( arg != NULL && arg->kind == XL6_AST_NAME &&
                 xl6CgenFindVarType(ctx, arg->text) == NULL ) {
                int bareCount = 0;
                if ( xl6CgenFindUniqueBareFunctionValueFunc(ctx, arg->text, &bareCount) != NULL ) {
                    return 1;
                }
            }
        }
    }

    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeHasBareFuncCallArg(ctx, node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenProgramHasBareFuncCallArg(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    int result = xl6CgenNodeHasBareFuncCallArg(ctx, root);
    xl6CgenFreeCleanupsSince(ctx, 0);
    xl6CgenFreeVarsSince(ctx, 0);
    return result;
}
