static int xl6CgenErrorAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* left,
    const char* op,
    const XL6_AstNode* right,
    XL6_SemLvalueKind lvalueKind)
{
    const XL6_AstNode* targetType;
    XL6_AstNode* ownedTargetType;
    int id;
    char slotName[64];
    char valueName[64];

    if ( ctx == NULL || left == NULL ||
         (!xl6CTextEquals(op, "=") && !xl6CTextEquals(op, "?=")) ) {
        return 0;
    }

    ownedTargetType = NULL;
    if ( left->kind == XL6_AST_NAME ) {
        targetType = xl6CgenNameAssignTargetType(ctx, left->text);
    } else if ( left->kind == XL6_AST_MEMBER &&
                lvalueKind == XL6_SEM_LVALUE_FIELD ) {
        ownedTargetType = xl6CgenCloneMemberFieldTypeForInference(ctx, left);
        targetType = ownedTargetType;
    } else {
        return 0;
    }
    if ( !xl6CgenIsErrorType(ctx, targetType) ) {
        xl6AstFree(ownedTargetType);
        return 0;
    }

    id = ctx->tempId++;
    snprintf(slotName, sizeof(slotName), "__xl6_error_slot_%d", id);
    snprintf(valueName, sizeof(valueName), "__xl6_error_value_%d", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "XL6_Error** %s = &(", slotName);
    if ( left->kind == XL6_AST_NAME ) {
        char targetName[512];

        xl6CgenNameAssignTargetPath(ctx, left->text, targetName, sizeof(targetName));
        xl6OutputPuts(targetName, ctx->out);
    } else {
        xl6CgenExpr(ctx, left);
    }
    xl6OutputPuts(");\n", ctx->out);

    if ( xl6CTextEquals(op, "?=") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (*%s == 0) {\n", slotName);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "*%s = ", slotName);
        xl6CgenErrorOwnedExpr(ctx, right);
        xl6OutputPuts(";\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6AstFree(ownedTargetType);
        return 1;
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "XL6_Error* %s = ", valueName);
    xl6CgenErrorOwnedExpr(ctx, right);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xl6_error_free(*%s);\n", slotName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "*%s = %s;\n", slotName, valueName);
    xl6AstFree(ownedTargetType);
    return 1;
}

static void xl6CgenAssignStmtPlanned(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    XL6_SemLvalueKind lvalueKind)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    const char* op;

    left = node->childCount > 0 ? node->children[0] : NULL;
    right = node->childCount > 1 ? node->children[1] : NULL;
    op = node->text != NULL ? node->text : "=";
    if ( left == NULL || (left->kind != XL6_AST_NAME && left->kind != XL6_AST_MEMBER && left->kind != XL6_AST_INDEX) ) {
        xl6CgenError(ctx, node, "assignment target must be name, member or index");
        return;
    }
    if ( lvalueKind == XL6_SEM_LVALUE_NONE ) {
        xl6CgenError(ctx, node, "typed HIR 缺少赋值目标计划");
        return;
    }
    if ( (left->kind == XL6_AST_NAME &&
          lvalueKind != XL6_SEM_LVALUE_LOCAL &&
          lvalueKind != XL6_SEM_LVALUE_GLOBAL &&
          lvalueKind != XL6_SEM_LVALUE_DISCARD) ||
         (left->kind == XL6_AST_INDEX &&
          lvalueKind != XL6_SEM_LVALUE_INDEX &&
          lvalueKind != XL6_SEM_LVALUE_PROPERTY) ||
         (left->kind == XL6_AST_MEMBER &&
          lvalueKind != XL6_SEM_LVALUE_FIELD &&
          lvalueKind != XL6_SEM_LVALUE_PROPERTY &&
          lvalueKind != XL6_SEM_LVALUE_DYNAMIC_MEMBER) ) {
        xl6CgenError(ctx, node, "typed HIR 赋值目标计划与 AST 形态不一致");
        return;
    }
    if ( left->kind == XL6_AST_INDEX ) {
        if ( lvalueKind == XL6_SEM_LVALUE_PROPERTY ) {
            if ( !xl6CTextEquals(op, "=")
                 ? xl6CgenClassPropertyCompoundAssignStmt(ctx, node, left, op, right)
                 : xl6CgenClassPropertyIndexSetStmt(ctx, node, left, right) ) {
                return;
            }
            xl6CgenError(ctx, left, "typed HIR 属性索引赋值计划无法生成");
            return;
        }
        if ( xl6CTextEquals(op, "=") && xl6CgenClassProtocolIndexSetStmt(ctx, left, right) ) {
            return;
        }
        if ( xl6CTextEquals(op, "=") && xl6CgenClassProtocolIndexHasGetter(ctx, left) ) {
            xl6CgenError(ctx, left, "class protocol index assignment requires __setItem__");
            return;
        }
        xl6CgenIndexCompoundAssignStmt(ctx, left, op, right);
        return;
    }
    if ( lvalueKind == XL6_SEM_LVALUE_PROPERTY ) {
        if ( !xl6CTextEquals(op, "=")
             ? xl6CgenClassPropertyCompoundAssignStmt(ctx, node, left, op, right)
             : xl6CgenClassPropertyMemberSetStmt(ctx, node, left, right) ) {
            return;
        }
        xl6CgenError(ctx, left, "typed HIR 属性赋值计划无法生成");
        return;
    }
    if ( xl6CgenErrorAssignStmt(ctx, left, op, right, lvalueKind) ) {
        return;
    }
    if ( xl6CgenTypedContainerCompoundAssignStmt(ctx, left, op, right) ) {
        return;
    }
    if ( !xl6CTextEquals(op, "=") && xl6CgenClassProtocolCompoundAssignStmt(ctx, left, op, right) ) {
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* ctorNativeClass;
        XL6_AstNode* fieldType;
        XL6_AstNode* fieldNativeClass;

        ctorNativeClass = NULL;
        if ( lvalueKind == XL6_SEM_LVALUE_DYNAMIC_MEMBER ) {
            if ( !xl6CgenDynamicMemberAssignStmt(ctx, left, right) ) {
                xl6CgenError(ctx, left, "typed HIR dynamic member assignment plan cannot be lowered");
            }
            return;
        }
        if ( xl6CgenClassProtocolMemberSetStmt(ctx, left, right) ) {
            return;
        }
        if ( xl6CgenClassProtocolMemberHasGetter(ctx, left) ) {
            xl6CgenError(ctx, left, "class protocol member assignment requires __setField__");
            return;
        }
        fieldType = xl6CgenCloneMemberFieldTypeForInference(ctx, left);
        if ( xl6CgenIsOwnedStringTypeNode(ctx, fieldType) ) {
            int id = ctx->tempId++;
            char slotName[64];
            char valueName[64];

            snprintf(slotName, sizeof(slotName), "__xl6_string_field_slot_%d", id);
            snprintf(valueName, sizeof(valueName), "__xl6_string_field_value_%d", id);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "const char** %s = &(", slotName);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(");\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "const char* %s = %s(",
                valueName,
                xl6CgenIsNullableStringTypeNode(ctx, fieldType) ?
                    "xl6_nullable_string_copy" :
                    "xl6_xrt_string_dup");
            xl6CgenStringSourceExpr(ctx, fieldType, right);
            xl6OutputPuts(
                xl6CgenIsNullableStringTypeNode(ctx, fieldType) ?
                    ");\n" :
                    ", 0u);\n",
                ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xrtFree((void*)*%s);\n", slotName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "*%s = %s;\n", slotName, valueName);
            xl6AstFree(fieldType);
            return;
        }
        if ( xl6CgenIsWeakType(fieldType) ) {
            XL6_AstNode* sourceType;
            int id = ctx->tempId++;
            char slotName[64];

            snprintf(slotName, sizeof(slotName), "__xl6_weak_field_slot_%d", id);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xl6_weak* %s = &(", slotName);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(");\n", ctx->out);

            sourceType = xl6CgenCloneExprTypeForInference(ctx, right);
            xl6CgenIndent(ctx);
            if ( right == NULL || right->kind == XL6_AST_NULL ) {
                xl6OutputPrintf(ctx->out, "xl6WeakSet(%s, 0);\n", slotName);
            } else if ( xl6CgenIsWeakType(sourceType) &&
                        (right->kind == XL6_AST_CALL || right->kind == XL6_AST_AWAIT) ) {
                xl6OutputPrintf(ctx->out, "xl6WeakTake(%s, ", slotName);
                xl6CgenExpr(ctx, right);
                xl6OutputPuts(");\n", ctx->out);
            } else if ( xl6CgenIsWeakType(sourceType) ) {
                xl6OutputPrintf(ctx->out, "xl6WeakCopy(%s, &(", slotName);
                xl6CgenExpr(ctx, right);
                xl6OutputPuts("));\n", ctx->out);
            } else {
                xl6OutputPrintf(ctx->out, "xl6WeakSet(%s, ", slotName);
                xl6CgenExpr(ctx, right);
                xl6OutputPuts(");\n", ctx->out);
            }
            xl6AstFree(sourceType);
            xl6AstFree(fieldType);
            return;
        }
        if ( xl6CgenIsSpecialCollectionType(ctx, fieldType) ) {
            int id = ctx->tempId++;
            char slotName[64];
            char valueName[64];
            char slotExpr[72];

            snprintf(slotName, sizeof(slotName), "__xl6_special_field_slot_%d", id);
            snprintf(valueName, sizeof(valueName), "__xl6_special_field_value_%d", id);
            snprintf(slotExpr, sizeof(slotExpr), "*%s", slotName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s* %s = &(", xl6CgenTypeName(ctx, fieldType), slotName);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(");\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypeName(ctx, fieldType), valueName);
            if ( xl6CgenSpecialCollectionExprIsOwned(ctx, right) ) {
                xl6CgenExpr(ctx, right);
            } else {
                xl6CgenSpecialCollectionRetainExpr(ctx, fieldType, right);
            }
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6CgenSpecialCollectionReleaseCExpr(ctx, fieldType, slotExpr);
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "*%s = %s;\n", slotName, valueName);
            xl6AstFree(fieldType);
            return;
        }
        if ( xl6CgenTypedContainerMemberAssignStmt(ctx, left, right, fieldType) ) {
            xl6AstFree(fieldType);
            return;
        }
        if ( xl6CgenIsTaggedEnumType(ctx, fieldType) ) {
            int id = ctx->tempId++;
            char slotName[64];
            char valueName[64];
            const char* cFieldType = xl6CgenTypeName(ctx, fieldType);

            snprintf(slotName, sizeof(slotName), "__xl6_enum_field_slot_%d", id);
            snprintf(valueName, sizeof(valueName), "__xl6_enum_field_value_%d", id);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s* %s = &(", cFieldType, slotName);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(");\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s %s = ", cFieldType, valueName);
            xl6CgenTaggedEnumValueExpr(ctx, fieldType, right);
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPuts("xl6TypeDropValue(", ctx->out);
            xl6CgenEmitXrtTypeDescRef(ctx, fieldType);
            xl6OutputPrintf(ctx->out, ", %s);\n", slotName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "*%s = %s;\n", slotName, valueName);
            xl6AstFree(fieldType);
            return;
        }
        if ( xl6CgenXValueMemberAssignStmt(ctx, left, right, fieldType) ) {
            xl6AstFree(fieldType);
            return;
        }
        if ( xl6CgenNullableUsesTaggedValue(fieldType) ) {
            int id = ctx->tempId++;
            char cTypeName[64];
            char dropName[64];
            char slotName[64];
            char valueName[64];

            xl6CgenNullableCTypeName(
                cTypeName,
                sizeof(cTypeName),
                fieldType);
            dropName[0] = '\0';
            if ( xl6CgenNullableHasManagedInner(ctx, fieldType) ) {
                xl6CgenNullableDropHelperName(
                    dropName,
                    sizeof(dropName),
                    fieldType);
            }
            snprintf(
                slotName,
                sizeof(slotName),
                "__xl6_nullable_field_slot_%d",
                id);
            snprintf(
                valueName,
                sizeof(valueName),
                "__xl6_nullable_field_value_%d",
                id);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "%s* %s = &(",
                cTypeName,
                slotName);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(");\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "%s %s = ",
                cTypeName,
                valueName);
            xl6CgenNullableTaggedValueExpr(ctx, fieldType, right);
            xl6OutputPuts(";\n", ctx->out);
            if ( dropName[0] != '\0' ) {
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "%s(%s);\n", dropName, slotName);
            }
            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "*%s = %s;\n",
                slotName,
                valueName);
            xl6AstFree(fieldType);
            return;
        }
        if ( xl6CgenIsFunctionType(fieldType) ) {
            int id = ctx->tempId++;
            char tempName[64];

            snprintf(tempName, sizeof(tempName), "__xl6_function_field_assign_%d", id);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "XL6_Function %s = ", tempName);
            xl6CgenFunctionValueExpr(ctx, right, fieldType);
            xl6OutputPuts(";\n", ctx->out);
            if ( xl6CgenExprIsFunctionBorrow(ctx, right) ) {
                xl6CgenRetainFunctionValue(ctx, tempName);
            }
            xl6CgenIndent(ctx);
            xl6OutputPuts("if (", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".release != 0 && ", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".env != 0) {\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".release(", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".env);\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".env = 0;\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".release = 0;\n", ctx->out);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6CgenExpr(ctx, left);
            xl6OutputPrintf(ctx->out, " = %s;\n", tempName);
            xl6AstFree(fieldType);
            return;
        }
        if ( xl6CgenCallIsNativeBackedCtor(ctx, right, &ctorNativeClass) ) {
            fieldNativeClass = fieldType != NULL ? xl6CgenFindNativeBackedClass(ctx, fieldType->text) : NULL;
            if ( fieldNativeClass == NULL ) {
                xl6AstFree(fieldType);
                xl6CgenError(ctx, left, "native-backed class constructor can only assign to same-type native-backed class field");
                return;
            }
            if ( !xl6CTextEquals(fieldNativeClass->text, ctorNativeClass->text) ) {
                xl6CgenError(ctx, right, "native-backed class constructor type does not match target field type");
                xl6AstFree(fieldType);
                return;
            }
            xl6CgenIndent(ctx);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".handle = ", ctx->out);
            xl6CgenNativeBackedCtorValue(ctx, ctorNativeClass, right);
            xl6OutputPuts(";\n", ctx->out);
            xl6AstFree(fieldType);
            return;
        }
        if ( xl6CgenClassMemberAssignStmt(ctx, left, right, fieldType) ) {
            xl6AstFree(fieldType);
            return;
        }
        xl6AstFree(fieldType);
    }
    if ( left->kind == XL6_AST_MEMBER && xl6CgenMemberAssignIsDynamicTarget(ctx, left) ) {
        xl6CgenError(ctx, node, "dynamic member assignment only supports =");
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME &&
         xl6CgenIsSpecialCollectionType(ctx, xl6CgenNameAssignTargetType(ctx, left->text)) ) {
        const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);
        int id = ctx->tempId++;
        char targetName[512];
        char tempName[64];

        xl6CgenNameAssignTargetPath(ctx, left->text, targetName, sizeof(targetName));
        snprintf(tempName, sizeof(tempName), "__xl6_special_assign_%d", id);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypeName(ctx, targetType), tempName);
        if ( xl6CgenSpecialCollectionExprIsOwned(ctx, right) ) {
            xl6CgenExpr(ctx, right);
        } else {
            xl6CgenSpecialCollectionRetainExpr(ctx, targetType, right);
        }
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6CgenSpecialCollectionReleaseCExpr(ctx, targetType, targetName);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = %s;\n", targetName, tempName);
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME &&
         xl6CgenNullableTypedContainerAssignStmt(
             ctx,
             left,
             right,
             xl6CgenNameAssignTargetType(ctx, left->text)) ) {
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME &&
         xl6CgenIsTypedContainerType(ctx, xl6CgenNameAssignTargetType(ctx, left->text)) ) {
        const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);
        const XL6_AstNode* literal = xl6CgenTypedContainerInitLiteral(ctx, targetType, right);
        XL6_AstNode* targetCanonical = xl6CgenCloneCanonicalTypeNode(ctx, targetType);
        XL6_AstNode* rightType = literal == NULL ? xl6CgenCloneExprStorageType(ctx, right) : NULL;
        int moveReturn = literal == NULL && xl6CgenTypedContainerCanMoveFromExpr(ctx, targetType, right);
        int importDynamic = literal == NULL && xl6CgenTypedContainerCanImportDynamicExpr(ctx, targetType, right);
        unsigned int targetFlags = xl6CgenFindTrackedVarFlags(ctx, left->text);
        int targetBorrowed = (targetFlags & XL6_CGEN_VAR_BORROWED) != 0 &&
                             (targetFlags & (XL6_CGEN_VAR_BYREF | XL6_CGEN_VAR_OUT)) == 0;
        int id = ctx->tempId++;
        char cName[512];
        char tempName[64];
        char memberTempName[64];
        XL6_AstNode* memberFunctionType;

        if ( literal == NULL && !importDynamic &&
             (targetCanonical == NULL || rightType == NULL || !xl6CgenStorageTypeNodeEqual(targetCanonical, rightType)) ) {
            xl6CgenError(ctx, right, "typed container assignment requires the same container type or literal");
            xl6AstFree(targetCanonical);
            xl6AstFree(rightType);
            return;
        }
        xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
        snprintf(tempName, sizeof(tempName), "__xl6_typed_container_assign_%d", id);
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
        xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, targetType), tempName);
        if ( literal != NULL ) {
            xl6CgenTypedContainerCreateExpr(ctx, targetType);
        } else if ( moveReturn ) {
            if ( memberFunctionType != NULL ) {
                xl6CgenMemberFunctionStatementInvokeExpr(ctx, right, memberFunctionType, memberTempName);
            } else {
                xl6CgenExpr(ctx, right);
            }
        } else if ( importDynamic ) {
            xl6CgenTypedContainerImportDynamicExpr(ctx, targetType, right);
        } else {
            xl6CgenTypedContainerRetainExpr(ctx, targetType, right);
        }
        xl6OutputPuts(";\n", ctx->out);
        if ( importDynamic ) {
            xl6CgenTypedContainerImportCheckStmt(ctx, tempName, right);
        }
        xl6AstFree(memberFunctionType);
        if ( literal != NULL ) {
            xl6CgenTypedContainerFillFromLiteral(ctx, tempName, targetType, right);
        }
        if ( targetBorrowed ) {
            char ownerName[512];

            xl6CgenTypedParamOwnerName(ownerName, sizeof(ownerName), left->text);
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
        xl6OutputPrintf(ctx->out, "%s = %s;\n", cName, tempName);
        xl6AstFree(targetCanonical);
        xl6AstFree(rightType);
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME ) {
        const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);
        char targetName[512];

        if ( xl6CgenIsTaggedEnumType(ctx, targetType) ) {
            int id = ctx->tempId++;
            char valueName[64];
            const char* cTargetType = xl6CgenTypeName(ctx, targetType);

            snprintf(valueName, sizeof(valueName), "__xl6_enum_assign_%d", id);
            xl6CgenNameAssignTargetPath(
                ctx,
                left->text,
                targetName,
                sizeof(targetName));
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s %s = ", cTargetType, valueName);
            xl6CgenTaggedEnumValueExpr(ctx, targetType, right);
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPuts("xl6TypeDropValue(", ctx->out);
            xl6CgenEmitXrtTypeDescRef(ctx, targetType);
            xl6OutputPrintf(ctx->out, ", &%s);\n", targetName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = %s;\n", targetName, valueName);
            return;
        }
        if ( xl6CgenNullableUsesTaggedValue(targetType) ) {
            xl6CgenNameAssignTargetPath(ctx, left->text, targetName, sizeof(targetName));
            if ( xl6CgenNullableHasManagedInner(ctx, targetType) ) {
                int id = ctx->tempId++;
                char cTypeName[64];
                char dropName[64];
                char valueName[64];

                xl6CgenNullableCTypeName(
                    cTypeName,
                    sizeof(cTypeName),
                    targetType);
                xl6CgenNullableDropHelperName(
                    dropName,
                    sizeof(dropName),
                    targetType);
                snprintf(
                    valueName,
                    sizeof(valueName),
                    "__xl6_nullable_assign_%d",
                    id);
                xl6CgenIndent(ctx);
                xl6OutputPrintf(
                    ctx->out,
                    "%s %s = ",
                    cTypeName,
                    valueName);
                xl6CgenNullableTaggedValueExpr(ctx, targetType, right);
                xl6OutputPuts(";\n", ctx->out);
                xl6CgenIndent(ctx);
                xl6OutputPrintf(
                    ctx->out,
                    "%s(&%s);\n",
                    dropName,
                    targetName);
                xl6CgenIndent(ctx);
                xl6OutputPrintf(
                    ctx->out,
                    "%s = %s;\n",
                    targetName,
                    valueName);
                return;
            }
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = ", targetName);
            xl6CgenNullableTaggedValueExpr(ctx, targetType, right);
            xl6OutputPuts(";\n", ctx->out);
            return;
        }
        if ( xl6CgenIsWeakType(targetType) ) {
            XL6_AstNode* sourceType;

            xl6CgenNameAssignTargetPath(ctx, left->text, targetName, sizeof(targetName));
            sourceType = xl6CgenCloneExprTypeForInference(ctx, right);
            xl6CgenIndent(ctx);
            if ( right == NULL || right->kind == XL6_AST_NULL ) {
                xl6OutputPrintf(ctx->out, "xl6WeakSet(&(%s), 0);\n", targetName);
            } else if ( xl6CgenIsWeakType(sourceType) &&
                        (right->kind == XL6_AST_CALL || right->kind == XL6_AST_AWAIT) ) {
                xl6OutputPrintf(ctx->out, "xl6WeakTake(&(%s), ", targetName);
                xl6CgenExpr(ctx, right);
                xl6OutputPuts(");\n", ctx->out);
            } else if ( xl6CgenIsWeakType(sourceType) ) {
                xl6OutputPrintf(ctx->out, "xl6WeakCopy(&(%s), &(", targetName);
                xl6CgenExpr(ctx, right);
                xl6OutputPuts("));\n", ctx->out);
            } else {
                xl6OutputPrintf(ctx->out, "xl6WeakSet(&(%s), ", targetName);
                xl6CgenExpr(ctx, right);
                xl6OutputPuts(");\n", ctx->out);
            }
            xl6AstFree(sourceType);
            return;
        }
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME ) {
        const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);
        XL6_AstNode* classNode = xl6CgenClassNodeForType(ctx, targetType);
        int upcastAssign = xl6CgenExprNeedsClassUpcast(ctx, targetType, right, NULL, 0, NULL, NULL);

        if ( classNode != NULL &&
             !xl6CgenIsXValueType(targetType) &&
             (upcastAssign || xl6CgenClassNeedsDestroy(ctx, classNode)) ) {
            int id = ctx->tempId++;
            char cName[512];
            char tempName[64];
            char memberTempName[64];
            XL6_AstNode* memberFunctionType;

            xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
            snprintf(tempName, sizeof(tempName), "__xl6_class_assign_%d", id);
            snprintf(memberTempName, sizeof(memberTempName), "__xl6_member_fn_stmt_%d", id);
            memberFunctionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, right);
            if ( memberFunctionType != NULL ) {
                xl6CgenEmitMemberFunctionStatementTemp(ctx, right, memberTempName);
            }
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s %s = {0};\n", xl6CgenTypeName(ctx, targetType), tempName);
            if ( upcastAssign ) {
                if ( !xl6CgenClassUpcastStmt(ctx, targetType, right, tempName) ) {
                    xl6CgenError(ctx, right, "class assignment upcast lowering failed");
                }
            } else if ( memberFunctionType != NULL ) {
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "%s = ", tempName);
                xl6CgenMemberFunctionStatementInvokeExpr(ctx, right, memberFunctionType, memberTempName);
                xl6OutputPuts(";\n", ctx->out);
                xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, targetType, tempName, right);
            } else {
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "%s = ", tempName);
                xl6CgenExpr(ctx, right);
                xl6OutputPuts(";\n", ctx->out);
                xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, targetType, tempName, right);
            }
            xl6AstFree(memberFunctionType);
            xl6CgenReleaseClassFunctionFieldsForType(ctx, targetType, cName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = %s;\n", cName, tempName);
            return;
        }
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME &&
         xl6CgenIsFunctionType(xl6CgenNameAssignTargetType(ctx, left->text)) ) {
        int id = ctx->tempId++;
        char cName[512];
        char tempName[64];
        const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);

        xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
        snprintf(tempName, sizeof(tempName), "__xl6_function_assign_%d", id);

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "XL6_Function %s = ", tempName);
        xl6CgenFunctionValueExpr(ctx, right, targetType);
        xl6OutputPuts(";\n", ctx->out);
        if ( xl6CgenExprIsFunctionBorrow(ctx, right) ) {
            xl6CgenRetainFunctionValue(ctx, tempName);
        }
        xl6CgenReleaseFunctionValue(ctx, left->text);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = %s;\n", cName, tempName);
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME &&
         xl6CgenIsXValueType(xl6CgenNameAssignTargetType(ctx, left->text)) ) {
        int id = ctx->tempId++;
        char cName[512];
        const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);

        xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));

        if ( !xl6CgenAssignTryXValueConditionalStmt(ctx, targetType, right, id) &&
             !xl6CgenAssignTryXValueBinaryStmt(ctx, targetType, right, id) ) {
            char assignName[64];
            snprintf(assignName, sizeof(assignName), "__xl6_assign_%d", id);
            xl6CgenXValueLocalInitStmt(ctx, targetType, assignName, right);
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cName);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xrtValueRelease(%s);\n", cName);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = __xl6_assign_%d;\n", cName, id);
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME &&
         xl6CgenIsOwnedStringTypeNode(
             ctx,
             xl6CgenNameAssignTargetType(ctx, left->text)) ) {
        int id = ctx->tempId++;
        char cName[512];
        const XL6_AstNode* targetType =
            xl6CgenNameAssignTargetType(ctx, left->text);
        int nullableString =
            xl6CgenIsNullableStringTypeNode(ctx, targetType);

        xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "const char* __xl6_string_assign_%d = %s(",
            id,
            nullableString ?
                "xl6_nullable_string_copy" :
                "xl6_xrt_string_dup");
        xl6CgenStringSourceExpr(ctx, targetType, right);
        xl6OutputPuts(nullableString ? ");\n" : ", 0u);\n", ctx->out);
        if ( !nullableString ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(
                ctx->out,
                "if (__xl6_string_assign_%d == 0) { xl6_runtime_fatal(3); }\n",
                id);
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xrtFree((void*)%s);\n", cName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = __xl6_string_assign_%d;\n", cName, id);
        return;
    }

    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME ) {
        const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);
        const char* cType = targetType != NULL ? xl6CgenTypeName(ctx, targetType) : NULL;
        char cName[512];

        xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
        if ( xl6CgenAssignTryStaticXValueCompareStmt(ctx, targetType, cType, cName, right) ) {
            return;
        }
        if ( xl6CgenAssignTryStaticXValueApproxStmt(ctx, targetType, cType, cName, right) ) {
            return;
        }
        if ( xl6CgenAssignTryStaticXValueScalarStmt(ctx, targetType, cName, right) ) {
            return;
        }
    }

    xl6CgenIndent(ctx);
    if ( xl6CTextEquals(op, "?=") ) {
        if ( left->kind == XL6_AST_NAME && xl6CgenIsXValueType(xl6CgenNameAssignTargetType(ctx, left->text)) ) {
            int id = ctx->tempId++;
            char cName[512];
            const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);

            xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
            xl6OutputPrintf(ctx->out, "if (xl6_xrt_value_is_null(%s)) {\n", cName);
            ctx->indent += 1;
            {
                char assignName[64];
                snprintf(assignName, sizeof(assignName), "__xl6_assign_%d", id);
                xl6CgenXValueLocalInitStmt(ctx, targetType, assignName, right);
            }
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xrtValueRelease(%s);\n", cName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = __xl6_assign_%d;\n", cName, id);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            return;
        }
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(" = (", ctx->out);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(" == 0 ? ", ctx->out);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(");\n", ctx->out);
        return;
    }
    if ( xl6CTextEquals(op, "+=") &&
         xl6CgenExprStaticTypeIs(ctx, left, "string") &&
         xl6CgenExprStaticTypeIs(ctx, right, "string") ) {
        int id = ctx->tempId++;

        xl6OutputPrintf(ctx->out, "const char** __xl6_string_slot_%d = &(", id);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out,
                        "char* __xl6_string_value_%d = xl6_xrt_string_dup(xl6_xrt_str_concat(*__xl6_string_slot_%d, ",
                        id,
                        id);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts("), 0u);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (__xl6_string_value_%d == 0) { xl6_runtime_fatal(3); }\n", id);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xrtFree((void*)*__xl6_string_slot_%d);\n", id);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "*__xl6_string_slot_%d = __xl6_string_value_%d;\n", id, id);
        return;
    }
    if ( xl6CTextEquals(op, "**=") ) {
        if ( !xl6CgenExprIntPowKind(ctx, left) || !xl6CgenExprIntPowKind(ctx, right) ) {
            xl6CgenError(ctx, node, "当前 C 生成 **= 只支持整数表达式");
            return;
        }
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(" = xl6_int_pow_i64(", ctx->out);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(");\n", ctx->out);
        return;
    }

    if ( xl6CgenStaticNumericCompoundXValueStmt(ctx, left, op, right) ) {
        return;
    }

    xl6CgenExpr(ctx, left);
    xl6OutputPrintf(ctx->out, " %s ", op);
    xl6CgenExpr(ctx, right);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenAssignStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left =
        node != NULL && node->childCount > 0 ? node->children[0] : NULL;

    xl6CgenAssignStmtPlanned(
        ctx,
        node,
        xl6CgenHirLvalueKind(ctx, left));
}

static void xl6CgenTimeLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const char* text;
    size_t len;
    const char* body;
    size_t bodyLen;

    text = node != NULL ? node->text : NULL;
    len = text != NULL ? strlen(text) : 0;
    if ( len < 3 || text[0] != '#' || text[len - 1] != '#' ) {
        xl6CgenError(ctx, node, "time literal format must be #yyyy-mm-dd#, #hh:mm:ss# or #yyyy-mm-dd hh:mm:ss#");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    body = text + 1;
    bodyLen = len - 2;
    xl6OutputPrintf(ctx->out,
        "xl6_xrt_time_parse_any_value(\"%.*s\", %u)",
        (int)bodyLen,
        body,
        (unsigned int)bodyLen);
}
