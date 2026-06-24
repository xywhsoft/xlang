static void xl6CgenStmt(XL6_CgenContext* ctx, const XL6_AstNode* node);

static void xl6CgenBlock(XL6_CgenContext* ctx, const XL6_AstNode* block)
{
    size_t i;
    size_t cleanupMark;

    if ( block == NULL ) {
        return;
    }
    cleanupMark = ctx->cleanupCount;
    for ( i = 0; i < block->childCount; ++i ) {
        xl6CgenStmt(ctx, block->children[i]);
    }
    xl6CgenCloseScope(ctx, cleanupMark);
}

static void xl6CgenDecl(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const char* cType;
    const XL6_AstNode* typeNode;
    const XL6_AstNode* initExpr;
    XL6_AstNode* ctorCustom;
    XL6_AstNode* declCustom;
    XL6_AstNode* ctorClass;
    XL6_AstNode* declClass;
    XL6_AstNode* ctorClassType;
    XL6_AstNode* declClassType;
    char cName[512];
    int declaredAuto;

    xl6CgenIndent(ctx);
    typeNode = node->childCount > 0 ? node->children[0] : NULL;
    initExpr = node->childCount > 1 ? node->children[1] : NULL;
    xl6CgenCIdentText(cName, sizeof(cName), node->text);
    declaredAuto = typeNode != NULL && xl6CTextEquals(typeNode->text, "auto");
    ctorCustom = NULL;
    declCustom = NULL;
    ctorClass = NULL;
    declClass = NULL;
    ctorClassType = NULL;
    declClassType = NULL;

    if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") && initExpr != NULL ) {
        xl6CgenCallIsCustomCtor(ctx, initExpr, &ctorCustom);
        if ( !xl6CgenCallIsClassCtor(ctx, initExpr, &ctorClass) ) {
            ctorClassType = xl6CgenGenericClassCtorType(ctx, initExpr, &ctorClass);
        }
    } else if ( xl6CgenIsCustomType(ctx, typeNode) ) {
        declCustom = xl6CgenFindCustom(ctx, typeNode->text);
    } else if ( xl6CgenIsClassType(ctx, typeNode) ) {
        declClass = xl6CgenFindClass(ctx, typeNode->text);
        if ( initExpr != NULL && typeNode != NULL && typeNode->childCount > 0 ) {
            declClassType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
            ctorClassType = xl6CgenGenericClassCtorType(ctx, initExpr, &ctorClass);
            if ( declClassType == NULL ) {
                xl6CgenError(ctx, typeNode, "code generation error");
            } else if ( ctorClassType != NULL && !xl6CgenTypeNodeEqual(declClassType, ctorClassType) ) {
                xl6CgenError(ctx, initExpr, "code generation error");
            }
        } else if ( typeNode != NULL && typeNode->childCount > 0 ) {
            declClassType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
            if ( declClassType == NULL ) {
                xl6CgenError(ctx, typeNode, "code generation error");
            }
        }
    }

    if ( ctorCustom != NULL || declCustom != NULL ) {
        const XL6_AstNode* customNode = ctorCustom != NULL ? ctorCustom : declCustom;
        xl6CgenCustomTypeName(ctx->out, customNode->text);
        xl6OutputPrintf(ctx->out, " %s = {0};\n", cName);
        xl6CgenAddVarType(ctx, node->text, customNode);
        xl6CgenAddCustomCleanup(ctx, node->text, customNode);
        if ( initExpr != NULL ) {
            if ( xl6CgenCallIsCustomCtor(ctx, initExpr, &ctorCustom) && xl6CTextEquals(ctorCustom->text, customNode->text) ) {
                xl6CgenCustomCtorAssign(ctx, customNode, node->text, initExpr);
            } else {
                XL6_AstNode* initType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
                const XL6_AstNode* resolvedInitType = xl6CgenResolveAliasType(ctx, initType);

                /*
                    �?destroy �?custom 本质只是句柄值，可以按结构体值复制�?                    �?destroy �?custom 已经由语义层判为不可复制，这里仍保持构造表达式专用路径�?                */
                if ( resolvedInitType != NULL &&
                     xl6CTextEquals(resolvedInitType->text, customNode->text) &&
                     !xl6CgenCustomHasDestroy(customNode) ) {
                    xl6CgenIndent(ctx);
                    xl6OutputPrintf(ctx->out, "%s = ", cName);
                    xl6CgenExpr(ctx, initExpr);
                    xl6OutputPuts(";\n", ctx->out);
                } else {
                    xl6CgenError(ctx, initExpr, "custom 变量只能使用同类型构造表达式或可复制同类型值初始化");
                }
                xl6AstFree(initType);
            }
        }
        return;
    }

    if ( ctorClass != NULL || declClass != NULL ) {
        const XL6_AstNode* classNode = declClass != NULL ? declClass : ctorClass;
        const XL6_AstNode* classTypeNode = (declClass != NULL && declClassType != NULL) ? declClassType : (ctorClassType != NULL ? ctorClassType : typeNode);
        int isGenericClass = (ctorClassType != NULL) || (declClassType != NULL);
        const XL6_AstNode* args = initExpr != NULL && initExpr->childCount > 1 ? initExpr->children[1] : NULL;
        const XL6_AstNode* createNode = NULL;
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
        int copyInit = 0;
        if ( ctorClassType != NULL ) {
            xl6CgenOwnTypeNode(ctx, ctorClassType);
        }
        if ( declClassType != NULL ) {
            xl6CgenOwnTypeNode(ctx, declClassType);
        }
        if ( isGenericClass ) {
            ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
            ctx->currentGenericArgs = classTypeNode->children[0];
            xl6OutputPuts(xl6CgenTypeName(ctx, classTypeNode), ctx->out);
        } else {
            xl6CgenClassTypeName(ctx->out, classNode->text);
        }
        createNode = (initExpr != NULL && ctorClass != NULL) ?
                     xl6CgenResolveClassCreate(ctx, classNode, initExpr) :
                     xl6CgenFindClassSpecial(classNode, "create");
        xl6OutputPrintf(ctx->out, " %s = {0};\n", cName);
        xl6CgenAddVarType(ctx, node->text, isGenericClass ? classTypeNode : classNode);
        xl6CgenAddClassCleanup(ctx, node->text, classNode, isGenericClass ? classTypeNode : NULL);
        if ( initExpr != NULL && ctorClassType == NULL ) {
            if ( !xl6CgenCallIsClassCtor(ctx, initExpr, &ctorClass) || !xl6CTextEquals(ctorClass->text, classNode->text) ) {
                XL6_AstNode* initType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
                const XL6_AstNode* resolvedInitType = xl6CgenResolveAliasType(ctx, initType);
                int sameType = 0;

                if ( isGenericClass ) {
                    sameType = resolvedInitType != NULL && xl6CgenTypeNodeEqual(classTypeNode, resolvedInitType);
                } else {
                    sameType = resolvedInitType != NULL && xl6CTextEquals(resolvedInitType->text, classNode->text);
                }

                if ( sameType ) {
                    copyInit = 1;
                } else {
                    xl6CgenError(ctx, initExpr, "class 变量只能使用同类型构造表达式或可复制同类型值初始化");
                }
                xl6AstFree(initType);
            }
        }
        xl6CgenClassDefaultInitFields(ctx, classNode, cName);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        if ( copyInit ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = ", cName);
            xl6CgenExpr(ctx, initExpr);
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, isGenericClass ? classTypeNode : classNode, cName, initExpr);
        } else if ( initExpr != NULL && ctorClass != NULL && xl6CTextEquals(ctorClass->text, classNode->text) ) {
            if ( createNode != NULL ) {
                xl6CgenIndent(ctx);
                if ( isGenericClass ) {
                    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
                    ctx->currentGenericArgs = classTypeNode->children[0];
                    xl6CgenGenericClassCallableSymbol(ctx, classNode, classTypeNode, createNode);
                } else {
                    xl6CgenClassCallableSymbol(ctx, classNode, createNode);
                }
                xl6OutputPrintf(ctx->out, "(&%s", cName);
                if ( args != NULL && xl6CgenFixedParamCount(createNode->childCount > 0 ? createNode->children[0] : NULL) > 0 ) {
                    xl6OutputPuts(", ", ctx->out);
                    xl6CgenParamNodeArgs(ctx, createNode->childCount > 0 ? createNode->children[0] : NULL, args);
                }
                xl6OutputPuts(");\n", ctx->out);
                ctx->currentGenericParams = prevParams;
                ctx->currentGenericArgs = prevArgs;
            } else if ( args != NULL && args->childCount > 0 ) {
                xl6CgenError(ctx, initExpr, "code generation error");
            }
        }
        if ( !copyInit && createNode == NULL ) {
            xl6CgenEmitAutoBaseCreateForValue(ctx, classNode, cName, node);
        }
        return;
    }

    if ( declaredAuto ) {
        XL6_AstNode* inferredTypeNode;
        cType = initExpr != NULL ? xl6CgenInferExprType(ctx, initExpr) : "int";
        inferredTypeNode = xl6CgenCloneExprTypeForInference(ctx, initExpr);
        if ( inferredTypeNode != NULL ) {
            xl6CgenOwnTypeNode(ctx, inferredTypeNode);
            typeNode = inferredTypeNode;
        }
    } else {
        cType = typeNode != NULL ? xl6CgenTypeName(ctx, typeNode) : "int";
    }
    if ( declaredAuto && initExpr != NULL && xl6CgenIsClassType(ctx, typeNode) ) {
        XL6_AstNode* autoClass;
        XL6_AstNode* autoClassType;
        const XL6_AstNode* autoClassTypeNode;
        const XL6_AstNode* prevParams;
        const XL6_AstNode* prevArgs;
        int isGenericAutoClass;

        autoClass = xl6CgenFindClass(ctx, typeNode != NULL ? typeNode->text : NULL);
        if ( autoClass != NULL ) {
            autoClassType = NULL;
            autoClassTypeNode = autoClass;
            isGenericAutoClass = typeNode != NULL && typeNode->childCount > 0 && xl6CgenClassHasTypeParams(autoClass);
            if ( isGenericAutoClass ) {
                autoClassType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
                if ( autoClassType == NULL ) {
                    xl6CgenError(ctx, typeNode, "code generation error");
                    return;
                }
                xl6CgenOwnTypeNode(ctx, autoClassType);
                autoClassTypeNode = autoClassType;
            }

            prevParams = ctx->currentGenericParams;
            prevArgs = ctx->currentGenericArgs;
            if ( isGenericAutoClass ) {
                ctx->currentGenericParams = xl6CgenClassTypeParams(autoClass);
                ctx->currentGenericArgs = autoClassTypeNode->children[0];
            }
            xl6OutputPuts(xl6CgenTypeName(ctx, autoClassTypeNode), ctx->out);
            xl6OutputPrintf(ctx->out, " %s = {0};\n", cName);
            xl6CgenAddVarType(ctx, node->text, autoClassTypeNode);
            xl6CgenAddClassCleanup(ctx, node->text, autoClass, isGenericAutoClass ? autoClassTypeNode : NULL);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = ", cName);
            xl6CgenExpr(ctx, initExpr);
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, autoClassTypeNode, cName, initExpr);
            ctx->currentGenericParams = prevParams;
            ctx->currentGenericArgs = prevArgs;
            return;
        }
    }
    if ( xl6CgenDeclShouldUseTypedContainer(ctx, typeNode, initExpr, declaredAuto) ) {
        int initByCopy = xl6CgenTypedContainerCanCopyFromExpr(ctx, typeNode, initExpr);
        int initByMove = xl6CgenTypedContainerCanMoveFromExpr(ctx, typeNode, initExpr);
        xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, typeNode), cName);
        if ( initByCopy ) {
            xl6CgenTypedContainerCopyExpr(ctx, typeNode, initExpr);
        } else if ( initByMove ) {
            xl6CgenExpr(ctx, initExpr);
        } else {
            xl6CgenTypedContainerCreateExpr(ctx, typeNode);
        }
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenAddVarTypeWithFlags(ctx, node->text, typeNode, XL6_CGEN_VAR_TYPED_CONTAINER);
        xl6CgenAddTypedContainerCleanup(ctx, node->text, typeNode);
        if ( !initByCopy && !initByMove ) {
            xl6CgenTypedContainerFillFromLiteral(ctx, cName, typeNode, initExpr);
        }
        return;
    }
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6CgenAddVarType(ctx, node->text, typeNode);
        xl6CgenAddXValueCleanup(ctx, node->text, typeNode);
        xl6OutputPrintf(ctx->out, "xvalue %s = ", cName);
        if ( initExpr != NULL ) {
            xl6CgenXValueFromExprForTarget(ctx, typeNode, initExpr);
        } else {
            xl6CgenDefaultXValueForTarget(ctx, typeNode);
        }
        xl6OutputPuts(";\n", ctx->out);
        return;
    }
    xl6CgenAddVarType(ctx, node->text, typeNode);
    xl6CgenAddFunctionCleanup(ctx, node->text, typeNode);
    xl6CgenAddFutureCleanup(ctx, node->text, typeNode);
    xl6CgenAddChannelCleanup(ctx, node->text, typeNode);
    xl6CgenAddThreadCleanup(ctx, node->text, typeNode);
    xl6CgenAddGeneratorCleanup(ctx, node->text, typeNode);
    xl6CgenAddSyncCleanup(ctx, node->text, typeNode);
    xl6OutputPrintf(ctx->out, "%s %s", cType, cName);
    if ( initExpr != NULL ) {
        xl6OutputPuts(" = ", ctx->out);
        if ( xl6CgenIsFunctionType(typeNode) ) {
            xl6CgenFunctionValueExpr(ctx, initExpr, typeNode);
        } else {
            xl6CgenExpr(ctx, initExpr);
        }
    } else {
        xl6OutputPuts(" = ", ctx->out);
        xl6CgenDefaultValue(ctx, typeNode);
    }
    xl6OutputPuts(";\n", ctx->out);
    if ( xl6CgenIsFunctionType(typeNode) && xl6CgenExprIsFunctionBorrow(ctx, initExpr) ) {
        xl6CgenRetainFunctionValue(ctx, node->text);
    }
}

