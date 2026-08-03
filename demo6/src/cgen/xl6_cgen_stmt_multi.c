static const XL6_AstNode* xl6CgenResolveMultiReturnCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    XL6_AstNode** ownedTypeArgs)
{
    const XL6_AstNode* callee;
    XL6_AstNode* func;
    const XL6_AstNode* returns;
    XL6_AstNode* typeArgs;

    if ( ownedTypeArgs != NULL ) {
        *ownedTypeArgs = NULL;
    }

    if ( expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return NULL;
    }
    callee = expr->children[0];
    if ( callee == NULL ) {
        return NULL;
    }
    if ( callee->kind == XL6_AST_GENERIC_INSTANT ) {
        const XL6_AstNode* nameNode = callee->childCount > 0 ? callee->children[0] : NULL;
        const XL6_AstNode* explicitArgs = callee->childCount > 1 ? callee->children[1] : NULL;

        if ( nameNode == NULL || nameNode->kind != XL6_AST_NAME || explicitArgs == NULL ) {
            return NULL;
        }
        func = xl6CgenResolveExplicitGenericCallFunc(ctx, expr, nameNode->text, explicitArgs, 1);
        returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
        if ( returns == NULL || returns->childCount <= 1 ) {
            return NULL;
        }
        if ( ownedTypeArgs != NULL ) {
            *ownedTypeArgs = xl6CgenCloneTypeNode(explicitArgs);
            if ( *ownedTypeArgs == NULL ) {
                return NULL;
            }
        }
        return func;
    }
    if ( callee->kind != XL6_AST_NAME ) {
        return NULL;
    }
    typeArgs = NULL;
    func = xl6CgenResolveCallFuncWithTypeArgs(ctx, expr, callee->text, &typeArgs);
    returns = func != NULL && func->childCount > 2 ? func->children[2] : NULL;
    if ( returns == NULL || returns->childCount <= 1 ) {
        xl6AstFree(typeArgs);
        return NULL;
    }
    if ( ownedTypeArgs != NULL ) {
        *ownedTypeArgs = typeArgs;
    } else {
        xl6AstFree(typeArgs);
    }
    return func;
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

static const XL6_AstNode* xl6CgenResolveMultiReturnClassMethod(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode** outOwnerClass,
    const XL6_AstNode** outReceiverType)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* concreteReceiverType;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* returns;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;

    if ( outOwnerClass != NULL ) {
        *outOwnerClass = NULL;
    }
    if ( outReceiverType != NULL ) {
        *outReceiverType = NULL;
    }
    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return NULL;
    }
    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER || callee->childCount == 0 ) {
        return NULL;
    }
    receiver = callee->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return NULL;
    }

    receiverType = NULL;
    concreteReceiverType = NULL;
    if ( xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        receiverType = ctx->currentClass;
        concreteReceiverType = receiverType;
        classNode = ctx->currentClass;
    } else {
        receiverType = xl6CgenFindVarType(ctx, receiver->text);
        concreteReceiverType = xl6CgenClassHandleTargetType(ctx, receiverType);
        if ( concreteReceiverType == NULL ) {
            concreteReceiverType = xl6CgenResolveAliasType(ctx, receiverType);
        }
        classNode = xl6CgenClassNodeForType(ctx, receiverType);
    }
    if ( classNode == NULL ) {
        return NULL;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( concreteReceiverType != NULL && concreteReceiverType->childCount > 0 &&
         xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = concreteReceiverType->children[0];
    }
    ownerClass = NULL;
    method = xl6CgenResolveClassFunc(ctx, classNode, expr, callee->text, &ownerClass);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;

    returns = method != NULL && method->childCount > 2 ? method->children[2] : NULL;
    if ( returns == NULL || returns->childCount <= 1 ) {
        return NULL;
    }
    if ( outOwnerClass != NULL ) {
        *outOwnerClass = ownerClass != NULL ? ownerClass : classNode;
    }
    if ( outReceiverType != NULL ) {
        *outReceiverType = concreteReceiverType;
    }
    return method;
}

static void xl6CgenEmitKnownFuncCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* func,
    const XL6_AstNode* call,
    const XL6_AstNode* typeArgs)
{
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( typeArgs != NULL ) {
        ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
        ctx->currentGenericArgs = typeArgs;
        xl6CgenAddGenericInstance(ctx, func, typeArgs);
    }
    if ( func != NULL && func->kind == XL6_AST_EXPORT_FUNC ) {
        xl6CgenExportInternalSymbol(ctx, func);
    } else {
        xl6CgenFuncSymbol(ctx, func);
    }
    xl6OutputPutc('(', ctx->out);
    if ( call != NULL && call->childCount > 1 ) {
        xl6CgenKnownCallArgs(ctx, func, call->children[1]);
    }
    xl6OutputPutc(')', ctx->out);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static const XL6_AstNode* xl6CgenMultiReturnMemberCallee(const XL6_AstNode* expr, const XL6_AstNode* functionType)
{
    const XL6_AstNode* callee;

    if ( functionType == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return NULL;
    }
    callee = expr->children[0];
    return callee != NULL && callee->kind == XL6_AST_MEMBER ? callee : NULL;
}

static void xl6CgenEmitMultiReturnMemberTemp(XL6_CgenContext* ctx, const XL6_AstNode* member, int id, char* tempName, size_t tempNameSize)
{
    if ( tempName == NULL || tempNameSize == 0 ) {
        return;
    }
    tempName[0] = '\0';
    if ( member == NULL ) {
        return;
    }
    snprintf(tempName, tempNameSize, "__xl6_member_fn_multi_%d", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "XL6_Function %s = ", tempName);
    xl6CgenExpr(ctx, member);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenEmitMultiReturnCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    const XL6_AstNode* func,
    const XL6_AstNode* functionType,
    const XL6_AstNode* classMethod,
    const XL6_AstNode* funcTypeArgs,
    const char* memberTempName)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;

    if ( classMethod != NULL ) {
        callee = expr != NULL && expr->childCount > 0 ? expr->children[0] : NULL;
        if ( callee != NULL && callee->kind == XL6_AST_MEMBER ) {
            xl6CgenClassMethodCall(ctx, expr, callee);
            return;
        }
    }

    if ( functionType != NULL ) {
        callee = expr != NULL && expr->childCount > 0 ? expr->children[0] : NULL;
        args = expr != NULL && expr->childCount > 1 ? expr->children[1] : NULL;
        if ( callee != NULL && callee->kind == XL6_AST_NAME ) {
            xl6CgenFunctionInvoke(ctx, callee->text, functionType, args);
            return;
        }
        if ( callee != NULL && callee->kind == XL6_AST_MEMBER ) {
            xl6CgenFunctionInvoke(ctx, memberTempName != NULL ? memberTempName : "__xl6_missing_member_fn", functionType, args);
            return;
        }
    }

    xl6CgenEmitKnownFuncCall(ctx, func, expr, funcTypeArgs);
}

