static int xl6CgenClassUpcastPath(
    XL6_CgenContext* ctx,
    const XL6_AstNode* fromClass,
    const XL6_AstNode* targetClass,
    char* buffer,
    size_t capacity)
{
    XL6_AstNode* baseClass;
    size_t len;

    if ( buffer == NULL || capacity == 0 || fromClass == NULL || targetClass == NULL ) {
        return 0;
    }
    if ( xl6CTextEquals(fromClass->text, targetClass->text) ) {
        return 1;
    }

    baseClass = xl6CgenBaseClass(ctx, fromClass);
    if ( baseClass == NULL ) {
        return 0;
    }

    len = strlen(buffer);
    if ( len + 5 >= capacity ) {
        return 0;
    }
    memcpy(buffer + len, ".base", 6);
    if ( xl6CTextEquals(baseClass->text, targetClass->text) ) {
        return 1;
    }
    return xl6CgenClassUpcastPath(ctx, baseClass, targetClass, buffer, capacity);
}

static int xl6CgenExprNeedsClassUpcast(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* expr,
    char* basePath,
    size_t basePathSize,
    XL6_AstNode** outExprType,
    XL6_AstNode** outTargetType)
{
    XL6_AstNode* exprType;
    XL6_AstNode* targetCanonical;
    const XL6_AstNode* concreteExprType;
    const XL6_AstNode* concreteTargetType;
    XL6_AstNode* fromClass;
    XL6_AstNode* targetClass;
    char localBasePath[256];
    char* pathBuffer;
    size_t pathCapacity;
    int ok;

    if ( outExprType != NULL ) {
        *outExprType = NULL;
    }
    if ( outTargetType != NULL ) {
        *outTargetType = NULL;
    }
    pathBuffer = basePath != NULL && basePathSize > 0 ? basePath : localBasePath;
    pathCapacity = basePath != NULL && basePathSize > 0 ? basePathSize : sizeof(localBasePath);
    pathBuffer[0] = '\0';
    if ( ctx == NULL || targetType == NULL || expr == NULL ) {
        return 0;
    }

    exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
    targetCanonical = xl6CgenCloneCanonicalTypeNode(ctx, targetType);
    concreteExprType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, exprType));
    concreteTargetType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, targetCanonical));
    fromClass = xl6CgenClassNodeForType(ctx, concreteExprType);
    targetClass = xl6CgenClassNodeForType(ctx, concreteTargetType);

    ok = fromClass != NULL &&
         targetClass != NULL &&
         !xl6CTextEquals(fromClass->text, targetClass->text) &&
         xl6CgenClassUpcastPath(ctx, fromClass, targetClass, pathBuffer, pathCapacity) &&
         pathBuffer[0] != '\0';

    if ( ok && outExprType != NULL ) {
        *outExprType = exprType;
    } else {
        xl6AstFree(exprType);
    }
    if ( ok && outTargetType != NULL ) {
        *outTargetType = targetCanonical;
    } else {
        xl6AstFree(targetCanonical);
    }
    return ok;
}