static const XL6_AstNode* xl6CgenResolveMultiReturnCall(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    XL6_AstNode* func;
    const XL6_AstNode* returns;

    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return NULL;
    }
    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return NULL;
    }
    func = xl6CgenResolveCallFunc(ctx, expr, callee->text);
    returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
    return (returns != NULL && returns->childCount > 1) ? func : NULL;
}

static const XL6_AstNode* xl6CgenResolveMultiReturnFunctionType(XL6_CgenContext* ctx, const XL6_AstNode* expr, XL6_AstNode** ownedType)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* functionType;
    const XL6_AstNode* returns;

    if ( ownedType != NULL ) {
        *ownedType = NULL;
    }
    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return NULL;
    }
    callee = expr->children[0];
    if ( callee == NULL ) {
        return NULL;
    }

    if ( callee->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* memberType = xl6CgenCloneMemberFieldTypeForInference(ctx, callee);
        returns = xl6CgenFunctionReturnTypes(memberType);
        if ( xl6CgenIsFunctionType(memberType) && returns != NULL && returns->childCount > 1 ) {
            if ( ownedType != NULL ) {
                *ownedType = memberType;
                return memberType;
            }
        }
        xl6AstFree(memberType);
        return NULL;
    }
    if ( callee->kind != XL6_AST_NAME ) {
        return NULL;
    }

    functionType = xl6CgenFindVarType(ctx, callee->text);
    returns = xl6CgenFunctionReturnTypes(functionType);
    return (xl6CgenIsFunctionType(functionType) && returns != NULL && returns->childCount > 1) ? functionType : NULL;
}
static void xl6CgenEmitKnownFuncCall(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* call)
{
    xl6CgenFuncSymbol(ctx, func);
    xl6OutputPutc('(', ctx->out);
    if ( call != NULL && call->childCount > 1 ) {
        xl6CgenKnownCallArgs(ctx, func, call->children[1]);
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenEmitMultiReturnCall(XL6_CgenContext* ctx, const XL6_AstNode* expr, const XL6_AstNode* func, const XL6_AstNode* functionType)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;

    if ( functionType != NULL ) {
        callee = expr != NULL && expr->childCount > 0 ? expr->children[0] : NULL;
        args = expr != NULL && expr->childCount > 1 ? expr->children[1] : NULL;
        if ( callee != NULL && callee->kind == XL6_AST_NAME ) {
            xl6CgenFunctionInvoke(ctx, callee->text, functionType, args);
            return;
        }
        if ( callee != NULL && callee->kind == XL6_AST_MEMBER ) {
            int id = ctx->tempId++;
            char tempName[64];

            snprintf(tempName, sizeof(tempName), "__xl6_member_fn_multi_%d", id);
            xl6OutputPrintf(ctx->out, "({ XL6_Function %s = ", tempName);
            xl6CgenExpr(ctx, callee);
            xl6OutputPuts("; ", ctx->out);
            xl6CgenFunctionInvoke(ctx, tempName, functionType, args);
            xl6OutputPuts("; })", ctx->out);
            return;
        }
    }

    xl6CgenEmitKnownFuncCall(ctx, func, expr);
}
static void xl6CgenMultiReturnStructName(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* functionType)
{
    if ( functionType != NULL ) {
        xl6CgenFunctionReturnStructName(ctx,
                                        xl6CgenFunctionParamTypes(functionType),
                                        xl6CgenFunctionReturnTypes(functionType));
        return;
    }
    xl6CgenFuncReturnStructName(ctx, func);
}

static void xl6CgenAddManagedCleanupForType(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    XL6_AstNode* classNode;
    XL6_AstNode* customNode;

    if ( name == NULL || typeNode == NULL ) {
        return;
    }
    if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6CgenAddFunctionCleanup(ctx, name, typeNode);
        return;
    }
    if ( xl6CgenIsFutureLikeType(typeNode) ) {
        xl6CgenAddFutureCleanup(ctx, name, typeNode);
        return;
    }
    if ( xl6CgenIsChannelType(typeNode) ) {
        xl6CgenAddChannelCleanup(ctx, name, typeNode);
        return;
    }
    if ( xl6CgenIsThreadType(typeNode) ) {
        xl6CgenAddThreadCleanup(ctx, name, typeNode);
        return;
    }
    if ( xl6CgenIsGeneratorType(typeNode) ) {
        xl6CgenAddGeneratorCleanup(ctx, name, typeNode);
        return;
    }
    if ( xl6CgenIsSyncHandleType(typeNode) ) {
        xl6CgenAddSyncCleanup(ctx, name, typeNode);
        return;
    }
    if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        xl6CgenAddTypedContainerCleanup(ctx, name, typeNode);
        return;
    }
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6CgenAddXValueCleanup(ctx, name, typeNode);
        return;
    }
    customNode = typeNode->text != NULL ? xl6CgenFindCustom(ctx, typeNode->text) : NULL;
    if ( customNode != NULL ) {
        xl6CgenAddCustomCleanup(ctx, name, customNode);
        return;
    }
    classNode = typeNode->text != NULL ? xl6CgenFindClass(ctx, typeNode->text) : NULL;
    if ( classNode != NULL ) {
        xl6CgenAddClassCleanup(ctx, name, classNode, typeNode->childCount > 0 ? typeNode : NULL);
    }
}