static void xl6CgenMultiReturnClassMethodStructName(
    XL6_CgenContext* ctx,
    const XL6_AstNode* method,
    const XL6_AstNode* ownerClass,
    const XL6_AstNode* receiverType)
{
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( receiverType != NULL && receiverType->childCount > 0 &&
         ownerClass != NULL && xl6CgenClassHasTypeParams(ownerClass) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(ownerClass);
        ctx->currentGenericArgs = receiverType->children[0];
    }
    xl6CgenFuncReturnStructName(ctx, method);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenMultiReturnStructName(
    XL6_CgenContext* ctx,
    const XL6_AstNode* func,
    const XL6_AstNode* functionType,
    const XL6_AstNode* classMethod,
    const XL6_AstNode* ownerClass,
    const XL6_AstNode* receiverType,
    const XL6_AstNode* funcTypeArgs)
{
    if ( classMethod != NULL ) {
        xl6CgenMultiReturnClassMethodStructName(ctx, classMethod, ownerClass, receiverType);
        return;
    }
    if ( functionType != NULL ) {
        xl6CgenFunctionReturnStructName(ctx,
                                        xl6CgenFunctionParamTypes(functionType),
                                        xl6CgenFunctionReturnTypes(functionType));
        return;
    }
    if ( funcTypeArgs != NULL ) {
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
        ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
        ctx->currentGenericArgs = funcTypeArgs;
        xl6CgenFuncReturnStructName(ctx, func);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
    } else {
        xl6CgenFuncReturnStructName(ctx, func);
    }
}

static XL6_AstNode* xl6CgenConcreteMultiReturnTypes(
    XL6_CgenContext* ctx,
    const XL6_AstNode* returns,
    const XL6_AstNode* func,
    const XL6_AstNode* funcTypeArgs,
    const XL6_AstNode* classMethod,
    const XL6_AstNode* ownerClass,
    const XL6_AstNode* receiverType)
{
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    XL6_AstNode* result;
    size_t i;

    if ( returns == NULL ) {
        return NULL;
    }
    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( func != NULL && funcTypeArgs != NULL ) {
        ctx->currentGenericParams = xl6CgenFuncTypeParams(func);
        ctx->currentGenericArgs = funcTypeArgs;
    } else if ( classMethod != NULL && ownerClass != NULL && receiverType != NULL &&
                receiverType->childCount > 0 && xl6CgenClassHasTypeParams(ownerClass) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(ownerClass);
        ctx->currentGenericArgs = receiverType->children[0];
    }

    result = xl6AstNew(XL6_AST_LIST, NULL);
    for ( i = 0; result != NULL && i < returns->childCount; ++i ) {
        XL6_AstNode* typeNode = xl6CgenCloneCanonicalTypeNode(ctx, returns->children[i]);
        size_t oldCount;
        if ( typeNode == NULL ) {
            xl6AstFree(result);
            result = NULL;
            break;
        }
        oldCount = result->childCount;
        xl6AstAddChild(result, typeNode);
        if ( result->childCount == oldCount ) {
            xl6AstFree(typeNode);
            xl6AstFree(result);
            result = NULL;
            break;
        }
    }
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    return result;
}

static void xl6CgenAddManagedCleanupForType(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode)
{
    XL6_AstNode* classNode;
    XL6_AstNode* nativeClassNode;

    if ( name == NULL || typeNode == NULL ) {
        return;
    }
    if ( xl6CgenIsTaggedEnumType(ctx, typeNode) ) {
        xl6CgenAddTypedValueCleanup(ctx, name, typeNode);
        return;
    }
    if ( xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
        xl6CgenAddStringCleanup(ctx, name, typeNode);
        return;
    }
    if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6CgenAddFunctionCleanup(ctx, name, typeNode);
        return;
    }
    if ( xl6CgenNullableHasManagedInner(ctx, typeNode) ) {
        xl6CgenAddNullableManagedCleanup(ctx, name, typeNode);
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
    nativeClassNode = typeNode->text != NULL ? xl6CgenFindNativeBackedClass(ctx, typeNode->text) : NULL;
    if ( nativeClassNode != NULL ) {
        xl6CgenAddNativeClassCleanup(ctx, name, nativeClassNode);
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
    const XL6_AstNode* classMethod;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* receiverType;
    XL6_AstNode* ownedFunctionType;
    XL6_AstNode* ownedFuncTypeArgs;
    XL6_AstNode* ownedReturns;
    const XL6_AstNode* returns;
    int id;
    size_t i;

    names = node->childCount > 0 ? node->children[0] : NULL;
    expr = node->childCount > 1 ? node->children[1] : NULL;
    ownedFunctionType = NULL;
    ownedFuncTypeArgs = NULL;
    functionType = xl6CgenResolveMultiReturnFunctionType(ctx, expr, &ownedFunctionType);
    func = functionType == NULL ? xl6CgenResolveMultiReturnCall(ctx, expr, &ownedFuncTypeArgs) : NULL;
    ownerClass = NULL;
    receiverType = NULL;
    classMethod = (functionType == NULL && func == NULL) ?
        xl6CgenResolveMultiReturnClassMethod(ctx, expr, &ownerClass, &receiverType) :
        NULL;
    returns = functionType != NULL ? xl6CgenFunctionReturnTypes(functionType) :
              (func != NULL ? func->children[2] :
              (classMethod != NULL && classMethod->childCount > 2 ? classMethod->children[2] : NULL));
    ownedReturns = xl6CgenConcreteMultiReturnTypes(
        ctx, returns, func, ownedFuncTypeArgs, classMethod, ownerClass, receiverType);
    if ( ownedReturns != NULL ) {
        returns = ownedReturns;
    }
    if ( returns == NULL || names == NULL ) {
        xl6AstFree(ownedFunctionType);
        xl6AstFree(ownedFuncTypeArgs);
        xl6AstFree(ownedReturns);
        xl6CgenError(ctx, node, "多返回值声明右侧必须是多返回值 xlang 函数调用");
        return;
    }

    if ( names->childCount != returns->childCount ) {
        xl6AstFree(ownedFunctionType);
        xl6AstFree(ownedFuncTypeArgs);
        xl6AstFree(ownedReturns);
        xl6CgenError(ctx, node, "多返回值声明数量和函数返回值数量不匹配");
        return;
    }

    id = ctx->tempId++;
    {
        char memberTempName[64];
        const XL6_AstNode* memberCallee = xl6CgenMultiReturnMemberCallee(expr, functionType);

        xl6CgenEmitMultiReturnMemberTemp(ctx, memberCallee, id, memberTempName, sizeof(memberTempName));
        xl6CgenIndent(ctx);
        xl6CgenMultiReturnStructName(ctx, func, functionType, classMethod, ownerClass, receiverType, ownedFuncTypeArgs);
        xl6OutputPrintf(ctx->out, " __xl6_multi_%d = ", id);
        xl6CgenEmitMultiReturnCall(ctx, expr, func, functionType, classMethod, ownedFuncTypeArgs, memberTempName[0] != '\0' ? memberTempName : NULL);
        xl6OutputPuts(";\n", ctx->out);
    }

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
        xl6CgenValueStorageNameText(ctx, cName, sizeof(cName), name->text);
        xl6CgenIndent(ctx);
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, typeNode), ctx->out);
        xl6OutputPrintf(ctx->out, " %s = ", cName);
        if ( xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
            if ( xl6CgenIsNullableStringTypeNode(ctx, typeNode) ) {
                xl6OutputPrintf(
                    ctx->out,
                    "xl6_nullable_string_copy(__xl6_multi_%d.value%u)",
                    id,
                    (unsigned)i);
            } else {
                xl6OutputPrintf(ctx->out, "xrtCopyStr(__xl6_multi_%d.value%u, 0u)", id, (unsigned)i);
            }
        } else {
            xl6OutputPrintf(ctx->out, "__xl6_multi_%d.value%u", id, (unsigned)i);
        }
        xl6OutputPuts(";\n", ctx->out);
        if ( xl6CgenIsStringTypeNode(ctx, typeNode) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s == 0) { xl6_runtime_fatal(3); }\n", cName);
        }
        varTypeNode = typeNode;
        ownedTypeNode = xl6CgenCloneTypeNode(typeNode);
        if ( ownedTypeNode != NULL ) {
            xl6CgenOwnTypeNode(ctx, ownedTypeNode);
            varTypeNode = ownedTypeNode;
        }
        xl6CgenAddVarTypeWithFlags(
            ctx,
            name->text,
            varTypeNode,
            xl6CgenIsTypedContainerType(ctx, varTypeNode) ? XL6_CGEN_VAR_TYPED_CONTAINER : 0u);
        xl6CgenAddManagedCleanupForType(ctx, name->text, varTypeNode);
    }
    xl6AstFree(ownedFunctionType);
    xl6AstFree(ownedFuncTypeArgs);
    xl6AstFree(ownedReturns);
}

static void xl6CgenXValueFromTypedCExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* exprText)
{
    const XL6_AstNode* concreteType;
    const char* typeName;
    char cTypeName[512];

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CgenIsSpecialCollectionType(ctx, concreteType) ) {
        xl6OutputPuts("xrtTypeBoxValue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, concreteType);
        xl6OutputPuts(", &(", ctx->out);
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, concreteType), ctx->out);
        xl6OutputPrintf(ctx->out, "){%s})", exprText != NULL ? exprText : "0");
    } else if ( xl6CgenIsTypedContainerType(ctx, concreteType) ) {
        xl6CgenTypedContainerStorageToXValueExpr(ctx, concreteType, exprText);
    } else if ( xl6CgenIsXValueType(concreteType) ) {
        xl6OutputPuts(exprText != NULL ? exprText : "xvoCreateNull()", ctx->out);
    } else if ( xl6CTextEquals(typeName, "error") ) {
        xl6OutputPrintf(ctx->out, "xvoCreateHandle(xrtTypeHandle(), (void*)(%s), 0x00000002u | 0x00000004u)", exprText != NULL ? exprText : "0");
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
    } else if ( xl6CgenIsPlainRecordType(ctx, concreteType) ) {
        xl6CgenUnionTypeTextName(cTypeName, sizeof(cTypeName), typeName);
        xl6CgenBoxSymbol(ctx->out, cTypeName);
        xl6OutputPrintf(ctx->out, "(%s)", exprText != NULL ? exprText : "{}");
    } else if ( xl6CgenIsNativeBackedClassType(ctx, concreteType) ) {
        xl6CgenNativeBackedTypeRefName(cTypeName, sizeof(cTypeName), typeName);
        xl6CgenBoxSymbol(ctx->out, cTypeName);
        xl6OutputPrintf(ctx->out, "(%s)", exprText != NULL ? exprText : "{}");
    } else {
        snprintf(cTypeName, sizeof(cTypeName), "C generator cannot box type into var: %s (%s)",
                 typeName != NULL ? typeName : "<missing>", concreteType != NULL ? xl6AstKindName(concreteType->kind) : "NULL");
        xl6CgenError(ctx, typeNode, cTypeName);
        xl6OutputPuts("xvoCreateNull()", ctx->out);
    }
}

