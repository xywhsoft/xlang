}

/*
    需要构造、装箱、再销毁临时对象的表达式不能塞进 C 表达式内部。
    这里统一生成普通语句，保证输出保持标准 C，并让所有权转移清晰可审计。
*/
static int xl6CgenTryClassCtorBoxXValueStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* valueExpr,
    const char* valueName)
{
    XL6_AstNode* rawType;
    XL6_AstNode* canonicalType;
    const XL6_AstNode* typeNode;
    const XL6_AstNode* recordType;
    XL6_AstNode* classNode;
    XL6_AstNode* ctorClass;
    XL6_AstNode* genericType;
    char cTypeName[512];
    char tempName[64];
    int id;
    int isCtor;

    if ( ctx == NULL || valueExpr == NULL || valueName == NULL || valueExpr->kind != XL6_AST_CALL ) {
        return 0;
    }

    rawType = xl6CgenCloneExprTypeForInference(ctx, valueExpr);
    /* 构造表达式必须按实际类型装箱；目标类型只负责接收上转型后的句柄。 */
    canonicalType = xl6CgenCloneCanonicalTypeNode(
        ctx,
        xl6CgenSubstituteGenericType(ctx, rawType != NULL ? rawType : targetType));
    typeNode = canonicalType;
    recordType = xl6CgenClassHandleTargetType(ctx, typeNode);
    if ( recordType == NULL ) {
        recordType = typeNode;
    }
    classNode = xl6CgenClassNodeForType(ctx, typeNode);
    ctorClass = NULL;
    genericType = NULL;
    isCtor = xl6CgenCallIsClassCtor(ctx, valueExpr, &ctorClass);
    if ( !isCtor ) {
        genericType = xl6CgenGenericClassCtorType(ctx, valueExpr, &ctorClass);
        isCtor = genericType != NULL;
    }
    if ( typeNode == NULL || classNode == NULL || !isCtor || ctorClass == NULL ) {
        xl6AstFree(genericType);
        xl6AstFree(canonicalType);
        xl6AstFree(rawType);
        return 0;
    }

    xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), recordType);
    if ( xl6CgenClassIsPureNativeHandle(classNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", valueName);
        xl6CgenBoxSymbol(ctx->out, cTypeName);
        xl6OutputPutc('(', ctx->out);
        if ( !xl6CgenTryNativeClassCtorBoxValue(ctx, classNode, valueExpr, cTypeName) ) {
            xl6CgenError(ctx, valueExpr, "native-backed class constructor boxing failed");
            xl6CgenDefaultClassSymbol(ctx->out, cTypeName);
            xl6OutputPuts("()", ctx->out);
        }
        xl6OutputPuts(");\n", ctx->out);
        xl6AstFree(genericType);
        xl6AstFree(canonicalType);
        xl6AstFree(rawType);
        return 1;
    }

    id = ctx->tempId++;
    snprintf(tempName, sizeof(tempName), "__xl6_box_ctor_stmt_%d", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = {0};\n", xl6CgenTypeName(ctx, recordType), tempName);
    if ( !xl6CgenEmitClassCtorInto(ctx, valueExpr, tempName) ) {
        xl6CgenError(ctx, valueExpr, "class constructor boxing requires statement-level lowering");
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( xl6_error_has() ) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = xvoCreateNull();\n", valueName);
    if ( xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        xl6CgenIndent(ctx);
        if ( recordType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
            xl6CgenGenericClassMemberSymbol(ctx, classNode, recordType, "destroy");
        } else {
            xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
        }
        xl6OutputPrintf(ctx->out, "(&%s);\n", tempName);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenClassValuePublishShared(ctx, recordType, tempName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = xvoCreateRecordMove(", valueName);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("()", ctx->out);
    xl6OutputPrintf(ctx->out, ", &%s);\n", tempName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s == 0) {\n", valueName);
    ctx->indent += 1;
    if ( xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        xl6CgenIndent(ctx);
        if ( recordType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
            xl6CgenGenericClassMemberSymbol(ctx, classNode, recordType, "destroy");
        } else {
            xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
        }
        xl6OutputPrintf(ctx->out, "(&%s);\n", tempName);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = xvoCreateNull();\n", valueName);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6AstFree(genericType);
    xl6AstFree(canonicalType);
    xl6AstFree(rawType);
    return 1;
}

/*
    为没有显式初始化表达式的 class 创建真实对象。
    class 的公开语义是托管引用，但字段默认值和无参 create 仍在底层 C 记录上完成，
    因此这里先完整构造记录，再一次性移入 xvalue。
*/
static int xl6CgenDefaultClassHandleXValueStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const char* valueName,
    const XL6_AstNode* atNode)
{
    XL6_AstNode* canonicalType;
    const XL6_AstNode* typeNode;
    const XL6_AstNode* recordType;
    XL6_AstNode* classNode;
    const XL6_AstNode* createNode;
    const XL6_AstNode* params;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    char cTypeName[512];
    char tempName[64];
    int id;
    int isGeneric;

    if ( ctx == NULL || targetType == NULL || valueName == NULL ) {
        return 0;
    }
    canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, targetType);
    typeNode = canonicalType;
    recordType = xl6CgenClassHandleTargetType(ctx, typeNode);
    classNode = xl6CgenClassNodeForType(ctx, typeNode);
    if ( recordType == NULL || classNode == NULL ) {
        xl6AstFree(canonicalType);
        return 0;
    }

    id = ctx->tempId++;
    snprintf(tempName, sizeof(tempName), "__xl6_default_class_%d", id);
    xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), recordType);
    isGeneric = recordType->childCount > 0 && xl6CgenClassHasTypeParams(classNode);
    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( isGeneric ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = recordType->children[0];
    }

    createNode = xl6CgenFindClassCreateCallableWithoutArgs(classNode);
    if ( createNode == NULL && xl6CgenClassHasCreate(classNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xvoCreateNull();\n", valueName);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        xl6AstFree(canonicalType);
        return 1;
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = {0};\n", xl6CgenTypeName(ctx, recordType), tempName);
    xl6CgenClassDefaultInitFields(ctx, classNode, tempName);
    if ( createNode != NULL ) {
        xl6CgenIndent(ctx);
        if ( isGeneric ) {
            xl6CgenGenericClassCallableSymbol(ctx, classNode, recordType, createNode);
        } else {
            xl6CgenClassCallableSymbol(ctx, classNode, createNode);
        }
        xl6OutputPrintf(ctx->out, "(&%s", tempName);
        params = xl6CgenCallableParams(createNode);
        if ( xl6CgenFixedParamCount(params) > 0 ) {
            xl6OutputPuts(", ", ctx->out);
            xl6CgenParamNodeArgs(ctx, params, NULL);
        }
        xl6OutputPuts(");\n", ctx->out);
    } else {
        xl6CgenEmitAutoBaseCreateForValue(ctx, classNode, tempName, atNode);
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( xl6_error_has() ) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = xvoCreateNull();\n", valueName);
    if ( xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        xl6CgenIndent(ctx);
        if ( isGeneric ) {
            xl6CgenGenericClassMemberSymbol(ctx, classNode, recordType, "destroy");
        } else {
            xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
        }
        xl6OutputPrintf(ctx->out, "(&%s);\n", tempName);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenClassValuePublishShared(ctx, recordType, tempName);
    xl6CgenIndent(ctx);
    if ( xl6CgenClassIsPureNativeHandle(classNode) ) {
        xl6OutputPrintf(ctx->out, "%s = ", valueName);
        xl6CgenBoxSymbol(ctx->out, cTypeName);
        xl6OutputPrintf(ctx->out, "(%s);\n", tempName);
    } else {
        xl6OutputPrintf(ctx->out, "%s = xvoCreateRecordMove(", valueName);
        xl6CgenTypeDescSymbol(ctx->out, cTypeName);
        xl6OutputPrintf(ctx->out, "(), &%s);\n", tempName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s == 0) {\n", valueName);
        ctx->indent += 1;
        if ( xl6CgenClassNeedsDestroy(ctx, classNode) ) {
            xl6CgenIndent(ctx);
            if ( isGeneric ) {
                xl6CgenGenericClassMemberSymbol(ctx, classNode, recordType, "destroy");
            } else {
                xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
            }
            xl6OutputPrintf(ctx->out, "(&%s);\n", tempName);
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xvoCreateNull();\n", valueName);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    xl6AstFree(canonicalType);
    return 1;
}

static int xl6CgenTryTypedContainerReturnBoxXValueStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* valueExpr,
    const char* valueName)
{
    XL6_AstNode* rawType;
    const XL6_AstNode* typeNode;
    char storageName[64];
    int id;

    if ( ctx == NULL || valueExpr == NULL || valueName == NULL ||
         xl6CgenExprUsesTypedContainerStorage(ctx, valueExpr) ||
         !xl6CgenExprIsTypedContainerReturnCall(ctx, valueExpr) ) {
        return 0;
    }

    rawType = xl6CgenCloneExprTypeForInference(ctx, valueExpr);
    typeNode = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, rawType));
    if ( typeNode == NULL || !xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        xl6AstFree(rawType);
        return 0;
    }

    id = ctx->tempId++;
    snprintf(storageName, sizeof(storageName), "__xl6_typed_box_storage_stmt_%d", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, typeNode), storageName);
    xl6CgenExpr(ctx, valueExpr);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", valueName);
    xl6CgenTypedContainerStorageToXValueExpr(ctx, typeNode, storageName);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenTypedContainerLocalDestroyStmt(ctx, typeNode, storageName);
    xl6AstFree(rawType);
    return 1;
}

/* 将既有 class 值复制到动态 xvalue；新构造值由上面的 move 路径接管。 */
static int xl6CgenTryClassValueCopyBoxXValueStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* valueExpr,
    const char* valueName)
{
    XL6_AstNode* rawType;
    const XL6_AstNode* typeNode;
    XL6_AstNode* classNode;
    char cTypeName[512];

    if ( ctx == NULL || valueExpr == NULL || valueName == NULL ||
         (valueExpr->kind != XL6_AST_NAME && valueExpr->kind != XL6_AST_MEMBER) ) {
        return 0;
    }

    rawType = xl6CgenCloneExprTypeForInference(ctx, valueExpr);
    typeNode = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, rawType));
    classNode = xl6CgenClassNodeForType(ctx, typeNode);
    if ( typeNode == NULL || classNode == NULL || xl6CgenClassIsPureNativeHandle(classNode) ) {
        xl6AstFree(rawType);
        return 0;
    }

    xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), typeNode);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = xvoCreateRecord(", valueName);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("(), &(", ctx->out);
    xl6CgenExpr(ctx, valueExpr);
    xl6OutputPuts("));\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s == 0) { %s = xvoCreateNull(); }\n", valueName, valueName);
    xl6AstFree(rawType);
    return 1;
}

/* 接管函数返回的新 class 值，不先复制一份再销毁。 */
static int xl6CgenTryFreshClassValueMoveBoxXValueStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* valueExpr,
    const char* valueName)
{
    XL6_AstNode* rawType;
    const XL6_AstNode* typeNode;
    const XL6_AstNode* recordType;
    XL6_AstNode* classNode;
    char cTypeName[512];
    char tempName[64];
    int id;

    if ( ctx == NULL || valueExpr == NULL || valueName == NULL || valueExpr->kind != XL6_AST_CALL ) {
        return 0;
    }
    rawType = xl6CgenCloneExprTypeForInference(ctx, valueExpr);
    typeNode = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, rawType));
    recordType = xl6CgenClassHandleTargetType(ctx, typeNode);
    if ( recordType == NULL ) {
        recordType = typeNode;
    }
    classNode = xl6CgenClassNodeForType(ctx, typeNode);
    if ( typeNode == NULL || classNode == NULL || xl6CgenClassIsPureNativeHandle(classNode) ) {
        xl6AstFree(rawType);
        return 0;
    }

    id = ctx->tempId++;
    snprintf(tempName, sizeof(tempName), "__xl6_box_fresh_stmt_%d", id);
    xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), recordType);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypeName(ctx, recordType), tempName);
    xl6CgenExpr(ctx, valueExpr);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( xl6_error_has() ) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = xvoCreateNull();\n", valueName);
    if ( xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        xl6CgenIndent(ctx);
        if ( recordType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
            xl6CgenGenericClassMemberSymbol(ctx, classNode, recordType, "destroy");
        } else {
            xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
        }
        xl6OutputPrintf(ctx->out, "(&%s);\n", tempName);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = xvoCreateRecordMove(", valueName);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPrintf(ctx->out, "(), &%s);\n", tempName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s == 0) {\n", valueName);
    ctx->indent += 1;
    if ( xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        xl6CgenIndent(ctx);
        if ( recordType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
            xl6CgenGenericClassMemberSymbol(ctx, classNode, recordType, "destroy");
        } else {
            xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
        }
        xl6OutputPrintf(ctx->out, "(&%s);\n", tempName);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = xvoCreateNull();\n", valueName);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6AstFree(rawType);
    return 1;
}

static int xl6CgenTrySpecialXValueAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const char* valueName,
    const XL6_AstNode* valueExpr)
{
    return xl6CgenTryClassCtorBoxXValueStmt(ctx, targetType, valueExpr, valueName) ||
           xl6CgenTryTypedContainerReturnBoxXValueStmt(ctx, valueExpr, valueName);
}

static int xl6CgenXValueExprNeedsSpecialStmt(XL6_CgenContext* ctx, const XL6_AstNode* valueExpr)
{
    XL6_AstNode* rawType;
    const XL6_AstNode* typeNode;
    XL6_AstNode* classNode;
    XL6_AstNode* ctorClass;
    XL6_AstNode* genericType;
    int result;

    if ( ctx == NULL || valueExpr == NULL ) {
        return 0;
    }
    if ( !xl6CgenExprUsesTypedContainerStorage(ctx, valueExpr) &&
         xl6CgenExprIsTypedContainerReturnCall(ctx, valueExpr) ) {
        return 1;
    }
    if ( valueExpr->kind != XL6_AST_CALL ) {
        return 0;
    }
    if ( valueExpr->childCount > 0 && valueExpr->children[0] != NULL &&
         valueExpr->children[0]->kind == XL6_AST_NAME &&
         xl6CgenFindClass(ctx, valueExpr->children[0]->text) != NULL ) {
        return 1;
    }

    rawType = xl6CgenCloneExprTypeForInference(ctx, valueExpr);
    typeNode = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, rawType));
    classNode = xl6CgenClassNodeForType(ctx, typeNode);
    ctorClass = NULL;
    genericType = NULL;
    result = classNode != NULL && xl6CgenCallIsClassCtor(ctx, valueExpr, &ctorClass);
    if ( !result && classNode != NULL ) {
        genericType = xl6CgenGenericClassCtorType(ctx, valueExpr, &ctorClass);
        result = genericType != NULL && ctorClass != NULL;
    }
    xl6AstFree(genericType);
    xl6AstFree(rawType);
    return result;
}

