static void xl6CgenRegisterParamTypesEx(XL6_CgenContext* ctx, const XL6_AstNode* params, int borrowXValues)
{
    size_t i;

    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( param != NULL && param->childCount > 0 ) {
            unsigned int modeFlags = 0u;
            if ( (param->flags & XL6_AST_FLAG_BYREF) != 0 ) {
                modeFlags |= XL6_CGEN_VAR_BYREF;
            }
            if ( (param->flags & XL6_AST_FLAG_OUT) != 0 ) {
                modeFlags |= XL6_CGEN_VAR_OUT;
            }
            if ( xl6CgenParamTypeUsesTypedContainerStorage(ctx, param->children[0]) ) {
                xl6CgenAddVarTypeWithFlags(ctx, param->text, param->children[0],
                                           XL6_CGEN_VAR_TYPED_CONTAINER | XL6_CGEN_VAR_BORROWED | modeFlags);
                if ( modeFlags == 0 ) {
                    xl6CgenAddTypedContainerParamCleanup(ctx, param->text, param->children[0]);
                }
            } else {
                xl6CgenAddVarTypeWithFlags(ctx, param->text, param->children[0], modeFlags);
                /*
                    普通调用把 xvalue 实参的一个 owned 引用交给被调函数。
                    main 的 arg 由入口包装器持有，因此只在 borrowed 入口跳过释放。
                */
                if ( modeFlags == 0 && !borrowXValues && xl6CgenIsXValueType(param->children[0]) ) {
                    xl6CgenAddXValueCleanup(ctx, param->text, param->children[0]);
                }
                if ( modeFlags == 0 && !borrowXValues && xl6CgenIsWeakType(param->children[0]) ) {
                    xl6CgenAddWeakCleanup(ctx, param->text, param->children[0]);
                }
                if ( modeFlags == 0 && !borrowXValues &&
                     xl6CgenIsFunctionType(param->children[0]) ) {
                    xl6CgenAddFunctionCleanup(
                        ctx,
                        param->text,
                        param->children[0]);
                }
                if ( modeFlags == 0 && !borrowXValues &&
                     xl6CgenNullableHasManagedInner(
                         ctx,
                         param->children[0]) ) {
                    xl6CgenAddNullableManagedCleanup(
                        ctx,
                        param->text,
                        param->children[0]);
                }
                if ( modeFlags == 0 && !borrowXValues &&
                     xl6CgenIsTaggedEnumType(ctx, param->children[0]) ) {
                    xl6CgenAddTypedValueCleanup(
                        ctx,
                        param->text,
                        param->children[0]);
                }
            }
        }
    }
}

static void xl6CgenRegisterParamTypes(XL6_CgenContext* ctx, const XL6_AstNode* params)
{
    xl6CgenRegisterParamTypesEx(ctx, params, 0);
}

static void xl6CgenRegisterBorrowedParamTypes(XL6_CgenContext* ctx, const XL6_AstNode* params)
{
    xl6CgenRegisterParamTypesEx(ctx, params, 1);
}

static int xl6CgenBlockEndsWithReturn(const XL6_AstNode* block);

