static int xl6CgenClassNullableTypedContainerFieldInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* fieldType,
    const XL6_AstNode* initExpr,
    const char* fieldPath)
{
    const XL6_AstNode* innerType;
    const XL6_AstNode* literal;
    XL6_AstNode* targetCanonical;
    XL6_AstNode* sourceType;
    const XL6_AstNode* sourceConcrete;
    XL6_AstNode* sourceCanonical;
    XL6_HirOwnership ownership;
    int sourceShared;
    int id;
    char sourceName[64];

    if ( ctx == NULL || fieldType == NULL || fieldPath == NULL ||
         !xl6CgenIsNullableType(fieldType) ) {
        return 0;
    }
    innerType = xl6CgenNullableInnerType(fieldType);
    if ( !xl6CgenIsTypedContainerType(ctx, innerType) ) {
        return 0;
    }

    if ( initExpr == NULL || initExpr->kind == XL6_AST_NULL ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", fieldPath);
        return 1;
    }

    literal = xl6CgenTypedContainerInitLiteral(ctx, innerType, initExpr);
    if ( literal != NULL ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", fieldPath);
        xl6CgenTypedContainerCreateExprMode(ctx, innerType, "XRT_OBJMODE_SHARED");
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenTypedContainerFillFromLiteral(ctx, fieldPath, innerType, initExpr);
        return 1;
    }

    targetCanonical = xl6CgenCloneCanonicalTypeNode(ctx, innerType);
    sourceType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
    sourceConcrete = xl6CgenIsNullableType(sourceType)
        ? xl6CgenNullableInnerType(sourceType)
        : sourceType;
    sourceCanonical = sourceConcrete != NULL
        ? xl6CgenCloneCanonicalTypeNode(ctx, sourceConcrete)
        : NULL;
    if ( targetCanonical == NULL || sourceCanonical == NULL ||
         !xl6CgenTypeNodeEqual(targetCanonical, sourceCanonical) ) {
        xl6CgenError(
            ctx,
            initExpr,
            "nullable typed container field initializer requires null, the same container type, or a literal");
        xl6AstFree(targetCanonical);
        xl6AstFree(sourceType);
        xl6AstFree(sourceCanonical);
        return 1;
    }

    ownership = xl6CgenHirOwnership(ctx, initExpr);
    sourceShared = xl6CgenTypedContainerExprIsSharedStorage(ctx, initExpr);
    id = ctx->tempId++;
    snprintf(sourceName, sizeof(sourceName), "__xl6_nullable_field_source_%d", id);

    /* 先求值一次，避免带副作用的初始化表达式被重复执行。 */
    xl6CgenIndent(ctx);
    xl6OutputPrintf(
        ctx->out,
        "%s %s = ",
        xl6CgenTypedContainerCType(ctx, innerType),
        sourceName);
    xl6CgenExpr(ctx, initExpr);
    xl6OutputPuts(";\n", ctx->out);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = %s != 0 ? ", fieldPath, sourceName);
    if ( sourceShared && ownership == XL6_HIR_OWNERSHIP_MOVE ) {
        xl6OutputPuts(sourceName, ctx->out);
    } else if ( sourceShared ) {
        xl6CgenTypedContainerRetainCExpr(ctx, innerType, sourceName);
    } else {
        xl6CgenTypedContainerCopyCExprMode(
            ctx,
            innerType,
            sourceName,
            "XRT_OBJMODE_SHARED");
    }
    xl6OutputPuts(" : 0;\n", ctx->out);

    if ( !sourceShared && ownership == XL6_HIR_OWNERSHIP_MOVE ) {
        xl6CgenIndent(ctx);
        xl6CgenTypedContainerDestroyValue(ctx, innerType, sourceName);
    }

    xl6AstFree(targetCanonical);
    xl6AstFree(sourceType);
    xl6AstFree(sourceCanonical);
    return 1;
}

