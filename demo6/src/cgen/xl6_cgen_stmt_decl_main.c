static void xl6CgenDecl(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const char* cType;
    const XL6_AstNode* typeNode;
    const XL6_AstNode* initExpr;
    XL6_AstNode* ctorNativeClass;
    XL6_AstNode* declNativeClass;
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
    ctorNativeClass = NULL;
    declNativeClass = NULL;
    ctorClass = NULL;
    declClass = NULL;
    ctorClassType = NULL;
    declClassType = NULL;

    if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "auto") && initExpr != NULL ) {
        xl6CgenCallIsNativeBackedCtor(ctx, initExpr, &ctorNativeClass);
        if ( !xl6CgenCallIsClassCtor(ctx, initExpr, &ctorClass) ) {
            ctorClassType = xl6CgenGenericClassCtorType(ctx, initExpr, &ctorClass);
        }
    } else if ( xl6CgenIsNativeBackedClassType(ctx, typeNode) ) {
        declNativeClass = xl6CgenFindNativeBackedClass(ctx, typeNode->text);
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

    if ( ctorNativeClass != NULL || declNativeClass != NULL ) {
        const XL6_AstNode* nativeClassNode = ctorNativeClass != NULL ? ctorNativeClass : declNativeClass;
        xl6CgenNativeBackedTypeName(ctx->out, nativeClassNode->text);
        xl6OutputPrintf(ctx->out, " %s = {0};\n", cName);
        xl6CgenAddVarType(ctx, node->text, nativeClassNode);
        xl6CgenAddNativeClassCleanup(ctx, node->text, nativeClassNode);
        if ( initExpr != NULL ) {
            if ( xl6CgenCallIsNativeBackedCtor(ctx, initExpr, &ctorNativeClass) && xl6CTextEquals(ctorNativeClass->text, nativeClassNode->text) ) {
                xl6CgenNativeBackedCtorAssign(ctx, nativeClassNode, node->text, initExpr);
            } else {
                XL6_AstNode* initType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
                const XL6_AstNode* resolvedInitType = xl6CgenResolveAliasType(ctx, initType);

                /*
                    没有 destroy 的 native-backed class 本质只是句柄值，可以按结构体值复制。
                    有 destroy 的 native-backed class 已经由语义层判为不可复制，这里仍保持构造表达式专用路径。
                */
                if ( resolvedInitType != NULL &&
                     xl6CTextEquals(resolvedInitType->text, nativeClassNode->text) &&
                     (!xl6CgenNativeBackedClassHasDestroy(nativeClassNode) ||
                      (initExpr != NULL && initExpr->kind == XL6_AST_CALL)) ) {
                    xl6CgenIndent(ctx);
                    xl6OutputPrintf(ctx->out, "%s = ", cName);
                    xl6CgenExpr(ctx, initExpr);
                    xl6OutputPuts(";\n", ctx->out);
                } else {
                    xl6CgenError(ctx, initExpr, "native-backed class variable must be initialized by same-type constructor or copyable same-type value");
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
        int upcastInit = 0;
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
        if ( initExpr != NULL && ctorClass == NULL ) {
            xl6CgenCallIsClassCtor(ctx, initExpr, &ctorClass);
        }
        createNode = (initExpr != NULL && ctorClass != NULL && xl6CTextEquals(ctorClass->text, classNode->text)) ?
                     xl6CgenResolveClassCreate(ctx, classNode, initExpr) :
                     xl6CgenFindClassCreateCallableWithoutArgs(classNode);
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
                } else if ( !isGenericClass && xl6CgenExprNeedsClassUpcast(ctx, classTypeNode, initExpr, NULL, 0, NULL, NULL) ) {
                    copyInit = 1;
                    upcastInit = 1;
                } else {
                    xl6CgenError(ctx, initExpr, "class 变量只能使用同类型构造表达式或可复制同类型值初始化");
                }
                xl6AstFree(initType);
            }
        }
        if ( !copyInit ) {
            xl6CgenClassDefaultInitFields(ctx, classNode, cName);
        }
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
            if ( upcastInit ) {
                if ( !xl6CgenClassUpcastStmt(ctx, classTypeNode, initExpr, cName) ) {
                    xl6CgenError(ctx, initExpr, "class initialization upcast lowering failed");
                }
            } else if ( memberFunctionType != NULL ) {
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "%s = ", cName);
                xl6CgenMemberFunctionStatementInvokeExpr(ctx, initExpr, memberFunctionType, memberTempName);
                xl6OutputPuts(";\n", ctx->out);
                xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, isGenericClass ? classTypeNode : classNode, cName, initExpr);
            } else {
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "%s = ", cName);
                xl6CgenExpr(ctx, initExpr);
                xl6OutputPuts(";\n", ctx->out);
                xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, isGenericClass ? classTypeNode : classNode, cName, initExpr);
            }
            xl6AstFree(memberFunctionType);
        } else if ( initExpr != NULL && ctorClass != NULL && xl6CTextEquals(ctorClass->text, classNode->text) ) {
            XL6_CgenClassExprTempScope ctorArgScope;
            int hasCtorArgScope;

            hasCtorArgScope = xl6CgenClassExprTempScopeBegin(ctx, initExpr, &ctorArgScope);
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
            if ( hasCtorArgScope ) {
                xl6CgenClassExprTempScopeEnd(ctx, &ctorArgScope);
            }
        } else if ( initExpr == NULL ) {
            if ( createNode != NULL ) {
                const XL6_AstNode* createParams;

                /*
                    C 风格声明 `T x` 也应完成对象构造。
                    没有显式初始化表达式时，只允许调用无必选参数的 create，
                    否则对象会落入“字段默认值已写、构造体未运行”的半初始化状态。
                */
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
                if ( xl6CgenFixedParamCount(createParams) > 0 ) {
                    xl6OutputPuts(", ", ctx->out);
                    xl6CgenParamNodeArgs(ctx, createParams, NULL);
                }
                xl6OutputPuts(");\n", ctx->out);
                ctx->currentGenericParams = prevParams;
                ctx->currentGenericArgs = prevArgs;
            } else if ( xl6CgenClassHasCreate(classNode) ) {
                xl6CgenError(ctx, node, "class variable requires explicit constructor arguments");
            }
        }
        if ( !copyInit && createNode == NULL && !xl6CgenClassHasCreate(classNode) ) {
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
        int initByImport = xl6CgenTypedContainerCanImportDynamicExpr(ctx, typeNode, initExpr);
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
        if ( initByMove &&
             memberFunctionType == NULL &&
             xl6CgenDeclCallExprArgsNeedCallArgTemps(ctx, initExpr) ) {
            xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, typeNode), cName);
            xl6CgenDefaultValue(ctx, typeNode);
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenAddVarTypeWithFlags(ctx, node->text, typeNode, XL6_CGEN_VAR_TYPED_CONTAINER);
            xl6CgenAddTypedContainerCleanup(ctx, node->text, typeNode);
            if ( !xl6CgenDeclEmitTypedContainerCallExprWithArgTemps(ctx, typeNode, initExpr, cName) ) {
                xl6CgenError(ctx, initExpr, "typed container declaration call argument lowering failed");
            }
            return;
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
        } else if ( initByImport ) {
            xl6CgenTypedContainerImportDynamicExpr(ctx, typeNode, initExpr);
        } else {
            xl6CgenTypedContainerCreateExpr(ctx, typeNode);
        }
        xl6OutputPuts(";\n", ctx->out);
        xl6AstFree(memberFunctionType);
        if ( initByImport ) {
            xl6CgenTypedContainerImportCheckStmt(ctx, cName, initExpr);
        }
        xl6CgenAddVarTypeWithFlags(ctx, node->text, typeNode, XL6_CGEN_VAR_TYPED_CONTAINER);
        xl6CgenAddTypedContainerCleanup(ctx, node->text, typeNode);
        if ( !initByCopy && !initByMove && !initByImport ) {
            XL6_CgenTaskSpawnArgTempScope taskScope;
            int hasTaskScope;

            hasTaskScope = initExpr != NULL &&
                           xl6CgenTaskSpawnArgTempScopeBegin(ctx, initExpr, &taskScope);
            xl6CgenTypedContainerFillFromLiteral(ctx, cName, typeNode, initExpr);
            if ( hasTaskScope ) {
                xl6CgenTaskSpawnArgTempScopeEnd(ctx, &taskScope);
            }
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
        if ( initExpr != NULL && xl6CgenDeclTryTaskSpawnArgTempsInit(ctx, typeNode, "xvalue", cName, initExpr) ) {
            return;
        }
        if ( initExpr != NULL ) {
            xl6CgenXValueLocalInitStmt(ctx, typeNode, cName, initExpr);
        } else {
            xl6OutputPrintf(ctx->out, "xvalue %s = ", cName);
            xl6CgenDefaultXValueForTarget(ctx, typeNode);
            xl6OutputPuts(";\n", ctx->out);
        }
        return;
    }
    if ( xl6CgenIsStringTypeNode(ctx, typeNode) ) {
        xl6CgenAddVarType(ctx, node->text, typeNode);
        xl6CgenAddStringCleanup(ctx, node->text, typeNode);
        xl6OutputPrintf(ctx->out, "const char* %s = xrtCopyStr(", cName);
        if ( initExpr != NULL ) {
            xl6CgenStringSourceExpr(ctx, typeNode, initExpr);
        } else {
            xl6OutputPuts("\"\"", ctx->out);
        }
        xl6OutputPuts(", 0u);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s == 0) { xl6_runtime_fatal(3); }\n", cName);
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
    if ( initExpr != NULL && xl6CgenDeclTryTaskSpawnArgTempsInit(ctx, typeNode, cType, cName, initExpr) ) {
        return;
    }
    if ( initExpr == NULL && xl6CgenIsSyncHandleType(typeNode) ) {
        xl6OutputPrintf(ctx->out, "%s %s = ", cType, cName);
        if ( !xl6CgenSyncDefaultCtorExpr(ctx, typeNode, node->line, node->column) ) {
            xl6CgenDefaultValue(ctx, typeNode);
        }
        xl6OutputPuts(";\n", ctx->out);
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