static void xl6CgenXValueAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const char* valueName,
    const XL6_AstNode* valueExpr)
{
    if ( xl6CgenTrySpecialXValueAssignStmt(ctx, targetType, valueName, valueExpr) ) {
        return;
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", valueName);
    xl6CgenXValueFromExprForTarget(ctx, targetType, valueExpr);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenXValueLocalInitStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const char* valueName,
    const XL6_AstNode* valueExpr)
{
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue %s = 0;\n", valueName);
    xl6CgenXValueAssignStmt(ctx, targetType, valueName, valueExpr);
}

static int xl6CgenTypedContainerNestedLiteralValueStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const char* valueName,
    const XL6_AstNode* valueExpr)
{
    int id;
    char storageName[64];

    if ( !xl6CgenIsTypedContainerType(ctx, targetType) ||
         xl6CgenTypedContainerInitLiteral(ctx, targetType, valueExpr) == NULL ) {
        return 0;
    }

    id = ctx->tempId++;
    snprintf(storageName, sizeof(storageName), "__xl6_nested_container_%d", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, targetType), storageName);
    xl6CgenTypedContainerCreateExpr(ctx, targetType);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenTypedContainerFillFromLiteral(ctx, storageName, targetType, valueExpr);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", valueName);
    xl6CgenTypedContainerStorageToXValueExpr(ctx, targetType, storageName);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenTypedContainerLocalDestroyStmt(ctx, targetType, storageName);
    return 1;
}

static void xl6CgenTypedContainerValueStmt(
    XL6_CgenContext* ctx,
    const char* cName,
    const char* callPrefix,
    const XL6_AstNode* itemType,
    const XL6_AstNode* valueExpr)
{
    int id;
    char valueName[64];

    id = ctx->tempId++;
    snprintf(valueName, sizeof(valueName), "__xl6_typed_value_%d", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue %s = 0;\n", valueName);
    if ( !xl6CgenTypedContainerNestedLiteralValueStmt(ctx, itemType, valueName, valueExpr) ) {
        xl6CgenXValueAssignStmt(ctx, itemType, valueName, valueExpr);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (!%s%s, %s)) {\n", callPrefix, cName, valueName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("xl6_runtime_fatal(3);\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", valueName);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}