static void xl6CgenMultiDecl(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* names;
    const XL6_AstNode* expr;
    const XL6_AstNode* func;
    const XL6_AstNode* functionType;
    XL6_AstNode* ownedFunctionType;
    const XL6_AstNode* returns;
    int id;
    size_t i;

    names = node->childCount > 0 ? node->children[0] : NULL;
    expr = node->childCount > 1 ? node->children[1] : NULL;
    ownedFunctionType = NULL;
    functionType = xl6CgenResolveMultiReturnFunctionType(ctx, expr, &ownedFunctionType);
    func = functionType == NULL ? xl6CgenResolveMultiReturnCall(ctx, expr) : NULL;
    returns = functionType != NULL ? xl6CgenFunctionReturnTypes(functionType) : (func != NULL ? func->children[2] : NULL);
    if ( returns == NULL || names == NULL ) {
        xl6AstFree(ownedFunctionType);
        xl6CgenError(ctx, node, "多返回值声明右侧必须是多返回�?xlang 函数调用");
        return;
    }

    if ( names->childCount != returns->childCount ) {
        xl6AstFree(ownedFunctionType);
        xl6CgenError(ctx, node, "多返回值声明数量和函数返回值数量不匹配");
        return;
    }

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6CgenMultiReturnStructName(ctx, func, functionType);
    xl6OutputPrintf(ctx->out, " __xl6_multi_%d = ", id);
    xl6CgenEmitMultiReturnCall(ctx, expr, func, functionType);
    xl6OutputPuts(";\n", ctx->out);

    for ( i = 0; i < names->childCount; ++i ) {
        const XL6_AstNode* name = names->children[i];
        const XL6_AstNode* typeNode = returns->children[i];
        const XL6_AstNode* varTypeNode;
        XL6_AstNode* ownedTypeNode;
        char cName[512];

        if ( name == NULL || xl6CTextEquals(name->text, "_") ) {
            char valuePath[96];

            if ( xl6CgenIsXValueType(typeNode) ) {
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "if (__xl6_multi_%d.value%u != 0) {\n", id, (unsigned)i);
                ctx->indent += 1;
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_multi_%d.value%u);\n", id, (unsigned)i);
                ctx->indent -= 1;
                xl6CgenIndent(ctx);
                xl6OutputPuts("}\n", ctx->out);
            }
            snprintf(valuePath, sizeof(valuePath), "__xl6_multi_%d.value%u", id, (unsigned)i);
            xl6CgenReleaseReturnedValuePath(ctx, typeNode, valuePath);
            continue;
        }
        xl6CgenCIdentText(cName, sizeof(cName), name->text);
        xl6CgenIndent(ctx);
        xl6OutputPuts(xl6CgenTypeName(ctx, typeNode), ctx->out);
        xl6OutputPrintf(ctx->out, " %s = __xl6_multi_%d.value%u;\n", cName, id, (unsigned)i);
        varTypeNode = typeNode;
        ownedTypeNode = xl6CgenCloneTypeNode(typeNode);
        if ( ownedTypeNode != NULL ) {
            xl6CgenOwnTypeNode(ctx, ownedTypeNode);
            varTypeNode = ownedTypeNode;
        }
        xl6CgenAddVarType(ctx, name->text, varTypeNode);
        xl6CgenAddManagedCleanupForType(ctx, name->text, varTypeNode);
    }
    xl6AstFree(ownedFunctionType);
}

static void xl6CgenXValueFromTypedCExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* exprText)
{
    const XL6_AstNode* concreteType;
    const char* typeName;
    char cTypeName[512];

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CgenIsXValueType(concreteType) ) {
        xl6OutputPuts(exprText != NULL ? exprText : "xvoCreateNull()", ctx->out);
    } else if ( xl6CgenIsIntegerTypeName(typeName) ) {
        xl6OutputPrintf(ctx->out, "xvoCreateInt((long long)(%s))", exprText != NULL ? exprText : "0");
    } else if ( xl6CgenIsFloatTypeName(typeName) ) {
        xl6OutputPrintf(ctx->out, "xvoCreateFloat((double)(%s))", exprText != NULL ? exprText : "0");
    } else if ( xl6CTextEquals(typeName, "bool") ) {
        xl6OutputPrintf(ctx->out, "xvoCreateBool(%s)", exprText != NULL ? exprText : "0");
    } else if ( xl6CTextEquals(typeName, "string") ) {
        xl6OutputPrintf(ctx->out, "xvoCreateText((void*)(%s), 0, false)", exprText != NULL ? exprText : "\"\"");
    } else if ( xl6CTextEquals(typeName, "time") ) {
        xl6OutputPrintf(ctx->out, "xvoCreateTime((long long)(%s))", exprText != NULL ? exprText : "0");
    } else if ( xl6CTextEquals(typeName, "point") ) {
        xl6OutputPrintf(ctx->out, "xvoCreatePoint((void*)(%s))", exprText != NULL ? exprText : "0");
    } else if ( xl6CgenIsFunctionType(concreteType) ) {
        xl6OutputPrintf(ctx->out, "xl6_function_to_xvalue(%s)", exprText != NULL ? exprText : "(XL6_Function){0}");
    } else if ( xl6CgenIsClassType(ctx, concreteType) ) {
        xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), concreteType);
        xl6CgenBoxSymbol(ctx->out, cTypeName);
        xl6OutputPrintf(ctx->out, "(%s)", exprText != NULL ? exprText : "{}");
    } else if ( xl6CgenIsUnionType(ctx, concreteType) ) {
        xl6CgenUnionTypeTextName(cTypeName, sizeof(cTypeName), typeName);
        xl6CgenBoxSymbol(ctx->out, cTypeName);
        xl6OutputPrintf(ctx->out, "(%s)", exprText != NULL ? exprText : "{}");
    } else if ( xl6CgenIsCustomType(ctx, concreteType) ) {
        xl6CgenCustomTypeRefName(cTypeName, sizeof(cTypeName), typeName);
        xl6CgenBoxSymbol(ctx->out, cTypeName);
        xl6OutputPrintf(ctx->out, "(%s)", exprText != NULL ? exprText : "{}");
    } else {
        xl6CgenError(ctx, typeNode, "code generation error");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
    }
}