static void xl6CgenEmitManagedParamOwners(XL6_CgenContext* ctx, const XL6_AstNode* params)
{
    size_t i;

    if ( ctx == NULL || params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* typeNode;
        const XL6_AstNode* classTarget;
        XL6_AstNode* classNode;
        char ownerName[512];

        if ( param == NULL || param->childCount == 0 ) {
            continue;
        }
        typeNode = param->children[0];
        if ( (param->flags & (XL6_AST_FLAG_BYREF | XL6_AST_FLAG_OUT)) != 0 ) {
            continue;
        }
        classTarget = xl6CgenClassHandleTargetType(ctx, typeNode);
        if ( classTarget != NULL ) {
            /* 调用侧已经传递一个 owned 句柄；默认模式和 byval 都共享对象身份。 */
            continue;
        }
        if ( xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
            char paramName[512];
            int nullableString =
                xl6CgenIsNullableStringTypeNode(ctx, typeNode);

            xl6CgenCIdentText(paramName, sizeof(paramName), param->text);
            xl6CgenIndent(ctx);
            if ( nullableString ) {
                xl6OutputPrintf(
                    ctx->out,
                    "%s = xl6_nullable_string_copy(%s);\n",
                    paramName,
                    paramName);
            } else {
                xl6OutputPrintf(ctx->out,
                                "%s = xrtCopyStr(%s != 0 ? %s : \"\", 0u);\n",
                                paramName,
                                paramName,
                                paramName);
                xl6CgenIndent(ctx);
                xl6OutputPrintf(
                    ctx->out,
                    "if (%s == 0) { xl6_runtime_fatal(3); }\n",
                    paramName);
            }
            xl6CgenAddStringCleanup(ctx, param->text, typeNode);
            continue;
        }
        if ( xl6CgenParamTypeUsesTypedContainerStorage(ctx, typeNode) ) {
            xl6CgenTypedParamOwnerName(ownerName, sizeof(ownerName), param->text);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out,
                            "int %s = %d;\n",
                            ownerName,
                            0);
            continue;
        }

        if ( xl6CgenIsNativeBackedClassType(ctx, typeNode) ) {
            continue;
        }
        classNode = xl6CgenClassNodeForType(ctx, typeNode);
        if ( classNode == NULL || !xl6CgenClassNeedsDestroy(ctx, classNode) ) {
            continue;
        }
        xl6CgenCIdentText(ownerName, sizeof(ownerName), param->text);
        if ( !xl6CgenClassHasTypeParams(classNode) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = ", ownerName);
            xl6CgenClassCopyBuiltinHelperSymbol(ctx->out, classNode, 0);
            xl6OutputPrintf(ctx->out, "(%s, 1);\n", ownerName);
        } else {
            XL6_AstNode* canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
            const XL6_AstNode* prevParams = ctx->currentGenericParams;
            const XL6_AstNode* prevArgs = ctx->currentGenericArgs;

            ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
            ctx->currentGenericArgs = canonicalType != NULL
                ? xl6CgenTypeNodeGenericArgs(canonicalType)
                : xl6CgenTypeNodeGenericArgs(typeNode);
            xl6CgenCopyClassFunctionFields(ctx, classNode, ownerName);
            ctx->currentGenericParams = prevParams;
            ctx->currentGenericArgs = prevArgs;
            xl6AstFree(canonicalType);
        }
        xl6CgenAddClassCleanup(ctx, param->text, classNode, typeNode);
    }
}