static int xl6CgenClassUpcastExpr(XL6_CgenContext* ctx, const XL6_AstNode* targetType, const XL6_AstNode* expr)
{
    char basePath[256];
    XL6_AstNode* exprType;
    XL6_AstNode* targetCanonical;
    XL6_AstNode* targetClass;
    int targetHasVirtualSlots;

    exprType = NULL;
    targetCanonical = NULL;
    if ( !xl6CgenExprNeedsClassUpcast(ctx, targetType, expr, basePath, sizeof(basePath), &exprType, &targetCanonical) ) {
        return 0;
    }

    targetClass = xl6CgenClassNodeForType(ctx, targetCanonical);
    targetHasVirtualSlots = targetClass != NULL && xl6CgenClassHasVirtualSlotsInChain(ctx, targetClass);

    if ( expr->kind == XL6_AST_CALL || targetHasVirtualSlots ) {
        /* 复杂 upcast 拥有资源步骤，只允许语句级 lowering。 */
        xl6CgenError(ctx, expr, "class upcast requires statement-level lowering");
        xl6OutputPrintf(ctx->out, "(%s){0}", xl6CgenTypeName(ctx, targetCanonical));
    } else {
        xl6OutputPutc('(', ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts(")", ctx->out);
        xl6OutputPuts(basePath, ctx->out);
    }

    xl6AstFree(exprType);
    xl6AstFree(targetCanonical);
    return 1;
}

static int xl6CgenClassUpcastStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* expr,
    const char* targetName)
{
    char basePath[256];
    char tempName[64];
    char refName[64];
    char sourceCTypeName[512];
    XL6_AstNode* exprType;
    XL6_AstNode* targetCanonical;
    XL6_AstNode* fromClass;
    XL6_AstNode* targetClass;
    int targetHasVirtualSlots;
    int fromNeedsDestroy;
    int targetNeedsDestroy;
    int id;

    if ( ctx == NULL || targetType == NULL || expr == NULL || targetName == NULL ) {
        return 0;
    }
    exprType = NULL;
    targetCanonical = NULL;
    if ( !xl6CgenExprNeedsClassUpcast(
             ctx,
             targetType,
             expr,
             basePath,
             sizeof(basePath),
             &exprType,
             &targetCanonical) ) {
        return 0;
    }

    fromClass = xl6CgenClassNodeForType(ctx, exprType);
    targetClass = xl6CgenClassNodeForType(ctx, targetCanonical);
    targetHasVirtualSlots = targetClass != NULL && xl6CgenClassHasVirtualSlotsInChain(ctx, targetClass);
    fromNeedsDestroy = fromClass != NULL && xl6CgenClassNeedsDestroy(ctx, fromClass);
    targetNeedsDestroy = targetClass != NULL && xl6CgenClassNeedsDestroy(ctx, targetClass);
    id = ctx->tempId++;
    snprintf(tempName, sizeof(tempName), "__xl6_upcast_tmp_%d", id);
    snprintf(refName, sizeof(refName), "__xl6_upcast_ref_%d", id);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = {0};\n", xl6CgenTypeName(ctx, exprType), tempName);
    if ( targetHasVirtualSlots ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue* %s = 0;\n", refName);
    }
    if ( expr->kind == XL6_AST_CALL ) {
        if ( !xl6CgenEmitClassCtorInto(ctx, expr, tempName) ) {
            xl6CgenError(ctx, expr, "class upcast constructor lowering failed");
        }
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", tempName);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts(";\n", ctx->out);
        if ( fromNeedsDestroy ) {
            xl6CgenRetainClassFunctionFields(ctx, fromClass, tempName);
        }
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = %s%s;\n", targetName, tempName, basePath);
    if ( targetNeedsDestroy ) {
        xl6CgenClassClearVirtualSelfRefs(ctx, targetClass, targetName);
        xl6CgenRetainClassFunctionFields(ctx, targetClass, targetName);
    }
    if ( targetHasVirtualSlots ) {
        xl6CgenClassTypeRefName(sourceCTypeName, sizeof(sourceCTypeName), exprType);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", refName);
        xl6CgenBoxSymbol(ctx->out, sourceCTypeName);
        xl6OutputPrintf(ctx->out, "(%s);\n", tempName);
        xl6CgenClassAssignVirtualSelfRefs(ctx, targetClass, targetName, refName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtValueRelease(%s); }\n", refName, refName);
    }
    if ( fromNeedsDestroy ) {
        xl6CgenIndent(ctx);
        if ( exprType->childCount > 0 && xl6CgenClassHasTypeParams(fromClass) ) {
            xl6CgenGenericClassMemberSymbol(ctx, fromClass, exprType, "destroy");
        } else {
            xl6CgenClassMemberSymbol(ctx->out, fromClass->text, "destroy");
        }
        xl6OutputPrintf(ctx->out, "(&%s);\n", tempName);
    }

    xl6AstFree(exprType);
    xl6AstFree(targetCanonical);
    return 1;
}