static void xl6CgenClassDefaultInitFields(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* varPath)
{
    const XL6_AstNode* base;
    const XL6_AstNode* body;
    XL6_AstNode* baseClass;
    char basePath[512];
    size_t i;

    if ( classNode == NULL || varPath == NULL ) {
        return;
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        baseClass = xl6CgenFindClass(ctx, base->text);
        if ( baseClass != NULL ) {
            snprintf(basePath, sizeof(basePath), "%s.base", varPath);
            xl6CgenClassDefaultInitFields(ctx, baseClass, basePath);
        }
    }
    xl6CgenClassInitVTables(ctx, classNode, varPath);

    body = classNode->childCount > 1 ? classNode->children[1] : NULL;
    if ( body == NULL ) {
        return;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* fieldType;
        const XL6_AstNode* initExpr;
        XL6_AstNode* fieldNativeClass;
        XL6_AstNode* ctorNativeClass;
        XL6_AstNode* fieldClass;
        XL6_AstNode* ctorClass;
        char fieldPath[512];

        if ( !xl6CgenIsInstanceField(member) ) {
            continue;
        }
        fieldType = member->children[0];
        initExpr = xl6CgenFieldDefaultExpr(member);
        fieldNativeClass = fieldType != NULL ? xl6CgenFindNativeBackedClass(ctx, fieldType->text) : NULL;
        ctorNativeClass = NULL;
        fieldClass = fieldType != NULL ? xl6CgenFindClass(ctx, fieldType->text) : NULL;
        ctorClass = NULL;
        snprintf(fieldPath, sizeof(fieldPath), "%s.", varPath);
        xl6CgenCIdentText(fieldPath + strlen(fieldPath), sizeof(fieldPath) - strlen(fieldPath), member->text);
        if ( xl6CgenIsWeakType(fieldType) ) {
            XL6_AstNode* sourceType;

            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoWeakInit(&%s);\n", fieldPath);
            if ( initExpr == NULL || initExpr->kind == XL6_AST_NULL ) {
                continue;
            }
            sourceType = xl6CgenCloneExprTypeForInference(ctx, initExpr);
            xl6CgenIndent(ctx);
            if ( xl6CgenIsWeakType(sourceType) ) {
                xl6OutputPrintf(ctx->out, "xvoWeakCopy(&%s, &(", fieldPath);
                xl6CgenExpr(ctx, initExpr);
                xl6OutputPuts("));\n", ctx->out);
            } else {
                xl6OutputPrintf(ctx->out, "xvoWeakSet(&%s, ", fieldPath);
                xl6CgenExpr(ctx, initExpr);
                xl6OutputPuts(");\n", ctx->out);
            }
            xl6AstFree(sourceType);
            continue;
        }
        if ( xl6CgenIsSpecialCollectionType(ctx, fieldType) ) {
            xl6CgenIndent(ctx);
            xl6OutputPuts(fieldPath, ctx->out);
            xl6OutputPuts(" = ", ctx->out);
            if ( initExpr == NULL ) {
                xl6CgenSpecialCollectionDefaultCreateExpr(ctx, fieldType);
            } else if ( xl6CgenSpecialCollectionExprIsOwned(ctx, initExpr) ) {
                xl6CgenExpr(ctx, initExpr);
            } else {
                xl6CgenSpecialCollectionRetainExpr(ctx, fieldType, initExpr);
            }
            xl6OutputPuts(";\n", ctx->out);
            continue;
        }
        if ( xl6CgenClassNullableTypedContainerFieldInit(
                 ctx,
                 fieldType,
                 initExpr,
                 fieldPath) ) {
            continue;
        }
        if ( xl6CgenNullableUsesTaggedValue(fieldType) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = ", fieldPath);
            xl6CgenNullableTaggedValueExpr(
                ctx,
                fieldType,
                initExpr);
            xl6OutputPuts(";\n", ctx->out);
            continue;
        }
        if ( xl6CgenIsTypedContainerType(ctx, fieldType) ) {
            const XL6_AstNode* literal = xl6CgenTypedContainerInitLiteral(ctx, fieldType, initExpr);
            int initByCopy = initExpr != NULL && literal == NULL && xl6CgenTypedContainerCanCopyFromExpr(ctx, fieldType, initExpr);
            int initByMove = initExpr != NULL && literal == NULL && xl6CgenTypedContainerCanMoveFromExpr(ctx, fieldType, initExpr);
            int initByImport = initExpr != NULL && literal == NULL && xl6CgenTypedContainerCanImportDynamicExpr(ctx, fieldType, initExpr);

            xl6CgenIndent(ctx);
            xl6OutputPuts(fieldPath, ctx->out);
            xl6OutputPuts(" = ", ctx->out);
            if ( initByCopy ) {
                if ( xl6CgenTypedContainerExprIsSharedStorage(ctx, initExpr) ) {
            xl6CgenTypedContainerRetainExpr(ctx, fieldType, initExpr);
                } else {
                    xl6CgenTypedContainerCopyExprMode(ctx, fieldType, initExpr, "XRT_OBJMODE_SHARED");
                }
            } else if ( initByMove ) {
                xl6CgenTypedContainerMoveToSharedExpr(ctx, fieldType, initExpr);
            } else if ( initByImport ) {
                xl6CgenTypedContainerImportDynamicExprMode(
                    ctx,
                    fieldType,
                    initExpr,
                    "XRT_OBJMODE_SHARED");
            } else {
                xl6CgenTypedContainerCreateExprMode(ctx, fieldType, "XRT_OBJMODE_SHARED");
            }
            xl6OutputPuts(";\n", ctx->out);
            if ( initByImport ) {
                xl6CgenTypedContainerImportCheckStmt(ctx, fieldPath, initExpr);
            }
            if ( !initByCopy && !initByMove && !initByImport ) {
                xl6CgenTypedContainerFillFromLiteral(ctx, fieldPath, fieldType, initExpr);
            }
            continue;
        }
        if ( xl6CgenClassHandleTargetType(ctx, fieldType) != NULL ) {
            if ( initExpr != NULL ) {
                xl6CgenXValueAssignStmt(ctx, fieldType, fieldPath, initExpr);
            } else {
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "%s = xvoCreateNull();\n", fieldPath);
            }
            continue;
        }
        if ( initExpr == NULL ) {
            continue;
        }
        if ( xl6CgenIsOwnedStringTypeNode(ctx, fieldType) ) {
            xl6CgenIndent(ctx);
            xl6OutputPuts(fieldPath, ctx->out);
            xl6OutputPuts(
                xl6CgenIsNullableStringTypeNode(ctx, fieldType) ?
                    " = xl6_nullable_string_copy(" :
                    " = xrtCopyStr(",
                ctx->out);
            xl6CgenStringSourceExpr(ctx, fieldType, initExpr);
            xl6OutputPuts(
                xl6CgenIsNullableStringTypeNode(ctx, fieldType) ?
                    ");\n" :
                    ", 0u);\n",
                ctx->out);
            continue;
        }
        if ( fieldNativeClass != NULL && xl6CgenCallIsNativeBackedCtor(ctx, initExpr, &ctorNativeClass) ) {
            if ( !xl6CTextEquals(fieldNativeClass->text, ctorNativeClass->text) ) {
                xl6CgenError(ctx, initExpr, "native-backed class field initializer constructor type does not match field type");
                continue;
            }
            xl6CgenIndent(ctx);
            xl6OutputPuts(varPath, ctx->out);
            xl6OutputPutc('.', ctx->out);
            xl6CgenFputsCIdent(ctx->out, member->text);
            xl6OutputPuts(".handle = ", ctx->out);
            xl6CgenNativeBackedCtorValue(ctx, ctorNativeClass, initExpr);
            xl6OutputPuts(";\n", ctx->out);
            continue;
        }
        if ( fieldClass != NULL && xl6CgenCallIsClassCtor(ctx, initExpr, &ctorClass) ) {
            if ( !xl6CTextEquals(fieldClass->text, ctorClass->text) ) {
                xl6CgenError(ctx, initExpr, "class field initializer constructor type does not match field type");
                continue;
            }
            xl6CgenEmitClassCtorInto(ctx, initExpr, fieldPath);
            continue;
        }
        xl6CgenIndent(ctx);
        xl6OutputPuts(varPath, ctx->out);
        xl6OutputPutc('.', ctx->out);
        xl6CgenFputsCIdent(ctx->out, member->text);
        xl6OutputPuts(" = ", ctx->out);
        xl6CgenExpr(ctx, initExpr);
        xl6OutputPuts(";\n", ctx->out);
    }
}