static int xl6CgenNodeContainsBaseCtorCall(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME &&
         xl6CTextEquals(node->children[0]->text, "base") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeContainsBaseCtorCall(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenEmitAutoBaseCreateWithSelfExpr(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* selfExpr, const XL6_AstNode* atNode)
{
    XL6_AstNode* baseClass;
    const XL6_AstNode* createNode;

    baseClass = xl6CgenBaseClass(ctx, classNode);
    if ( baseClass == NULL ) {
        return;
    }

    createNode = xl6CgenFindClassCreateCallableWithoutArgs(baseClass);
    if ( createNode == NULL ) {
        if ( xl6CgenClassHasCreate(baseClass) ) {
            xl6CgenError(ctx, atNode, "父类 create 需要参数，子类 create 必须显式调用 base(...)");
        }
        return;
    }

    xl6CgenIndent(ctx);
    xl6CgenEmitClassCreateCall(ctx, baseClass, createNode, selfExpr, NULL);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenEmitAutoBaseCreateForSelf(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* atNode)
{
    xl6CgenEmitAutoBaseCreateWithSelfExpr(ctx, classNode, "&self->base", atNode);
}

static void xl6CgenEmitAutoBaseCreateForValue(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* varPath, const XL6_AstNode* atNode)
{
    char selfExpr[512];

    if ( varPath == NULL ) {
        return;
    }
    snprintf(selfExpr, sizeof(selfExpr), "&%s.base", varPath);
    xl6CgenEmitAutoBaseCreateWithSelfExpr(ctx, classNode, selfExpr, atNode);
}

static void xl6CgenEmitNativeHandleCleanupPath(XL6_CgenContext* ctx, const XL6_AstNode* nativeClassNode, const char* handlePath)
{
    const XL6_AstNode* destroyNode;

    if ( nativeClassNode == NULL || handlePath == NULL ) {
        return;
    }
    destroyNode = xl6CgenFindNativeBackedClassSpecial(nativeClassNode, "destroy");
    if ( destroyNode == NULL ) {
        return;
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", handlePath);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6CgenCStringSymbol(ctx->out, destroyNode->text);
    xl6OutputPrintf(ctx->out, "(%s);\n", handlePath);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = 0;\n", handlePath);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenEmitClassDirectFieldCleanups(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    if ( xl6CgenClassHasLocalVirtualSlots(classNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (self->__xl6_self_ref != 0) { xvoUnref(self->__xl6_self_ref); self->__xl6_self_ref = 0; }\n", ctx->out);
    }

    body = xl6CgenClassBody(classNode);
    if ( body == NULL ) {
        return;
    }

    for ( i = body->childCount; i > 0; --i ) {
        const XL6_AstNode* member = body->children[i - 1];
        const XL6_AstNode* typeNode;
        XL6_AstNode* concreteType;
        XL6_AstNode* nativeClassNode;
        XL6_AstNode* nestedClass;
        char path[512];
        char cFieldName[512];

        if ( !xl6CgenIsInstanceField(member) || member->childCount == 0 || member->text == NULL ) {
            continue;
        }
        xl6CgenCIdentText(cFieldName, sizeof(cFieldName), member->text);
        typeNode = member->children[0];
        if ( xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xrtFree((void*)self->%s); self->%s = 0;\n", cFieldName, cFieldName);
            continue;
        }
        if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
            snprintf(path, sizeof(path), "self->%s", cFieldName);
            xl6CgenTypedContainerFieldRef(ctx, typeNode, path, 0);
            continue;
        }
        if ( xl6CgenIsWeakType(typeNode) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoWeakUnit(&self->%s);\n", cFieldName);
            continue;
        }
        if ( xl6CgenIsXValueType(typeNode) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (self->%s != 0) { xvoUnref(self->%s); self->%s = 0; }\n", cFieldName, cFieldName, cFieldName);
            continue;
        }
        if ( xl6CgenIsFunctionType(typeNode) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (self->%s.release != 0 && self->%s.env != 0) {\n", cFieldName, cFieldName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "self->%s.release(self->%s.env);\n", cFieldName, cFieldName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "self->%s.env = 0;\n", cFieldName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "self->%s.release = 0;\n", cFieldName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            continue;
        }
        if ( xl6CgenNullableHasManagedInner(ctx, typeNode) ) {
            char dropName[64];

            xl6CgenNullableDropHelperName(
                dropName,
                sizeof(dropName),
                typeNode);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "%s(&self->%s);\n",
                dropName,
                cFieldName);
            continue;
        }
        nativeClassNode = xl6CgenFindNativeBackedClass(ctx, typeNode != NULL ? typeNode->text : NULL);
        if ( nativeClassNode != NULL && xl6CgenNativeBackedClassHasDestroy(nativeClassNode) ) {
            snprintf(path, sizeof(path), "self->%s.handle", cFieldName);
            xl6CgenEmitNativeHandleCleanupPath(ctx, nativeClassNode, path);
            continue;
        }

        concreteType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
        nestedClass = xl6CgenFindClass(ctx, concreteType != NULL ? concreteType->text : (typeNode != NULL ? typeNode->text : NULL));
        if ( nestedClass != NULL && xl6CgenClassNeedsDestroy(ctx, nestedClass) ) {
            xl6CgenIndent(ctx);
            if ( concreteType != NULL && concreteType->childCount > 0 && xl6CgenClassHasTypeParams(nestedClass) ) {
                xl6CgenGenericClassMemberSymbol(ctx, nestedClass, concreteType, "destroy");
            } else {
                xl6CgenClassMemberSymbol(ctx->out, nestedClass->text, "destroy");
            }
            xl6OutputPrintf(ctx->out, "(&self->%s);\n", cFieldName);
        }
        xl6AstFree(concreteType);
    }
}

static void xl6CgenEmitClassBaseCleanup(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;

    if ( classNode == NULL ) {
        return;
    }
    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return;
    }

    baseClass = xl6CgenFindClass(ctx, base->text);
    if ( baseClass == NULL || !xl6CgenClassNeedsDestroy(ctx, baseClass) ) {
        return;
    }

    xl6CgenIndent(ctx);
    xl6CgenClassMemberSymbol(ctx->out, baseClass->text, "destroy");
    xl6OutputPuts("(&self->base);\n", ctx->out);
}

