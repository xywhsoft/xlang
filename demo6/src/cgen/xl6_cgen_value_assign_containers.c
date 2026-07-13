/* typed 容器赋值与复合赋值 lowering。 */

static void xl6CgenTypedContainerDestroyValue(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* cName)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPrintf(ctx->out, "xrtTypedArrayDestroy(%s);\n", cName);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPrintf(ctx->out, "xrtTypedListDestroy(%s);\n", cName);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPrintf(ctx->out, "xrtTypedSetDestroy(%s);\n", cName);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPrintf(ctx->out, "xrtTypedDictDestroy(%s);\n", cName);
    } else {
        xl6CgenError(ctx, typeNode, "typed container destroy requires array/list/set/dict");
    }
}

static void xl6CgenTypedContainerRaiseStmt(XL6_CgenContext* ctx, const XL6_AstNode* at, const char* message)
{
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
                    "xl6_error_raise(\"ContainerError\", \"%s\", \"\", %d, %d);\n",
                    message != NULL ? message : "container operation failed",
                    at != NULL ? at->line : 0,
                    at != NULL ? at->column : 0);
}

static int xl6CgenTypedContainerRightLiteralTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* expr,
    char* tempName,
    size_t tempNameSize)
{
    const XL6_AstNode* literal;

    if ( tempName != NULL && tempNameSize > 0 ) {
        tempName[0] = '\0';
    }

    literal = xl6CgenTypedContainerInitLiteral(ctx, typeNode, expr);
    if ( literal == NULL ) {
        return 0;
    }

    snprintf(tempName, tempNameSize, "__xl6_typed_container_rhs_%d", ctx->tempId++);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, typeNode), tempName);
    xl6CgenTypedContainerCreateExpr(ctx, typeNode);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenTypedContainerFillFromLiteral(ctx, tempName, typeNode, expr);
    return 1;
}

static void xl6CgenTypedContainerExprOrTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    const char* tempName,
    int hasTemp)
{
    if ( hasTemp ) {
        xl6OutputPuts(tempName, ctx->out);
    } else {
        xl6CgenExpr(ctx, expr);
    }
}

static void xl6CgenTypedContainerReplaceWithTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const char* targetPath,
    const char* tempName,
    const XL6_AstNode* at,
    const char* message)
{
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s == 0) {\n", tempName);
    ctx->indent += 1;
    xl6CgenTypedContainerRaiseStmt(ctx, at, message);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6CgenTypedContainerDestroyValue(ctx, targetType, targetPath);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = %s;\n", targetPath, tempName);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static int xl6CgenTypedContainerCompoundAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* left,
    const char* op,
    const XL6_AstNode* right)
{
    XL6_AstNode* ownedType;
    const XL6_AstNode* targetType;
    const XL6_AstNode* concreteType;
    const char* typeName;
    char targetPath[512];
    char targetSlot[64];
    char rightTemp[64];
    char resultTemp[64];
    const char* setFunc;
    int hasRightTemp;

    if ( left == NULL || op == NULL || right == NULL ||
         xl6CTextEquals(op, "=") || xl6CTextEquals(op, "?=") ) {
        return 0;
    }

    if ( left->kind != XL6_AST_NAME && left->kind != XL6_AST_MEMBER ) {
        return 0;
    }
    ownedType = xl6CgenCloneExprTypeForInference(ctx, left);
    targetType = ownedType;

    if ( !xl6CgenIsTypedContainerType(ctx, targetType) ) {
        xl6AstFree(ownedType);
        return 0;
    }
    if ( left->kind == XL6_AST_MEMBER ) {
        snprintf(targetSlot, sizeof(targetSlot), "__xl6_typed_compound_slot_%d", ctx->tempId++);
        snprintf(targetPath, sizeof(targetPath), "(*%s)", targetSlot);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s* %s = &(", xl6CgenTypedContainerCType(ctx, targetType), targetSlot);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(");\n", ctx->out);
    } else if ( !xl6CgenSimpleAssignTargetPath(ctx, left, targetPath, sizeof(targetPath)) ) {
        xl6CgenError(ctx, left, "typed container compound assignment requires a direct variable or class field target");
        xl6AstFree(ownedType);
        return 1;
    }

    concreteType = xl6CgenTypedContainerConcreteType(ctx, targetType);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    hasRightTemp = xl6CgenTypedContainerRightLiteralTemp(ctx, targetType, right, rightTemp, sizeof(rightTemp));

    if ( xl6CTextEquals(typeName, "array") && xl6CTextEquals(op, "+=") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (!xrtTypedArrayExtend(%s, ", targetPath);
        xl6CgenTypedContainerExprOrTemp(ctx, right, rightTemp, hasRightTemp);
        xl6OutputPuts(")) {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenTypedContainerRaiseStmt(ctx, right, "array += failed");
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        if ( hasRightTemp ) {
            xl6CgenIndent(ctx);
            xl6CgenTypedContainerDestroyValue(ctx, targetType, rightTemp);
        }
        xl6AstFree(ownedType);
        return 1;
    }

    if ( xl6CTextEquals(typeName, "list") && xl6CTextEquals(op, "+=") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (!xrtTypedListMerge(%s, ", targetPath);
        xl6CgenTypedContainerExprOrTemp(ctx, right, rightTemp, hasRightTemp);
        xl6OutputPuts(")) {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenTypedContainerRaiseStmt(ctx, right, "list += failed");
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        if ( hasRightTemp ) {
            xl6CgenIndent(ctx);
            xl6CgenTypedContainerDestroyValue(ctx, targetType, rightTemp);
        }
        xl6AstFree(ownedType);
        return 1;
    }

    if ( xl6CTextEquals(typeName, "dict") && xl6CTextEquals(op, "+=") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (!xrtTypedDictMerge(%s, ", targetPath);
        xl6CgenTypedContainerExprOrTemp(ctx, right, rightTemp, hasRightTemp);
        xl6OutputPuts(")) {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenTypedContainerRaiseStmt(ctx, right, "dict += failed");
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        if ( hasRightTemp ) {
            xl6CgenIndent(ctx);
            xl6CgenTypedContainerDestroyValue(ctx, targetType, rightTemp);
        }
        xl6AstFree(ownedType);
        return 1;
    }

    if ( xl6CTextEquals(typeName, "list") && xl6CTextEquals(op, "-=") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "(void)xrtTypedListRemove(%s, (long long)(", targetPath);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts("));\n", ctx->out);
        xl6AstFree(ownedType);
        return 1;
    }

    if ( xl6CTextEquals(typeName, "dict") && xl6CTextEquals(op, "-=") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "(void)xrtTypedDictRemove(%s, ", targetPath);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(", 0);\n", ctx->out);
        xl6AstFree(ownedType);
        return 1;
    }

    if ( xl6CTextEquals(typeName, "set") && xl6CTextEquals(op, "|=") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (!xrtTypedSetMerge(%s, ", targetPath);
        xl6CgenTypedContainerExprOrTemp(ctx, right, rightTemp, hasRightTemp);
        xl6OutputPuts(")) {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenTypedContainerRaiseStmt(ctx, right, "set |= failed");
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        if ( hasRightTemp ) {
            xl6CgenIndent(ctx);
            xl6CgenTypedContainerDestroyValue(ctx, targetType, rightTemp);
        }
        xl6AstFree(ownedType);
        return 1;
    }

    setFunc = NULL;
    if ( xl6CTextEquals(typeName, "set") && xl6CTextEquals(op, "&=") ) {
        setFunc = "xrtTypedSetIntersection";
    } else if ( xl6CTextEquals(typeName, "set") && xl6CTextEquals(op, "-=") ) {
        setFunc = "xrtTypedSetDifference";
    } else if ( xl6CTextEquals(typeName, "set") && xl6CTextEquals(op, "^=") ) {
        setFunc = "xrtTypedSetSymmetricDifference";
    }
    if ( setFunc != NULL ) {
        snprintf(resultTemp, sizeof(resultTemp), "__xl6_typed_set_result_%d", ctx->tempId++);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xtset %s = %s(%s, ", resultTemp, setFunc, targetPath);
        xl6CgenTypedContainerExprOrTemp(ctx, right, rightTemp, hasRightTemp);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenTypedContainerReplaceWithTemp(ctx, targetType, targetPath, resultTemp, right, "set compound assignment failed");
        if ( hasRightTemp ) {
            xl6CgenIndent(ctx);
            xl6CgenTypedContainerDestroyValue(ctx, targetType, rightTemp);
        }
        xl6AstFree(ownedType);
        return 1;
    }

    xl6CgenError(ctx, left, "typed container compound assignment operator is not supported");
    if ( hasRightTemp ) {
        xl6CgenIndent(ctx);
        xl6CgenTypedContainerDestroyValue(ctx, targetType, rightTemp);
    }
    xl6AstFree(ownedType);
    return 1;
}

static int xl6CgenTypedContainerMemberAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* left,
    const XL6_AstNode* right,
    const XL6_AstNode* fieldType)
{
    const XL6_AstNode* literal;
    XL6_AstNode* fieldCanonical;
    XL6_AstNode* rightType;
    XL6_AstNode* memberFunctionType;
    int moveReturn;
    int importDynamic;
    int id;
    char tempName[64];
    char slotName[64];
    char slotPath[80];
    char memberTempName[64];

    if ( !xl6CgenIsTypedContainerType(ctx, fieldType) ) {
        return 0;
    }
    literal = xl6CgenTypedContainerInitLiteral(ctx, fieldType, right);
    fieldCanonical = xl6CgenCloneCanonicalTypeNode(ctx, fieldType);
    rightType = literal == NULL ? xl6CgenCloneExprTypeForInference(ctx, right) : NULL;
    moveReturn = literal == NULL && xl6CgenTypedContainerCanMoveFromExpr(ctx, fieldType, right);
    importDynamic = literal == NULL && xl6CgenTypedContainerCanImportDynamicExpr(ctx, fieldType, right);
    if ( literal == NULL && !importDynamic &&
         (fieldCanonical == NULL || rightType == NULL || !xl6CgenTypeNodeEqual(fieldCanonical, rightType)) ) {
        xl6CgenError(ctx, right, "typed container member assignment requires the same container type or literal");
        xl6AstFree(fieldCanonical);
        xl6AstFree(rightType);
        return 1;
    }

    id = ctx->tempId++;
    snprintf(tempName, sizeof(tempName), "__xl6_typed_member_assign_%d", id);
    snprintf(slotName, sizeof(slotName), "__xl6_typed_member_slot_%d", id);
    snprintf(slotPath, sizeof(slotPath), "(*%s)", slotName);
    memberTempName[0] = '\0';
    memberFunctionType = NULL;
    if ( moveReturn ) {
        memberFunctionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, right);
        if ( memberFunctionType != NULL ) {
            snprintf(memberTempName, sizeof(memberTempName), "__xl6_member_fn_stmt_%d", id);
            xl6CgenEmitMemberFunctionStatementTemp(ctx, right, memberTempName);
        }
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, fieldType), tempName);
    if ( literal != NULL ) {
        /* class 是可跨线程共享的引用对象，其拥有的容器字段从写入时起就必须是共享根。 */
        xl6CgenTypedContainerCreateExprMode(ctx, fieldType, "XRT_OBJMODE_SHARED");
    } else if ( moveReturn ) {
        if ( memberFunctionType != NULL ) {
            xl6CgenMemberFunctionStatementInvokeExpr(ctx, right, memberFunctionType, memberTempName);
        } else {
            xl6CgenExpr(ctx, right);
        }
    } else if ( importDynamic ) {
        xl6CgenTypedContainerImportDynamicExprMode(ctx, fieldType, right, "XRT_OBJMODE_SHARED");
    } else {
        xl6CgenTypedContainerRetainExpr(ctx, fieldType, right);
    }
    xl6OutputPuts(";\n", ctx->out);
    if ( importDynamic ) {
        xl6CgenTypedContainerImportCheckStmt(ctx, tempName, right);
    }
    xl6AstFree(memberFunctionType);
    if ( literal != NULL ) {
        xl6CgenTypedContainerFillFromLiteral(ctx, tempName, fieldType, right);
    } else if ( moveReturn ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", tempName);
        xl6CgenTypedContainerMoveToSharedCExpr(ctx, fieldType, tempName);
        xl6OutputPuts(";\n", ctx->out);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s* %s = &(", xl6CgenTypedContainerCType(ctx, fieldType), slotName);
    xl6CgenExpr(ctx, left);
    xl6OutputPuts(");\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6CgenTypedContainerDestroyValue(ctx, fieldType, slotPath);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "*%s = %s;\n", slotName, tempName);

    xl6AstFree(fieldCanonical);
    xl6AstFree(rightType);
    return 1;
}