static void xl6CgenMultiAssign(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* targets;
    const XL6_AstNode* expr;
    const XL6_AstNode* func;
    const XL6_AstNode* functionType;
    const XL6_AstNode* classMethod;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* receiverType;
    XL6_AstNode* ownedFunctionType;
    XL6_AstNode* ownedFuncTypeArgs;
    XL6_AstNode* ownedReturns;
    const XL6_AstNode* returns;
    int id;
    size_t i;

    targets = node->childCount > 0 ? node->children[0] : NULL;
    expr = node->childCount > 1 ? node->children[1] : NULL;
    ownedFunctionType = NULL;
    ownedFuncTypeArgs = NULL;
    functionType = xl6CgenResolveMultiReturnFunctionType(ctx, expr, &ownedFunctionType);
    func = functionType == NULL ? xl6CgenResolveMultiReturnCall(ctx, expr, &ownedFuncTypeArgs) : NULL;
    ownerClass = NULL;
    receiverType = NULL;
    classMethod = (functionType == NULL && func == NULL) ?
        xl6CgenResolveMultiReturnClassMethod(ctx, expr, &ownerClass, &receiverType) :
        NULL;
    returns = functionType != NULL ? xl6CgenFunctionReturnTypes(functionType) :
              (func != NULL ? func->children[2] :
              (classMethod != NULL && classMethod->childCount > 2 ? classMethod->children[2] : NULL));
    ownedReturns = xl6CgenConcreteMultiReturnTypes(
        ctx, returns, func, ownedFuncTypeArgs, classMethod, ownerClass, receiverType);
    if ( ownedReturns != NULL ) {
        returns = ownedReturns;
    }
    if ( returns == NULL || targets == NULL ) {
        xl6AstFree(ownedFunctionType);
        xl6AstFree(ownedFuncTypeArgs);
        xl6AstFree(ownedReturns);
        xl6CgenError(ctx, node, "多返回值赋值右侧必须是多返回值 xlang 函数调用");
        return;
    }

    if ( targets->childCount != returns->childCount ) {
        xl6AstFree(ownedFunctionType);
        xl6AstFree(ownedFuncTypeArgs);
        xl6AstFree(ownedReturns);
        xl6CgenError(ctx, node, "多返回值赋值数量和函数返回值数量不匹配");
        return;
    }

    id = ctx->tempId++;
    {
        char memberTempName[64];
        const XL6_AstNode* memberCallee = xl6CgenMultiReturnMemberCallee(expr, functionType);

        xl6CgenEmitMultiReturnMemberTemp(ctx, memberCallee, id, memberTempName, sizeof(memberTempName));
        xl6CgenIndent(ctx);
        xl6CgenMultiReturnStructName(ctx, func, functionType, classMethod, ownerClass, receiverType, ownedFuncTypeArgs);
        xl6OutputPrintf(ctx->out, " __xl6_multi_%d = ", id);
        xl6CgenEmitMultiReturnCall(ctx, expr, func, functionType, classMethod, ownedFuncTypeArgs, memberTempName[0] != '\0' ? memberTempName : NULL);
        xl6OutputPuts(";\n", ctx->out);
    }

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
        if ( xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
            int valueId = ctx->tempId++;

            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "const char** __xl6_multi_string_slot_%d = &(", valueId);
            xl6CgenExpr(ctx, target);
            xl6OutputPuts(");\n", ctx->out);
            xl6CgenIndent(ctx);
            if ( xl6CgenIsNullableStringTypeNode(ctx, typeNode) ) {
                xl6OutputPrintf(
                    ctx->out,
                    "const char* __xl6_multi_string_value_%d = xl6_nullable_string_copy(__xl6_multi_%d.value%u);\n",
                    valueId,
                    id,
                    (unsigned)i);
            } else {
                xl6OutputPrintf(ctx->out,
                                "const char* __xl6_multi_string_value_%d = xrtCopyStr(__xl6_multi_%d.value%u, 0u);\n",
                                valueId,
                                id,
                                (unsigned)i);
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out,
                                "if (__xl6_multi_string_value_%d == 0) { xl6_runtime_fatal(3); }\n",
                                valueId);
            }
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xrtFree((void*)*__xl6_multi_string_slot_%d);\n", valueId);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out,
                            "*__xl6_multi_string_slot_%d = __xl6_multi_string_value_%d;\n",
                            valueId,
                            valueId);
            continue;
        }
        if ( target->kind == XL6_AST_NAME && xl6CgenIsXValueType(xl6CgenFindVarType(ctx, target->text)) ) {
            xl6CgenValueStorageNameText(ctx, cName, sizeof(cName), target->text);
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
        if ( target->kind == XL6_AST_NAME && xl6CgenIsTypedContainerType(ctx, xl6CgenFindVarType(ctx, target->text)) ) {
            const XL6_AstNode* targetType = xl6CgenFindVarType(ctx, target->text);
            unsigned int targetFlags = xl6CgenFindTrackedVarFlags(ctx, target->text);
            int targetBorrowed = (targetFlags & XL6_CGEN_VAR_BORROWED) != 0;

            xl6CgenValueStorageNameText(ctx, cName, sizeof(cName), target->text);
            if ( targetBorrowed ) {
                char ownerName[512];

                xl6CgenTypedParamOwnerName(ownerName, sizeof(ownerName), target->text);
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "if (%s) {\n", ownerName);
                ctx->indent += 1;
                xl6CgenIndent(ctx);
                xl6CgenTypedContainerDestroyValue(ctx, targetType, cName);
                ctx->indent -= 1;
                xl6CgenIndent(ctx);
                xl6OutputPuts("} else {\n", ctx->out);
                ctx->indent += 1;
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "%s = 1;\n", ownerName);
                ctx->indent -= 1;
                xl6CgenIndent(ctx);
                xl6OutputPuts("}\n", ctx->out);
            } else {
                xl6CgenIndent(ctx);
                xl6CgenTypedContainerDestroyValue(ctx, targetType, cName);
            }
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = __xl6_multi_%d.value%u;\n", cName, id, (unsigned)i);
            continue;
        }
        if ( target->kind == XL6_AST_NAME && xl6CgenIsFunctionType(xl6CgenFindVarType(ctx, target->text)) ) {
            xl6CgenValueStorageNameText(ctx, cName, sizeof(cName), target->text);
            xl6CgenReleaseFunctionValue(ctx, target->text);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = __xl6_multi_%d.value%u;\n", cName, id, (unsigned)i);
            continue;
        }
        if ( target->kind == XL6_AST_NAME &&
             xl6CgenNullableHasManagedInner(
                 ctx,
                 xl6CgenFindVarType(ctx, target->text)) ) {
            const XL6_AstNode* targetType = xl6CgenFindVarType(ctx, target->text);
            char dropName[64];

            xl6CgenValueStorageNameText(ctx, cName, sizeof(cName), target->text);
            xl6CgenNullableDropHelperName(
                dropName,
                sizeof(dropName),
                targetType);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s(&%s);\n", dropName, cName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "%s = __xl6_multi_%d.value%u;\n",
                cName,
                id,
                (unsigned)i);
            continue;
        }
        if ( target->kind == XL6_AST_NAME && xl6CgenClassTypeNeedsFunctionFieldCleanup(ctx, xl6CgenFindVarType(ctx, target->text)) ) {
            xl6CgenValueStorageNameText(ctx, cName, sizeof(cName), target->text);
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
            if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
                char valuePath[96];

                snprintf(valuePath, sizeof(valuePath), "__xl6_multi_%d.value%u", id, (unsigned)i);
                xl6CgenIndent(ctx);
                xl6CgenTypedContainerDestroyValue(ctx, typeNode, valuePath);
            }
            continue;
        }
        xl6CgenIndent(ctx);
        xl6CgenExpr(ctx, target);
        xl6OutputPrintf(ctx->out, " = __xl6_multi_%d.value%u;\n", id, (unsigned)i);
    }
    xl6AstFree(ownedFunctionType);
    xl6AstFree(ownedFuncTypeArgs);
    xl6AstFree(ownedReturns);
}