static void xl6CgenEmitClassDestroyTail(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    xl6CgenEmitClassDirectFieldCleanups(ctx, classNode);
    xl6CgenEmitClassBaseCleanup(ctx, classNode);
}

static void xl6CgenClassFunc(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevReturnParams;
    const XL6_AstNode* prevReturns;
    const XL6_AstNode* prevReturnFunc;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    size_t varMark;
    size_t cleanupMark;

    xl6CgenClassFuncProto(ctx, classNode, funcNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    xl6CgenBeginTempScope(ctx, funcNode);
    prevClass = ctx->currentClass;
    prevReturnType = ctx->currentReturnType;
    prevReturnParams = ctx->currentReturnParams;
    prevReturns = ctx->currentReturns;
    prevReturnFunc = ctx->currentReturnFunc;
    ctx->currentClass = classNode;
    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    ctx->currentReturnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    if ( !xl6CgenIsStaticMember(funcNode) ) {
        xl6CgenAddVarType(ctx, "self", classNode);
    }
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    ctx->currentReturnParams = params;
    ctx->currentReturns = returns;
    ctx->currentReturnFunc = funcNode;
    xl6CgenRegisterParamTypes(ctx, params);
    xl6CgenEmitManagedParamOwners(ctx, params);
    if ( funcNode->childCount > 3 ) {
        xl6CgenBlock(ctx, funcNode->children[3]);
    }
    if ( !xl6CgenBlockEndsWithReturn(funcNode->childCount > 3 ? funcNode->children[3] : NULL) ) {
        xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    }
    xl6CgenEmitDefaultErrorReturn(ctx);
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    ctx->currentReturnType = prevReturnType;
    ctx->currentReturnParams = prevReturnParams;
    ctx->currentReturns = prevReturns;
    ctx->currentReturnFunc = prevReturnFunc;
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenGenericClassFunc(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevReturnParams;
    const XL6_AstNode* prevReturns;
    const XL6_AstNode* prevReturnFunc;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    size_t varMark;
    size_t cleanupMark;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    xl6CgenGenericClassFuncProto(ctx, classNode, typeNode, funcNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    xl6CgenBeginTempScope(ctx, funcNode);
    prevClass = ctx->currentClass;
    prevReturnType = ctx->currentReturnType;
    prevReturnParams = ctx->currentReturnParams;
    prevReturns = ctx->currentReturns;
    prevReturnFunc = ctx->currentReturnFunc;
    ctx->currentClass = classNode;
    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    ctx->currentReturnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    if ( !xl6CgenIsStaticMember(funcNode) ) {
        xl6CgenAddVarType(ctx, "self", typeNode);
    }
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    ctx->currentReturnParams = params;
    ctx->currentReturns = returns;
    ctx->currentReturnFunc = funcNode;
    xl6CgenRegisterParamTypes(ctx, params);
    xl6CgenEmitManagedParamOwners(ctx, params);
    if ( funcNode->childCount > 3 ) {
        xl6CgenBlock(ctx, funcNode->children[3]);
    }
    if ( !xl6CgenBlockEndsWithReturn(funcNode->childCount > 3 ? funcNode->children[3] : NULL) ) {
        xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    }
    xl6CgenEmitDefaultErrorReturn(ctx);
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    ctx->currentReturnType = prevReturnType;
    ctx->currentReturnParams = prevReturnParams;
    ctx->currentReturns = prevReturns;
    ctx->currentReturnFunc = prevReturnFunc;
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenGenericClassSpecial(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const XL6_AstNode* specialNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    const XL6_AstNode* params;
    size_t varMark;
    size_t cleanupMark;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    xl6CgenGenericClassSpecialProto(ctx, classNode, typeNode, specialNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    xl6CgenBeginTempScope(ctx, specialNode);
    prevClass = ctx->currentClass;
    prevReturnType = ctx->currentReturnType;
    ctx->currentClass = classNode;
    ctx->currentReturnType = NULL;
    xl6CgenAddVarType(ctx, "self", typeNode);
    params = specialNode->childCount > 0 ? specialNode->children[0] : NULL;
    xl6CgenRegisterParamTypes(ctx, params);
    xl6CgenEmitManagedParamOwners(ctx, params);
    if ( specialNode->childCount > 1 ) {
        xl6CgenBlock(ctx, specialNode->children[1]);
    }
    if ( xl6CTextEquals(specialNode->text, "destroy") ) {
        xl6CgenEmitClassDestroyTail(ctx, classNode);
    }
    xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    ctx->currentReturnType = prevReturnType;
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenGenericClassAutoDestroy(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* prevClass;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    size_t varMark;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    xl6CgenGenericClassDestroyProto(ctx, classNode, typeNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    ctx->currentClass = classNode;
    xl6CgenAddVarType(ctx, "self", typeNode);
    xl6CgenEmitClassDestroyTail(ctx, classNode);
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenClassSpecial(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* specialNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* params;
    size_t varMark;
    size_t cleanupMark;

    xl6CgenClassSpecialProto(ctx, classNode, specialNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    xl6CgenBeginTempScope(ctx, specialNode);
    prevClass = ctx->currentClass;
    prevReturnType = ctx->currentReturnType;
    ctx->currentClass = classNode;
    ctx->currentReturnType = NULL;
    xl6CgenAddVarType(ctx, "self", classNode);
    params = specialNode->childCount > 0 ? specialNode->children[0] : NULL;
    xl6CgenRegisterParamTypes(ctx, params);
    xl6CgenEmitManagedParamOwners(ctx, params);
    if ( xl6CTextEquals(specialNode->text, "create") &&
         !xl6CgenNodeContainsBaseCtorCall(specialNode->childCount > 1 ? specialNode->children[1] : NULL) ) {
        xl6CgenEmitAutoBaseCreateForSelf(ctx, classNode, specialNode);
    }
    if ( specialNode->childCount > 1 ) {
        xl6CgenBlock(ctx, specialNode->children[1]);
    }
    if ( xl6CTextEquals(specialNode->text, "destroy") ) {
        xl6CgenEmitClassDestroyTail(ctx, classNode);
    }
    xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    ctx->currentReturnType = prevReturnType;
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassAutoDestroy(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* prevClass;
    size_t varMark;

    xl6CgenClassDestroyProto(ctx, classNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    ctx->currentClass = classNode;
    xl6CgenAddVarType(ctx, "self", classNode);
    xl6CgenEmitClassDestroyTail(ctx, classNode);
    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassAutoCopyProtocol(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* protocolName, int deep)
{
    const XL6_AstNode* prevClass;
    size_t varMark;

    xl6CgenClassAutoCopyProto(ctx, classNode, protocolName);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    prevClass = ctx->currentClass;
    ctx->currentClass = classNode;
    xl6CgenAddVarType(ctx, "self", classNode);

    xl6CgenIndent(ctx);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts(" value = {0};\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( self == 0 ) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("return value;\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("value = *self;\n", ctx->out);
    if ( deep ) {
        xl6CgenDeepCopyClassFunctionFields(ctx, classNode, "value");
    } else {
        xl6CgenCopyClassFunctionFields(ctx, classNode, "value");
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts("return value;\n", ctx->out);

    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenEmitNativeExternArgNames(XL6_CgenContext* ctx, const XL6_AstNode* params, int wroteArg)
{
    size_t i;

    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( wroteArg ) {
            xl6OutputPuts(", ", ctx->out);
        }
        xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : "_");
        wroteArg = 1;
    }
}

static void xl6CgenClassExternFunc(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    int hasReturn;
    int copyExternString;

    params = xl6CgenCallableParams(funcNode);
    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    hasReturn = returns != NULL && returns->childCount == 1;
    copyExternString = hasReturn && xl6CgenIsStringTypeNode(ctx, returns->children[0]);

    xl6CgenClassFuncProto(ctx, classNode, funcNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;

    xl6CgenIndent(ctx);
    if ( hasReturn ) {
        xl6OutputPuts("return ", ctx->out);
    }
    if ( copyExternString ) {
        xl6OutputPuts("xl6_xrt_external_string_copy(", ctx->out);
    }
    xl6CgenCStringSymbol(ctx->out, funcNode->text);
    xl6OutputPuts("(self->handle", ctx->out);
    xl6CgenEmitNativeExternArgNames(ctx, params, 1);
    xl6OutputPutc(')', ctx->out);
    if ( copyExternString ) {
        xl6OutputPutc(')', ctx->out);
    }
    xl6OutputPuts(";\n", ctx->out);

    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassExternSpecial(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* specialNode)
{
    const XL6_AstNode* params;
    const char* name;
    const XL6_AstNode* prevClass;
    size_t varMark;
    size_t cleanupMark;

    params = xl6CgenCallableParams(specialNode);
    name = xl6CgenCallableName(specialNode);

    xl6CgenClassSpecialProto(ctx, classNode, specialNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    prevClass = ctx->currentClass;
    ctx->currentClass = classNode;
    xl6CgenAddVarType(ctx, "self", classNode);
    xl6CgenRegisterParamTypes(ctx, params);
    xl6CgenEmitManagedParamOwners(ctx, params);

    if ( xl6CTextEquals(name, "create") ) {
        xl6CgenEmitAutoBaseCreateForSelf(ctx, classNode, specialNode);
        xl6CgenIndent(ctx);
        xl6OutputPuts("self->handle = ", ctx->out);
        xl6CgenCStringSymbol(ctx->out, specialNode->text);
        xl6OutputPutc('(', ctx->out);
        xl6CgenEmitNativeExternArgNames(ctx, params, 0);
        xl6OutputPuts(");\n", ctx->out);
    } else if ( xl6CTextEquals(name, "destroy") ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (self->handle != 0) {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6CgenCStringSymbol(ctx->out, specialNode->text);
        xl6OutputPuts("(self->handle);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("self->handle = 0;\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenEmitClassDestroyTail(ctx, classNode);
    }
    xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);

    ctx->currentClass = prevClass;
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassExportWrapper(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* funcNode)
{
    size_t i;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    int hasParamTemps;

    xl6CgenClassExportWrapperProto(ctx, classNode, funcNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;

    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    hasParamTemps = xl6CgenExportWrapperHasParamTemps(ctx, params);
    xl6CgenEmitExportWrapperParamTemps(ctx, params);

    xl6CgenIndent(ctx);
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFuncReturnStructName(ctx, funcNode);
        xl6OutputPuts(" __xl6_return = ", ctx->out);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        if ( hasParamTemps ) {
            xl6OutputPuts(xl6CgenCAbiTypeName(ctx, returns->children[0]), ctx->out);
            xl6OutputPuts(" __xl6_return = ", ctx->out);
        } else {
            xl6OutputPuts("return ", ctx->out);
        }
    }
    xl6CgenClassCallableSymbol(ctx, classNode, funcNode);
    xl6OutputPuts("(self", ctx->out);
    if ( params != NULL && params->childCount > 0 ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitExportWrapperParamArgs(ctx, params);
    }
    xl6OutputPuts(");\n", ctx->out);
    xl6CgenEmitExportWrapperParamTempCleanups(ctx, params);

    if ( returns != NULL && returns->childCount > 1 ) {
        for ( i = 0; i < returns->childCount; ++i ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if ( out%u != 0 ) {\n", (unsigned)i);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "*out%u = (", (unsigned)i);
            xl6OutputPuts(xl6CgenCAbiTypeName(ctx, returns->children[i]), ctx->out);
            xl6OutputPrintf(ctx->out, ")__xl6_return.value%u;\n", (unsigned)i);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        }
    } else if ( hasParamTemps &&
               returns != NULL &&
               returns->childCount == 1 ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("return __xl6_return;\n", ctx->out);
    }

    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassMethods(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;
    int hasDestroy;

    body = xl6CgenClassBody(classNode);
    if ( body == NULL || xl6CgenClassHasTypeParams(classNode) ) {
        return;
    }
    hasDestroy = 0;
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member == NULL ) {
            continue;
        }
        if ( member->kind == XL6_AST_NATIVE_EXTERN_FUNC || xl6CgenIsExternClassProperty(member) ) {
            xl6CgenClassExternFunc(ctx, classNode, member);
        } else if ( member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL ) {
            if ( xl6CTextEquals(xl6CgenCallableName(member), "destroy") ) {
                hasDestroy = 1;
            }
            xl6CgenClassExternSpecial(ctx, classNode, member);
        } else if ( member->kind == XL6_AST_EXPORT_FUNC ) {
            xl6CgenClassFunc(ctx, classNode, member);
            if ( member->text != NULL && member->text[0] != '\0' ) {
                xl6CgenClassExportWrapper(ctx, classNode, member);
            }
        } else if ( member->kind == XL6_AST_ASYNC_FUNC ) {
            xl6CgenAsyncClassFunc(ctx, classNode, NULL, member);
        } else if ( member->kind == XL6_AST_FUNC || member->kind == XL6_AST_GET || member->kind == XL6_AST_LET ) {
            xl6CgenClassFunc(ctx, classNode, member);
        } else if ( member->kind == XL6_AST_CREATE ) {
            if ( xl6CTextEquals(xl6CgenCallableName(member), "destroy") ) {
                hasDestroy = 1;
            }
            xl6CgenClassSpecial(ctx, classNode, member);
        }
    }
    if ( !hasDestroy && xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        xl6CgenClassAutoDestroy(ctx, classNode);
    }
    if ( xl6CgenClassNeedsAutoCopyProtocol(ctx, classNode, "copy", "__copy__") ) {
        xl6CgenClassAutoCopyProtocol(ctx, classNode, "__copy__", 0);
    }
    if ( xl6CgenClassNeedsAutoCopyProtocol(ctx, classNode, "clone", "__clone__") ) {
        xl6CgenClassAutoCopyProtocol(ctx, classNode, "__clone__", 1);
    }
    if ( xl6CgenClassNeedsAutoValueProtocol(ctx, classNode, "__toValue__") ) {
        xl6CgenClassAutoToValueFormatProtocol(ctx, classNode, NULL);
        if ( xl6CgenClassHasFormatDerive(classNode, "json") ) {
            xl6CgenClassAutoToValueFormatProtocol(ctx, classNode, "json");
        }
        if ( xl6CgenClassHasFormatDerive(classNode, "xson") ) {
            xl6CgenClassAutoToValueFormatProtocol(ctx, classNode, "xson");
        }
        xl6CgenClassAutoToValueProtocol(ctx, classNode);
    }
    if ( xl6CgenClassNeedsAutoValueProtocol(ctx, classNode, "__fromValue__") ) {
        xl6CgenClassAutoFromValueFormatProtocol(ctx, classNode, NULL);
        if ( xl6CgenClassHasFormatDerive(classNode, "json") ) {
            xl6CgenClassAutoFromValueFormatProtocol(ctx, classNode, "json");
        }
        if ( xl6CgenClassHasFormatDerive(classNode, "xson") ) {
            xl6CgenClassAutoFromValueFormatProtocol(ctx, classNode, "xson");
        }
        xl6CgenClassAutoFromValueProtocol(ctx, classNode);
    }
    if ( xl6CgenClassNeedsAutoDeriveProtocol(ctx, classNode, "string", "__toString__") ) {
        xl6CgenClassAutoToStringProtocol(ctx, classNode);
    }
    if ( xl6CgenClassNeedsAutoDeriveProtocol(ctx, classNode, "hash", "__hash__") ) {
        xl6CgenClassAutoHashProtocol(ctx, classNode);
    }
    if ( xl6CgenClassNeedsAutoDeriveProtocol(ctx, classNode, "equals", "__eq__") ) {
        xl6CgenClassAutoEqualsBody(ctx, classNode, 0);
    }
    if ( xl6CgenClassNeedsAutoDeriveProtocol(ctx, classNode, "equals", "__ne__") ) {
        xl6CgenClassAutoEqualsBody(ctx, classNode, 1);
    }
    xl6CgenClassPersistenceHelpers(ctx, classNode);
}

static void xl6CgenGenericClassMethods(XL6_CgenContext* ctx)
{
    size_t i;
    size_t j;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->genericClassInstanceCount; ++i ) {
        const XL6_AstNode* classNode = ctx->genericClassInstances[i].classNode;
        const XL6_AstNode* typeNode = ctx->genericClassInstances[i].typeNode;
        const XL6_AstNode* body = xl6CgenClassBody(classNode);
        int hasDestroy;

        if ( body == NULL ) {
            continue;
        }
        hasDestroy = 0;
        for ( j = 0; j < body->childCount; ++j ) {
            const XL6_AstNode* member = body->children[j];
            if ( member != NULL && member->kind == XL6_AST_ASYNC_FUNC ) {
                xl6CgenAsyncClassFunc(
                    ctx,
                    classNode,
                    typeNode,
                    member);
            } else if ( member != NULL &&
                 (member->kind == XL6_AST_FUNC || member->kind == XL6_AST_GET || member->kind == XL6_AST_LET) ) {
                xl6CgenGenericClassFunc(ctx, classNode, typeNode, member);
            } else if ( member != NULL && member->kind == XL6_AST_CREATE ) {
                if ( xl6CTextEquals(member->text, "destroy") ) {
                    hasDestroy = 1;
                }
                xl6CgenGenericClassSpecial(ctx, classNode, typeNode, member);
            }
        }
        if ( !hasDestroy && xl6CgenGenericClassNeedsDestroy(ctx, classNode, typeNode) ) {
            xl6CgenGenericClassAutoDestroy(ctx, classNode, typeNode);
        }
        xl6CgenEmitGenericDynamicMethodDefinitions(ctx, classNode, typeNode);
    }
}

static int xl6CgenBlockEndsWithReturn(const XL6_AstNode* block)
{
    const XL6_AstNode* last;

    if ( block == NULL || block->childCount == 0 ) {
        return 0;
    }
    last = block->children[block->childCount - 1];
    return last != NULL && last->kind == XL6_AST_RETURN;
}

static void xl6CgenNativeClassFunc(XL6_CgenContext* ctx, const XL6_AstNode* nativeClassNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevReturnParams;
    const XL6_AstNode* prevReturns;
    const XL6_AstNode* prevReturnFunc;
    const XL6_AstNode* prevClass;
    const XL6_AstNode* prevNativeClass;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    const XL6_AstNode* body;
    size_t varMark;
    size_t cleanupMark;

    xl6CgenNativeClassFuncProto(ctx, nativeClassNode, funcNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;

    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    xl6CgenBeginTempScope(ctx, funcNode);
    prevReturnType = ctx->currentReturnType;
    prevReturnParams = ctx->currentReturnParams;
    prevReturns = ctx->currentReturns;
    prevReturnFunc = ctx->currentReturnFunc;
    prevClass = ctx->currentClass;
    prevNativeClass = ctx->currentNativeClass;

    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    body = funcNode->childCount > 3 ? funcNode->children[3] : NULL;

    ctx->currentReturnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    ctx->currentReturnParams = params;
    ctx->currentReturns = returns;
    ctx->currentReturnFunc = funcNode;
    ctx->currentClass = NULL;
    ctx->currentNativeClass = nativeClassNode;

    xl6CgenAddVarType(ctx, "self", nativeClassNode);
    xl6CgenRegisterParamTypes(ctx, params);
    xl6CgenEmitManagedParamOwners(ctx, params);
    if ( body != NULL ) {
        xl6CgenBlock(ctx, body);
    }
    if ( !xl6CgenBlockEndsWithReturn(body) ) {
        xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    }
    xl6CgenEmitDefaultErrorReturn(ctx);
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);

    ctx->currentReturnType = prevReturnType;
    ctx->currentReturnParams = prevReturnParams;
    ctx->currentReturns = prevReturns;
    ctx->currentReturnFunc = prevReturnFunc;
    ctx->currentClass = prevClass;
    ctx->currentNativeClass = prevNativeClass;
    xl6CgenFreeVarsSince(ctx, varMark);

    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenNativeClassMethods(XL6_CgenContext* ctx, const XL6_AstNode* nativeClassNode)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(nativeClassNode);
    if ( body == NULL ) {
        return;
    }

    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL && member->kind == XL6_AST_ASYNC_FUNC ) {
            xl6CgenAsyncNativeClassFunc(
                ctx,
                nativeClassNode,
                member);
        } else if ( member != NULL && member->kind == XL6_AST_FUNC ) {
            xl6CgenNativeClassFunc(ctx, nativeClassNode, member);
        }
    }
}
