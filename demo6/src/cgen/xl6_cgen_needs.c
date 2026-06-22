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

static int xl6CgenNodeNeedsStringRuntime(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
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

    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        const XL6_AstNode* typeNode = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* initExpr = node->childCount > 1 ? node->children[1] : NULL;

        if ( initExpr != NULL && xl6CgenNodeNeedsStringRuntime(ctx, initExpr) ) {
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
        const XL6_AstNode* body = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 1 : 3)
                                      ? node->children[node->kind == XL6_AST_ANON_FUNC ? 1 : 3]
                                      : NULL;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( (node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC) && xl6CgenFuncIsGeneric(node) && ctx->currentGenericParams == NULL ) {
            return 0;
        }
        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentCustom != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentCustom);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsStringRuntime(ctx, body);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_TRY ) {
        const XL6_AstNode* tryBlock = node->childCount > 0 ? node->children[0] : NULL;
        const XL6_AstNode* catchNode = node->childCount > 1 ? node->children[1] : NULL;
        const XL6_AstNode* finallyBlock = node->childCount > 2 ? node->children[2] : NULL;

        if ( xl6CgenNodeNeedsStringRuntime(ctx, tryBlock) ) {
            return 1;
        }
        if ( catchNode != NULL && catchNode->kind == XL6_AST_CATCH ) {
            const XL6_AstNode* catchName = catchNode->childCount > 0 ? catchNode->children[0] : NULL;
            const XL6_AstNode* catchBlock = catchNode->childCount > 1 ? catchNode->children[1] : NULL;
            const XL6_AstNode* errorType = xl6CgenOwnSimpleType(ctx, "error", catchName != NULL ? catchName : catchNode);
            size_t varMark = ctx->varCount;
            size_t cleanupMark = ctx->cleanupCount;
            int result;

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
        } else if ( ctx->currentCustom != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentCustom);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsStringRuntime(ctx, body);
        xl6CgenFreeCleanupsSince(ctx, cleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        return result;
    }

    if ( node->kind == XL6_AST_CLASS ) {
        const XL6_AstNode* prevClass = ctx->currentClass;
        const XL6_AstNode* prevCustom = ctx->currentCustom;
        int result = 0;

        ctx->currentClass = node;
        ctx->currentCustom = NULL;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsStringRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentCustom = prevCustom;
        return result;
    }

    if ( node->kind == XL6_AST_CUSTOM ) {
        const XL6_AstNode* prevClass = ctx->currentClass;
        const XL6_AstNode* prevCustom = ctx->currentCustom;
        int result = 0;

        ctx->currentClass = NULL;
        ctx->currentCustom = node;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsStringRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentCustom = prevCustom;
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

    if ( (xl6CgenIsIntegerTypeName(target) || xl6CgenIsFloatTypeName(target) || xl6CTextEquals(target, "time")) &&
         xl6CTextEquals(source, "string") ) {
        result = 1;
    } else if ( xl6CTextEquals(target, "string") &&
                (xl6CgenIsIntegerTypeName(source) || xl6CgenIsFloatTypeName(source) ||
                 xl6CTextEquals(source, "time") || xl6CTextEquals(source, "point") ||
                 xl6CTextEquals(source, "var")) ) {
        result = 1;
    }

    xl6AstFree(sourceType);
    return result;
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
        const XL6_AstNode* body = node->childCount > (node->kind == XL6_AST_ANON_FUNC ? 1 : 3)
                                      ? node->children[node->kind == XL6_AST_ANON_FUNC ? 1 : 3]
                                      : NULL;
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( (node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC) && xl6CgenFuncIsGeneric(node) && ctx->currentGenericParams == NULL ) {
            return 0;
        }
        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentCustom != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentCustom);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsTypeConvertRuntime(ctx, body);
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
        } else if ( ctx->currentCustom != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentCustom);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsTypeConvertRuntime(ctx, body);
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
                const char* typeName = xl6CgenForTargetTypeName(iterableType, i, targets->childCount);
                xl6CgenRegisterLoopTargetType(ctx, targets->children[i], typeName);
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
        const XL6_AstNode* prevCustom = ctx->currentCustom;
        int result = 0;

        ctx->currentClass = node;
        ctx->currentCustom = NULL;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsTypeConvertRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentCustom = prevCustom;
        return result;
    }

    if ( node->kind == XL6_AST_CUSTOM ) {
        const XL6_AstNode* prevClass = ctx->currentClass;
        const XL6_AstNode* prevCustom = ctx->currentCustom;
        int result = 0;

        ctx->currentClass = NULL;
        ctx->currentCustom = node;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsTypeConvertRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentCustom = prevCustom;
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

static int xl6CgenNodeNeedsFunctionVarRuntime(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }

    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME ) {
        const XL6_AstNode* functionType = xl6CgenFindVarType(ctx, node->children[0]->text);
        if ( xl6CgenIsFunctionType(functionType) ||
             (functionType != NULL && xl6CTextEquals(functionType->text, "var")) ) {
            return 1;
        }
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* memberType = xl6CgenCloneMemberFieldTypeForInference(ctx, node->children[0]);
        int needsRuntime = xl6CgenIsFunctionType(memberType) ||
                           (memberType != NULL && xl6CTextEquals(memberType->text, "var"));
        xl6AstFree(memberType);
        if ( needsRuntime ) {
            return 1;
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
        size_t varMark = ctx->varCount;
        size_t cleanupMark = ctx->cleanupCount;
        int result;

        if ( (node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC) && xl6CgenFuncIsGeneric(node) && ctx->currentGenericParams == NULL ) {
            return 0;
        }
        if ( ctx->currentClass != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentClass);
        } else if ( ctx->currentCustom != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentCustom);
        }
        xl6CgenRegisterParamTypes(ctx, params);
        result = xl6CgenNodeNeedsFunctionVarRuntime(ctx, body);
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
        } else if ( ctx->currentCustom != NULL ) {
            xl6CgenAddVarType(ctx, "self", ctx->currentCustom);
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
                const char* typeName = xl6CgenForTargetTypeName(iterableType, i, targets->childCount);
                xl6CgenRegisterLoopTargetType(ctx, targets->children[i], typeName);
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
        const XL6_AstNode* prevCustom = ctx->currentCustom;
        int result = 0;

        ctx->currentClass = node;
        ctx->currentCustom = NULL;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsFunctionVarRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentCustom = prevCustom;
        return result;
    }

    if ( node->kind == XL6_AST_CUSTOM ) {
        const XL6_AstNode* prevClass = ctx->currentClass;
        const XL6_AstNode* prevCustom = ctx->currentCustom;
        int result = 0;

        ctx->currentClass = NULL;
        ctx->currentCustom = node;
        for ( i = 0; i < node->childCount; ++i ) {
            if ( xl6CgenNodeNeedsFunctionVarRuntime(ctx, node->children[i]) ) {
                result = 1;
                break;
            }
        }
        ctx->currentClass = prevClass;
        ctx->currentCustom = prevCustom;
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