/*
	class 的堆对象外壳由 XRT 管理，payload 仍保持透明 C 结构体布局。
	跨线程发布时，只需把 payload 拥有的动态值和 typed 容器切换为共享根。
*/
static void xl6CgenClassPublishSharedFields(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* varPath)
{
    const XL6_AstNode* base;
    const XL6_AstNode* body;
    XL6_AstNode* baseClass;
    char basePath[512];
    size_t i;

    if ( ctx == NULL || classNode == NULL || varPath == NULL ) {
        return;
    }
    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        baseClass = xl6CgenFindClass(ctx, base->text);
        if ( baseClass != NULL ) {
            snprintf(basePath, sizeof(basePath), "%s.base", varPath);
            xl6CgenClassPublishSharedFields(ctx, baseClass, basePath);
        }
    }

    body = classNode->childCount > 1 ? classNode->children[1] : NULL;
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* field = body->children[i];
        const XL6_AstNode* fieldType;
        XL6_AstNode* fieldClass;
        char fieldPath[512];

        if ( !xl6CgenIsInstanceField(field) || field->childCount == 0 ) {
            continue;
        }
        fieldType = field->children[0];
        snprintf(fieldPath, sizeof(fieldPath), "%s.", varPath);
        xl6CgenCIdentText(
            fieldPath + strlen(fieldPath),
            sizeof(fieldPath) - strlen(fieldPath),
            field->text);

        /* class 字段已经是 xvalue 句柄；发布句柄即可，不能按内嵌结构递归展开。 */
        if ( xl6CgenClassHandleTargetType(ctx, fieldType) != NULL ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = xl6_xrt_move_to_shared(%s);\n", fieldPath, fieldPath);
            continue;
        }
        if ( xl6CgenIsXValueType(fieldType) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = xl6_xrt_move_to_shared(%s);\n", fieldPath, fieldPath);
            continue;
        }
        if ( xl6CgenIsTypedContainerType(ctx, fieldType) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = ", fieldPath);
            xl6CgenTypedContainerMoveToSharedCExpr(ctx, fieldType, fieldPath);
            xl6OutputPuts(";\n", ctx->out);
            continue;
        }
        fieldClass = xl6CgenClassNodeForType(ctx, fieldType);
        if ( fieldClass != NULL && xl6CgenFindNativeBackedClass(ctx, fieldClass->text) == NULL ) {
            xl6CgenClassPublishSharedFields(ctx, fieldClass, fieldPath);
        }
    }
}


