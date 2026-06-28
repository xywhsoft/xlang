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
                xl6CgenError(ctx, typeNode, "generic class type could not be resolved");
            } else if ( ctorClassType != NULL && !xl6CgenTypeNodeEqual(declClassType, ctorClassType) ) {
                xl6CgenError(ctx, initExpr, "generic class constructor type does not match declared type");
            }
        } else if ( typeNode != NULL && typeNode->childCount > 0 ) {
            declClassType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
            if ( declClassType == NULL ) {
                xl6CgenError(ctx, typeNode, "generic class type could not be resolved");
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
                    没有 destroy 的 custom 本质只是句柄值，可以按结构体值复制。
                    有 destroy 的 custom 已经由语义层判为不可复制，这里仍保持构造表达式专用路径。
                */
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
            XL6_AstNode* memberFunctionType;
            char memberTempName[64];

            memberFunctionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, initExpr);
            memberTempName[0] = '\0';
            if ( memberFunctionType != NULL ) {
                snprintf(memberTempName, sizeof(memberTempName), "__xl6_member_fn_stmt_%d", ctx->tempId++);
                xl6CgenEmitMemberFunctionStatementTemp(ctx, initExpr, memberTempName);
            }
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = ", cName);
            if ( memberFunctionType != NULL ) {
                xl6CgenMemberFunctionStatementInvokeExpr(ctx, initExpr, memberFunctionType, memberTempName);
            } else {
                xl6CgenExpr(ctx, initExpr);
            }
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, isGenericClass ? classTypeNode : classNode, cName, initExpr);
            xl6AstFree(memberFunctionType);
        } else if ( initExpr != NULL && ctorClass != NULL && xl6CTextEquals(ctorClass->text, classNode->text) ) {
            if ( createNode != NULL ) {
                const XL6_AstNode* createParams;

                xl6CgenIndent(ctx);
                if ( isGenericClass ) {
                    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
                    ctx->currentGenericArgs = classTypeNode->children[0];
                    xl6CgenGenericClassCallableSymbol(ctx, classNode, classTypeNode, createNode);
                } else {
                    xl6CgenClassCallableSymbol(ctx, classNode, createNode);
                }
                xl6OutputPrintf(ctx->out, "(&%s", cName);
                createParams = xl6CgenCallableParams(createNode);
                if ( args != NULL && xl6CgenFixedParamCount(createParams) > 0 ) {
                    xl6OutputPuts(", ", ctx->out);
                    xl6CgenParamNodeArgs(ctx, createParams, args);
                }
                xl6OutputPuts(");\n", ctx->out);
                ctx->currentGenericParams = prevParams;
                ctx->currentGenericArgs = prevArgs;
            } else if ( args != NULL && args->childCount > 0 &&
                        !xl6CgenEmitNativeClassHandleBinding(ctx, classNode, cName, args) ) {
                xl6CgenError(ctx, initExpr, "class constructor has arguments but no matching create method");
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
                    xl6CgenError(ctx, typeNode, "auto generic class type could not be resolved");
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
            {
                XL6_AstNode* memberFunctionType;
                char memberTempName[64];

                memberFunctionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, initExpr);
                memberTempName[0] = '\0';
                if ( memberFunctionType != NULL ) {
                    snprintf(memberTempName, sizeof(memberTempName), "__xl6_member_fn_stmt_%d", ctx->tempId++);
                    xl6CgenEmitMemberFunctionStatementTemp(ctx, initExpr, memberTempName);
                }
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "%s = ", cName);
                if ( memberFunctionType != NULL ) {
                    xl6CgenMemberFunctionStatementInvokeExpr(ctx, initExpr, memberFunctionType, memberTempName);
                } else {
                    xl6CgenExpr(ctx, initExpr);
                }
                xl6OutputPuts(";\n", ctx->out);
                xl6AstFree(memberFunctionType);
            }
            xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, autoClassTypeNode, cName, initExpr);
            ctx->currentGenericParams = prevParams;
            ctx->currentGenericArgs = prevArgs;
            return;
        }
    }
    if ( initExpr != NULL && xl6CgenDeclTryTemporaryClassArgInit(ctx, node, typeNode, cType, cName, initExpr) ) {
        return;
    }
    if ( initExpr != NULL && xl6CgenDeclTryTemporaryClassFieldInit(ctx, node, typeNode, cType, cName, initExpr) ) {
        return;
    }
    if ( xl6CgenDeclShouldUseTypedContainer(ctx, typeNode, initExpr, declaredAuto) ) {
        int initByCopy = xl6CgenTypedContainerCanCopyFromExpr(ctx, typeNode, initExpr);
        int initByMove = xl6CgenTypedContainerCanMoveFromExpr(ctx, typeNode, initExpr);
        XL6_AstNode* memberFunctionType = NULL;
        char memberTempName[64];

        memberTempName[0] = '\0';
        if ( initByMove ) {
            memberFunctionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, initExpr);
            if ( memberFunctionType != NULL ) {
                snprintf(memberTempName, sizeof(memberTempName), "__xl6_member_fn_stmt_%d", ctx->tempId++);
                xl6CgenEmitMemberFunctionStatementTemp(ctx, initExpr, memberTempName);
            }
        }
        xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, typeNode), cName);
        if ( initByCopy ) {
            xl6CgenTypedContainerCopyExpr(ctx, typeNode, initExpr);
        } else if ( initByMove ) {
            if ( memberFunctionType != NULL ) {
                xl6CgenMemberFunctionStatementInvokeExpr(ctx, initExpr, memberFunctionType, memberTempName);
            } else {
                xl6CgenExpr(ctx, initExpr);
            }
        } else {
            xl6CgenTypedContainerCreateExpr(ctx, typeNode);
        }
        xl6OutputPuts(";\n", ctx->out);
        xl6AstFree(memberFunctionType);
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
        if ( initExpr != NULL && xl6CgenDeclTryXValueArrayLiteralInit(ctx, typeNode, cName, initExpr) ) {
            return;
        }
        if ( initExpr != NULL && xl6CgenDeclTryXValueNullCoalesceInit(ctx, typeNode, cName, initExpr) ) {
            return;
        }
        if ( initExpr != NULL && xl6CgenDeclTryXValueConditionalInit(ctx, typeNode, cName, initExpr) ) {
            return;
        }
        if ( initExpr != NULL && xl6CgenDeclTryXValueBinaryInit(ctx, typeNode, cName, initExpr) ) {
            return;
        }
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
    if ( initExpr != NULL && xl6CgenDeclTryStaticNullCoalesceInit(ctx, typeNode, cType, cName, initExpr) ) {
        return;
    }
    if ( initExpr != NULL && xl6CgenDeclTryStaticXValueCompareInit(ctx, typeNode, cType, cName, initExpr) ) {
        return;
    }
    if ( initExpr != NULL && xl6CgenDeclTryStaticXValueApproxInit(ctx, typeNode, cType, cName, initExpr) ) {
        return;
    }
    if ( initExpr != NULL && xl6CgenDeclCanEmitCallArgTemp(ctx, typeNode, initExpr) ) {
        xl6OutputPrintf(ctx->out, "%s %s = ", cType, cName);
        xl6CgenDefaultValue(ctx, typeNode);
        xl6OutputPuts(";\n", ctx->out);
        if ( !xl6CgenDeclEmitCallArgTemp(ctx, typeNode, cType, cName, initExpr) ) {
            xl6CgenError(ctx, initExpr, "declaration expression lowering failed");
        }
        return;
    }
    {
        XL6_CgenDeclClassArgTemp* exprTemps = NULL;
        XL6_CgenExprReplacement* exprReplacements = NULL;
        const XL6_CgenExprReplacement* prevExprReplacements = ctx->exprReplacements;
        size_t prevExprReplacementCount = ctx->exprReplacementCount;
        int exprTempCount = 0;
        int exprTempCapacity = 0;

        if ( initExpr != NULL && !xl6CgenIsFunctionType(typeNode) ) {
            xl6CgenDeclCollectClassArgTempsInExpr(ctx, initExpr, &exprTemps, &exprTempCount, &exprTempCapacity);
            exprReplacements = xl6CgenDeclCreateExprReplacements(ctx, exprTemps, exprTempCount);
        }

        if ( exprReplacements != NULL ) {
            xl6CgenDeclEmitClassArgTemps(ctx, exprTemps, exprTempCount, 1);
            xl6CgenIndent(ctx);
            ctx->exprReplacements = exprReplacements;
            ctx->exprReplacementCount = (size_t)exprTempCount;
        }

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
        if ( exprReplacements != NULL ) {
            ctx->exprReplacements = prevExprReplacements;
            ctx->exprReplacementCount = prevExprReplacementCount;
            xl6CgenDeclDestroyClassArgTemps(ctx, exprTemps, exprTempCount);
        }
        xrtFree(exprReplacements);
        xl6CgenDeclFreeClassArgTemps(exprTemps, exprTempCount);
    }
    if ( xl6CgenIsFunctionType(typeNode) && xl6CgenExprIsFunctionBorrow(ctx, initExpr) ) {
        xl6CgenRetainFunctionValue(ctx, node->text);
    }
}