static void xl6CgenMultiAssign(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* targets;
    const XL6_AstNode* expr;
    const XL6_AstNode* func;
    const XL6_AstNode* functionType;
    XL6_AstNode* ownedFunctionType;
    const XL6_AstNode* returns;
    int id;
    size_t i;

    targets = node->childCount > 0 ? node->children[0] : NULL;
    expr = node->childCount > 1 ? node->children[1] : NULL;
    ownedFunctionType = NULL;
    functionType = xl6CgenResolveMultiReturnFunctionType(ctx, expr, &ownedFunctionType);
    func = functionType == NULL ? xl6CgenResolveMultiReturnCall(ctx, expr) : NULL;
    returns = functionType != NULL ? xl6CgenFunctionReturnTypes(functionType) : (func != NULL ? func->children[2] : NULL);
    if ( returns == NULL || targets == NULL ) {
        xl6AstFree(ownedFunctionType);
        xl6CgenError(ctx, node, "多返回值赋值右侧必须是多返回�?xlang 函数调用");
        return;
    }

    if ( targets->childCount != returns->childCount ) {
        xl6AstFree(ownedFunctionType);
        xl6CgenError(ctx, node, "多返回值赋值数量和函数返回值数量不匹配");
        return;
    }

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6CgenMultiReturnStructName(ctx, func, functionType);
    xl6OutputPrintf(ctx->out, " __xl6_multi_%d = ", id);
    xl6CgenEmitMultiReturnCall(ctx, expr, func, functionType);
    xl6OutputPuts(";\n", ctx->out);

    for ( i = 0; i < targets->childCount; ++i ) {
        const XL6_AstNode* target = targets->children[i];
        const XL6_AstNode* typeNode = returns->children[i];
        char cName[512];

        if ( target == NULL || (target->kind == XL6_AST_NAME && xl6CTextEquals(target->text, "_")) ) {
            char valuePath[96];

            if ( xl6CgenIsXValueType(typeNode) ) {
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "if (__xl6_multi_%d.value%u != 0) {\n", id, (unsigned)i);
                ctx->indent += 1;
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_multi_%d.value%u);\n", id, (unsigned)i);
                ctx->indent -= 1;
                xl6CgenIndent(ctx);
                xl6OutputPuts("}\n", ctx->out);
            }
            snprintf(valuePath, sizeof(valuePath), "__xl6_multi_%d.value%u", id, (unsigned)i);
            xl6CgenReleaseReturnedValuePath(ctx, typeNode, valuePath);
            continue;
        }
        if ( target->kind == XL6_AST_NAME && xl6CgenIsXValueType(xl6CgenFindVarType(ctx, target->text)) ) {
            xl6CgenCIdentText(cName, sizeof(cName), target->text);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", cName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = __xl6_multi_%d.value%u;\n", cName, id, (unsigned)i);
            continue;
        }
        if ( target->kind == XL6_AST_NAME && xl6CgenIsFunctionType(xl6CgenFindVarType(ctx, target->text)) ) {
            xl6CgenCIdentText(cName, sizeof(cName), target->text);
            xl6CgenReleaseFunctionValue(ctx, target->text);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = __xl6_multi_%d.value%u;\n", cName, id, (unsigned)i);
            continue;
        }
        if ( target->kind == XL6_AST_NAME && xl6CgenClassTypeNeedsFunctionFieldCleanup(ctx, xl6CgenFindVarType(ctx, target->text)) ) {
            xl6CgenCIdentText(cName, sizeof(cName), target->text);
            xl6CgenReleaseClassFunctionFieldsForType(ctx, xl6CgenFindVarType(ctx, target->text), cName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = __xl6_multi_%d.value%u;\n", cName, id, (unsigned)i);
            continue;
        }
        if ( target->kind == XL6_AST_INDEX ) {
            int valueId = ctx->tempId++;
            char tempName[64];
            char valueExpr[96];

            snprintf(tempName, sizeof(tempName), "__xl6_multi_index_%d", valueId);
            snprintf(valueExpr, sizeof(valueExpr), "__xl6_multi_%d.value%u", id, (unsigned)i);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvalue %s = ", tempName);
            xl6CgenXValueFromTypedCExpr(ctx, typeNode, valueExpr);
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenIndexAssignTemp(ctx, target, tempName);
            continue;
        }
        xl6CgenIndent(ctx);
        xl6CgenExpr(ctx, target);
        xl6OutputPrintf(ctx->out, " = __xl6_multi_%d.value%u;\n", id, (unsigned)i);
    }
    xl6AstFree(ownedFunctionType);
}