static void xl6CgenClassValuePublishShared(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* varPath)
{
    XL6_AstNode* classNode;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;

    classNode = xl6CgenClassNodeForType(ctx, typeNode);
    if ( classNode == NULL ) {
        return;
    }
    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( typeNode != NULL && typeNode->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = typeNode->children[0];
    }
    xl6CgenClassPublishSharedFields(ctx, classNode, varPath);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}


/*
    class handle 的外壳由 XRT 管理，字段发布必须作用到它背后的透明 payload。
    这里先发布 payload，再把对象外壳切换为共享模式；空句柄保持为空。
*/
static void xl6CgenClassHandlePublishShared(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* handlePath)
{
    const XL6_AstNode* recordType;
    XL6_AstNode* classNode;
    char cTypeName[512];
    char dataName[64];
    char dataPath[80];
    int id;

    if ( ctx == NULL || typeNode == NULL || handlePath == NULL ) {
        return;
    }
    recordType = xl6CgenClassHandleTargetType(ctx, typeNode);
    classNode = recordType != NULL ? xl6CgenClassNodeForType(ctx, recordType) : NULL;
    if ( recordType == NULL || classNode == NULL ) {
        return;
    }
    if ( xl6CgenFindNativeBackedClass(ctx, classNode->text) == NULL ) {
        id = ctx->tempId++;
        snprintf(dataName, sizeof(dataName), "__xl6_shared_class_data_%d", id);
        snprintf(dataPath, sizeof(dataPath), "(*%s)", dataName);
        xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), recordType);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out,
                        "%s* %s = (%s*)xvoGetRecordData(%s);\n",
                        cTypeName,
                        dataName,
                        cTypeName,
                        handlePath);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", dataName);
        ctx->indent += 1;
        xl6CgenClassValuePublishShared(ctx, recordType, dataPath);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = xl6_xrt_move_to_shared(%s);\n", handlePath, handlePath);
}

static void xl6CgenNativeParamList(XL6_CgenContext* ctx, const XL6_AstNode* params, int includeSelf)
{
    size_t i;
    int wrote;

    wrote = 0;
    if ( includeSelf ) {
        xl6OutputPuts("void* self", ctx->out);
        wrote = 1;
    }
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( wrote ) {
                xl6OutputPuts(", ", ctx->out);
            }
            if ( param->childCount == 0 || xl6CTextEquals(param->children[0]->text, "array<var>") ||
                 xl6CTextEquals(param->children[0]->text, "dict<var>") ) {
                xl6CgenError(ctx, param, "native extern parameter must have a non-vararg C ABI type");
                xl6OutputPuts("int ", ctx->out);
            } else {
                xl6OutputPuts(xl6CgenTypeName(ctx, param->children[0]), ctx->out);
                xl6OutputPutc(' ', ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, param->text);
            wrote = 1;
        }
    }
    if ( !wrote ) {
        xl6OutputPuts("void", ctx->out);
    }
}

static void xl6CgenNativeExternProto(XL6_CgenContext* ctx, const XL6_AstNode* nativeClassNode, const XL6_AstNode* member)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    (void)nativeClassNode;
    if ( member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL ) {
        nameNode = member->childCount > 0 ? member->children[0] : NULL;
        params = member->childCount > 1 ? member->children[1] : NULL;
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, "create") ) {
            xl6OutputPuts("void* ", ctx->out);
            xl6CgenCStringSymbol(ctx->out, member->text);
            xl6OutputPutc('(', ctx->out);
            xl6CgenNativeParamList(ctx, params, 0);
            xl6OutputPutc(')', ctx->out);
        } else if ( nameNode != NULL && xl6CTextEquals(nameNode->text, "destroy") ) {
            xl6OutputPuts("long long ", ctx->out);
            xl6CgenCStringSymbol(ctx->out, member->text);
            xl6OutputPuts("(void* self)", ctx->out);
        }
        return;
    }

    nameNode = member->childCount > 0 ? member->children[0] : NULL;
    (void)nameNode;
    params = member->childCount > 1 ? member->children[1] : NULL;
    returns = member->childCount > 2 ? member->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenError(ctx, returns, "native extern method cannot return multiple values");
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPutc(' ', ctx->out);
    xl6CgenCStringSymbol(ctx->out, member->text);
    xl6OutputPutc('(', ctx->out);
    xl6CgenNativeParamList(ctx, params, 1);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenNativeHelperParamList(XL6_CgenContext* ctx, const XL6_AstNode* nativeClassNode, const XL6_AstNode* params)
{
    size_t i;

    xl6CgenNativeBackedTypeName(ctx->out, nativeClassNode->text);
    xl6OutputPuts("* self", ctx->out);
    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];

        xl6OutputPuts(", ", ctx->out);
        if ( param->childCount == 0 || xl6CTextEquals(param->children[0]->text, "array<var>") ||
             xl6CTextEquals(param->children[0]->text, "dict<var>") ) {
            xl6CgenError(ctx, param, "native-backed class helper C generation does not support vararg methods");
            xl6OutputPuts("int ", ctx->out);
        } else {
            xl6OutputPuts(xl6CgenTypeName(ctx, param->children[0]), ctx->out);
            xl6OutputPutc(' ', ctx->out);
        }
        xl6CgenFputsCIdent(ctx->out, param->text);
    }
}

static void xl6CgenNativeClassFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* nativeClassNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenError(ctx, returns, "native-backed class helper method cannot return multiple values");
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenFuncSingleReturnCType(ctx, funcNode), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPutc(' ', ctx->out);
    xl6CgenNativeCallableSymbol(ctx, nativeClassNode, funcNode);
    xl6OutputPutc('(', ctx->out);
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    xl6CgenNativeHelperParamList(ctx, nativeClassNode, params);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenNativeBackedClassDecls(XL6_CgenContext* ctx, const XL6_AstNode* nativeClassNode)
{
    const XL6_AstNode* body;
    char cTypeName[512];
    size_t i;

    xl6OutputPuts("typedef struct ", ctx->out);
    xl6CgenNativeBackedTypeName(ctx->out, nativeClassNode->text);
    xl6OutputPuts(" { void* handle; } ", ctx->out);
    xl6CgenNativeBackedTypeName(ctx->out, nativeClassNode->text);
    xl6OutputPuts(";\n", ctx->out);

    if ( ctx->needsXValueRuntime ) {
        xl6CgenNativeBackedTypeRefName(cTypeName, sizeof(cTypeName), nativeClassNode->text);
        xl6CgenEmitHandleBoxRuntime(ctx, nativeClassNode, cTypeName, nativeClassNode->text);
    }

    body = xl6CgenClassBody(nativeClassNode);
    if ( body == NULL ) {
        return;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL || member->kind == XL6_AST_NATIVE_EXTERN_FUNC ) {
            xl6CgenNativeExternProto(ctx, nativeClassNode, member);
            xl6OutputPuts(";\n", ctx->out);
        } else if ( member->kind == XL6_AST_ASYNC_FUNC ) {
            xl6CgenAsyncNativeClassFuncProto(
                ctx,
                nativeClassNode,
                member);
            xl6OutputPuts(";\n", ctx->out);
        } else if ( member->kind == XL6_AST_FUNC ) {
            xl6CgenNativeClassFuncProto(ctx, nativeClassNode, member);
            xl6OutputPuts(";\n", ctx->out);
        }
    }
}
