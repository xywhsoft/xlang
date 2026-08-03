static int xl6CgenNumericEnumMemberExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* enumNode;
    XL6_AstNode* typeNode;

    if ( ctx == NULL || member == NULL || member->kind != XL6_AST_MEMBER ||
         member->childCount == 0 || member->text == NULL ) {
        return 0;
    }
    receiver = member->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return 0;
    }
    enumNode = xl6CgenFindEnum(ctx, receiver->text);
    if ( enumNode == NULL ||
         (enumNode->flags & XL6_AST_FLAG_NUMERIC_ENUM) == 0 ) {
        return 0;
    }
    typeNode = xl6CgenEnumMakeSimpleType(enumNode);
    if ( typeNode == NULL ) {
        return 0;
    }
    xl6CgenEnumVariantValueSymbol(ctx->out, typeNode, member->text);
    xl6AstFree(typeNode);
    return 1;
}

static int xl6CgenTaggedEnumCtorExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call,
    const XL6_AstNode* member)
{
    XL6_AstNode* resultType;
    XL6_AstNode* enumNode;
    const XL6_AstNode* variants;
    const XL6_AstNode* variant;
    const XL6_AstNode* params;
    const XL6_AstNode* args;
    size_t i;

    if ( ctx == NULL || call == NULL || member == NULL ||
         call->semanticCallKind != XL6_SEM_CALL_ENUM_VARIANT ) {
        return 0;
    }
    resultType = xl6CgenCloneExprTypeForInference(ctx, call);
    enumNode = resultType != NULL ?
        xl6CgenFindEnum(ctx, resultType->text) :
        NULL;
    if ( enumNode == NULL ||
         (enumNode->flags & XL6_AST_FLAG_NUMERIC_ENUM) != 0 ) {
        xl6AstFree(resultType);
        return 0;
    }
    variants = xl6CgenEnumVariants(enumNode);
    variant = NULL;
    for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
        if ( xl6CTextEquals(variants->children[i]->text, member->text) ) {
            variant = variants->children[i];
            break;
        }
    }
    if ( variant == NULL ) {
        xl6CgenError(ctx, member, "tagged enum variant target is missing");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        xl6AstFree(resultType);
        return 1;
    }

    params = xl6CgenEnumVariantParams(variant);
    args = call->childCount > 1 ? call->children[1] : NULL;
    xl6CgenEnumCtorSymbol(ctx->out, resultType, member->text);
    xl6OutputPutc('(', ctx->out);
    xl6CgenParamNodeArgs(ctx, params, args);
    xl6OutputPutc(')', ctx->out);
    xl6AstFree(resultType);
    return 1;
}

static const XL6_AstNode* xl6CgenMatchEnumType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* matchNode,
    XL6_AstNode** ownedType)
{
    XL6_AstNode* typeNode;

    *ownedType = NULL;
    if ( matchNode == NULL || matchNode->childCount == 0 ) {
        return NULL;
    }
    typeNode = xl6CgenCloneExprTypeForInference(ctx, matchNode->children[0]);
    *ownedType = typeNode;
    return typeNode;
}

static const XL6_AstNode* xl6CgenMatchVariant(
    const XL6_AstNode* enumNode,
    const XL6_AstNode* pattern)
{
    const XL6_AstNode* variants;
    size_t i;

    variants = xl6CgenEnumVariants(enumNode);
    for ( i = 0; variants != NULL && pattern != NULL &&
                  i < variants->childCount; ++i ) {
        if ( xl6CTextEquals(variants->children[i]->text, pattern->text) ) {
            return variants->children[i];
        }
    }
    return NULL;
}

static void xl6CgenMatchDataRef(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* valueName,
    const char* dataName)
{
    if ( dataName != NULL && dataName[0] != '\0' ) {
        xl6OutputPuts(dataName, ctx->out);
        return;
    }
    (void)typeNode;
    xl6OutputPuts("(&(", ctx->out);
    xl6OutputPuts(valueName, ctx->out);
    xl6OutputPuts("))", ctx->out);
}

static void xl6CgenMatchCondition(
    XL6_CgenContext* ctx,
    const XL6_AstNode* enumNode,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* pattern,
    const char* valueName,
    const char* dataName)
{
    const XL6_AstNode* compareType = typeNode;
    const XL6_AstNode* literal;
    int nullableTagged;

    if ( pattern == NULL || pattern->kind == XL6_AST_PATTERN_ELSE ) {
        xl6OutputPuts("1", ctx->out);
        return;
    }
    if ( enumNode != NULL &&
         (enumNode->flags & XL6_AST_FLAG_NUMERIC_ENUM) != 0 ) {
        xl6OutputPuts(valueName, ctx->out);
        xl6OutputPuts(" == ", ctx->out);
        xl6CgenEnumVariantValueSymbol(ctx->out, typeNode, pattern->text);
        return;
    }
    if ( enumNode != NULL ) {
        xl6CgenMatchDataRef(ctx, typeNode, valueName, dataName);
        xl6OutputPuts(" != 0 && ", ctx->out);
        xl6CgenMatchDataRef(ctx, typeNode, valueName, dataName);
        xl6OutputPuts("->tag == ", ctx->out);
        xl6CgenEnumTagSymbol(ctx->out, typeNode, pattern->text);
        return;
    }
    if ( pattern->kind == XL6_AST_PATTERN_LITERAL &&
         pattern->childCount > 0 ) {
        literal = pattern->children[0];
        nullableTagged = xl6CgenNullableUsesTaggedValue(typeNode);
        if ( literal != NULL && literal->kind == XL6_AST_NULL ) {
            if ( nullableTagged ) {
                xl6OutputPuts("!", ctx->out);
                xl6OutputPuts(valueName, ctx->out);
                xl6OutputPuts(".hasValue", ctx->out);
            } else {
                xl6OutputPuts(valueName, ctx->out);
                xl6OutputPuts(" == 0", ctx->out);
            }
            return;
        }
        if ( xl6CgenIsNullableType(typeNode) ) {
            compareType = xl6CgenNullableInnerType(typeNode);
            if ( nullableTagged ) {
                xl6OutputPuts(valueName, ctx->out);
                xl6OutputPuts(".hasValue && ", ctx->out);
            } else {
                xl6OutputPuts(valueName, ctx->out);
                xl6OutputPuts(" != 0 && ", ctx->out);
            }
        }
        if ( xl6CgenIsOwnedStringTypeNode(ctx, compareType) ) {
            xl6OutputPuts("xrtStrComp(", ctx->out);
            xl6OutputPuts(valueName, ctx->out);
            if ( nullableTagged ) {
                xl6OutputPuts(".value", ctx->out);
            }
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, literal);
            xl6OutputPuts(", 0u, true) == 0", ctx->out);
            return;
        }
        xl6OutputPuts(valueName, ctx->out);
        if ( nullableTagged ) {
            xl6OutputPuts(".value", ctx->out);
        }
        xl6OutputPuts(" == ", ctx->out);
        xl6CgenExpr(ctx, literal);
        return;
    }
    xl6OutputPuts("0", ctx->out);
}

static void xl6CgenMatchBindingInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* binding,
    const XL6_AstNode* fieldType,
    const char* dataName,
    const char* variantName,
    const char* fieldName)
{
    char cName[512];
    char copyName[80];

    xl6CgenValueStorageNameText(
        ctx,
        cName,
        sizeof(cName),
        binding->text);
    xl6CgenIndent(ctx);
    xl6OutputPuts(xl6CgenValueStorageCType(ctx, fieldType), ctx->out);
    xl6OutputPutc(' ', ctx->out);
    xl6OutputPuts(cName, ctx->out);
    xl6OutputPuts(" = {0};\n", ctx->out);
    if ( xl6CgenIsXValueType(fieldType) ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts(cName, ctx->out);
        xl6OutputPuts(" = ", ctx->out);
        xl6OutputPuts(dataName, ctx->out);
        xl6OutputPuts("->payload.", ctx->out);
        xl6CgenFputsCIdent(ctx->out, variantName);
        xl6OutputPutc('.', ctx->out);
        xl6CgenFputsCIdent(ctx->out, fieldName);
        xl6OutputPuts(" != 0 ? xl6_xrt_retain(", ctx->out);
        xl6OutputPuts(dataName, ctx->out);
        xl6OutputPuts("->payload.", ctx->out);
        xl6CgenFputsCIdent(ctx->out, variantName);
        xl6OutputPutc('.', ctx->out);
        xl6CgenFputsCIdent(ctx->out, fieldName);
        xl6OutputPuts(") : 0;\n", ctx->out);
    } else if ( xl6CgenIsFunctionType(fieldType) ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts(cName, ctx->out);
        xl6OutputPuts(" = ", ctx->out);
        xl6OutputPuts("xl6_function_copy_value(", ctx->out);
        xl6OutputPuts(dataName, ctx->out);
        xl6OutputPuts("->payload.", ctx->out);
        xl6CgenFputsCIdent(ctx->out, variantName);
        xl6OutputPutc('.', ctx->out);
        xl6CgenFputsCIdent(ctx->out, fieldName);
        xl6OutputPuts(");\n", ctx->out);
    } else if ( xl6CgenNullableHasManagedInner(ctx, fieldType) ) {
        xl6CgenNullableCopyHelperName(copyName, sizeof(copyName), fieldType);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = %s(", cName, copyName);
        xl6OutputPuts(dataName, ctx->out);
        xl6OutputPuts("->payload.", ctx->out);
        xl6CgenFputsCIdent(ctx->out, variantName);
        xl6OutputPutc('.', ctx->out);
        xl6CgenFputsCIdent(ctx->out, fieldName);
        xl6OutputPuts(");\n", ctx->out);
    } else if ( xl6CgenIsWeakType(fieldType) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvoWeakCopy(&%s, &(", cName);
        xl6OutputPuts(dataName, ctx->out);
        xl6OutputPuts("->payload.", ctx->out);
        xl6CgenFputsCIdent(ctx->out, variantName);
        xl6OutputPutc('.', ctx->out);
        xl6CgenFputsCIdent(ctx->out, fieldName);
        xl6OutputPuts("));\n", ctx->out);
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPuts("{\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("const xrt_type_desc* fieldType = ", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, fieldType);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (fieldType != 0 && fieldType->Ops != 0 && fieldType->Ops->copy != 0) {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "fieldType->Ops->copy(&%s, &(", cName);
        xl6OutputPuts(dataName, ctx->out);
        xl6OutputPuts("->payload.", ctx->out);
        xl6CgenFputsCIdent(ctx->out, variantName);
        xl6OutputPutc('.', ctx->out);
        xl6CgenFputsCIdent(ctx->out, fieldName);
        xl6OutputPuts("));\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("} else {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", cName);
        xl6OutputPuts(dataName, ctx->out);
        xl6OutputPuts("->payload.", ctx->out);
        xl6CgenFputsCIdent(ctx->out, variantName);
        xl6OutputPutc('.', ctx->out);
        xl6CgenFputsCIdent(ctx->out, fieldName);
        xl6OutputPuts(";\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    }
    xl6CgenAddVarType(ctx, binding->text, fieldType);
    xl6CgenAddManagedCleanupForType(ctx, binding->text, fieldType);
}

static size_t xl6CgenMatchCountSymbolRefs(
    const XL6_AstNode* node,
    uint32 symbolId)
{
    size_t count;
    size_t i;

    if ( node == NULL || symbolId == 0 ) {
        return 0;
    }
    count = node->kind == XL6_AST_NAME &&
            (node->semanticSymbolId == symbolId ||
             node->semanticTargetSymbolId == symbolId) ?
        1u :
        0u;
    for ( i = 0; i < node->childCount; ++i ) {
        count += xl6CgenMatchCountSymbolRefs(node->children[i], symbolId);
    }
    return count;
}

static void xl6CgenMatchFillSymbolRefs(
    const XL6_AstNode* node,
    uint32 symbolId,
    const char* cName,
    unsigned int flags,
    XL6_CgenExprReplacement* replacements,
    size_t* index)
{
    size_t i;

    if ( node == NULL || symbolId == 0 || replacements == NULL ||
         index == NULL ) {
        return;
    }
    if ( node->kind == XL6_AST_NAME &&
         (node->semanticSymbolId == symbolId ||
          node->semanticTargetSymbolId == symbolId) ) {
        replacements[*index].expr = node;
        replacements[*index].cName = cName;
        replacements[*index].flags = flags;
        *index += 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenMatchFillSymbolRefs(
            node->children[i],
            symbolId,
            cName,
            flags,
            replacements,
            index);
    }
}

static char* xl6CgenMatchBindingPath(
    const XL6_AstNode* typeNode,
    const char* valueName,
    const char* variantName,
    const char* fieldName)
{
    char variantIdent[XL6_CGEN_C_IDENT_MAX + 1];
    char fieldIdent[XL6_CGEN_C_IDENT_MAX + 1];
    char path[1400];

    (void)typeNode;
    xl6CgenCIdentText(variantIdent, sizeof(variantIdent), variantName);
    xl6CgenCIdentText(fieldIdent, sizeof(fieldIdent), fieldName);
    snprintf(
        path,
        sizeof(path),
        "(%s).payload.%s.%s",
        valueName,
        variantIdent,
        fieldIdent);
    return (char*)xrtCopyStr((str)path, strlen(path));
}

static void xl6CgenMatchArmExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* branch,
    const XL6_AstNode* enumNode,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* variant,
    const XL6_AstNode* pattern,
    const char* valueName)
{
    const XL6_CgenExprReplacement* previous;
    XL6_CgenExprReplacement* replacements;
    char** ownedNames;
    size_t previousCount;
    size_t extraCount;
    size_t replacementCount;
    size_t ownedCount;
    size_t varMark;
    size_t i;

    if ( enumNode == NULL || variant == NULL || pattern == NULL ||
         pattern->childCount == 0 ) {
        xl6CgenExpr(ctx, branch);
        return;
    }

    extraCount = 0;
    for ( i = 0; i < pattern->childCount; ++i ) {
        extraCount += xl6CgenMatchCountSymbolRefs(
            branch,
            pattern->children[i]->semanticSymbolId);
    }
    if ( extraCount == 0 ) {
        xl6CgenExpr(ctx, branch);
        return;
    }

    previous = ctx->exprReplacements;
    previousCount = ctx->exprReplacementCount;
    replacements = (XL6_CgenExprReplacement*)xrtCalloc(
        previousCount + extraCount,
        sizeof(XL6_CgenExprReplacement));
    ownedNames = (char**)xrtCalloc(
        pattern->childCount,
        sizeof(char*));
    if ( replacements == NULL || ownedNames == NULL ) {
        xrtFree(replacements);
        xrtFree(ownedNames);
        xl6CgenError(
            ctx,
            branch,
            "out of memory while lowering match expression bindings");
        xl6OutputPuts("0", ctx->out);
        return;
    }
    if ( previousCount > 0 ) {
        memcpy(
            replacements,
            previous,
            previousCount * sizeof(XL6_CgenExprReplacement));
    }
    replacementCount = previousCount;
    ownedCount = 0;
    varMark = ctx->varCount;
    for ( i = 0; i < pattern->childCount; ++i ) {
        const XL6_AstNode* binding = pattern->children[i];
        const XL6_AstNode* params = xl6CgenEnumVariantParams(variant);
        const XL6_AstNode* param = params != NULL && i < params->childCount ?
            params->children[i] :
            NULL;
        XL6_AstNode* fieldType = param != NULL && param->childCount > 0 ?
            xl6CgenEnumConcreteFieldType(
                ctx,
                enumNode,
                typeNode,
                param->children[0]) :
            NULL;
        unsigned int flags =
            fieldType != NULL && xl6CgenIsXValueType(fieldType) ?
                XL6_CGEN_EXPR_REPLACEMENT_XVALUE :
                0u;
        char* path = xl6CgenMatchBindingPath(
            typeNode,
            valueName,
            variant->text,
            param != NULL ? param->text : binding->text);

        if ( path == NULL ) {
            xl6AstFree(fieldType);
            continue;
        }
        if ( fieldType != NULL ) {
            size_t ownedTypeMark = ctx->ownedTypeNodeCount;

            /*
                match 表达式通过替换路径直接读取 payload，但后端的成员、索引和
                重载选择仍需要知道绑定名的静态类型。该类型只在当前 arm 内有效。
            */
            xl6CgenOwnTypeNode(ctx, fieldType);
            if ( ctx->ownedTypeNodeCount > ownedTypeMark ) {
                xl6CgenAddVarType(ctx, binding->text, fieldType);
                fieldType = NULL;
            }
        }
        ownedNames[ownedCount++] = path;
        xl6CgenMatchFillSymbolRefs(
            branch,
            binding->semanticSymbolId,
            path,
            flags,
            replacements,
            &replacementCount);
        xl6AstFree(fieldType);
    }

    ctx->exprReplacements = replacements;
    ctx->exprReplacementCount = replacementCount;
    xl6CgenExpr(ctx, branch);
    ctx->exprReplacements = previous;
    ctx->exprReplacementCount = previousCount;
    xl6CgenFreeVarsSince(ctx, varMark);

    for ( i = 0; i < ownedCount; ++i ) {
        xrtFree(ownedNames[i]);
    }
    xrtFree(ownedNames);
    xrtFree(replacements);
}

static void xl6CgenMatchExprAssignValue(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* value,
    const char* valueName)
{
    char copyName[80];
    char dropName[80];

    if ( xl6CgenIsTaggedEnumType(ctx, typeNode) ) {
        xl6OutputPuts("xrtTypeDropValue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, typeNode);
        xl6OutputPuts(", &", ctx->out);
        xl6OutputPuts(valueName, ctx->out);
        xl6OutputPuts("), ", ctx->out);
        xl6OutputPuts(valueName, ctx->out);
        xl6OutputPuts(" = ", ctx->out);
        xl6CgenTaggedEnumValueExpr(ctx, typeNode, value);
        return;
    }
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6OutputPuts("xvoUnref(", ctx->out);
        xl6OutputPuts(valueName, ctx->out);
        xl6OutputPuts("), ", ctx->out);
        xl6OutputPuts(valueName, ctx->out);
        xl6OutputPuts(" = ", ctx->out);
        xl6CgenXValueFromExprForTarget(ctx, typeNode, value);
        return;
    }
    if ( xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
        xl6OutputPuts("xrtFree((void*)", ctx->out);
        xl6OutputPuts(valueName, ctx->out);
        xl6OutputPuts("), ", ctx->out);
        xl6OutputPuts(valueName, ctx->out);
        xl6OutputPuts(" = ", ctx->out);
        if ( xl6CgenIsNullableStringTypeNode(ctx, typeNode) ) {
            xl6OutputPuts("xl6_nullable_string_copy(", ctx->out);
            xl6CgenExpr(ctx, value);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6OutputPuts("xrtCopyStr((str)(", ctx->out);
            xl6CgenExpr(ctx, value);
            xl6OutputPuts("), 0u)", ctx->out);
        }
        return;
    }
    if ( xl6CgenNullableHasManagedInner(ctx, typeNode) ) {
        xl6CgenNullableDropHelperName(dropName, sizeof(dropName), typeNode);
        xl6CgenNullableCopyHelperName(copyName, sizeof(copyName), typeNode);
        xl6OutputPrintf(ctx->out, "%s(&%s), %s = %s(", dropName, valueName, valueName, copyName);
        xl6CgenExpr(ctx, value);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    if ( xl6CgenIsWeakType(typeNode) ) {
        xl6OutputPuts("xvoWeakUnit(&", ctx->out);
        xl6OutputPuts(valueName, ctx->out);
        xl6OutputPuts("), xvoWeakCopy(&", ctx->out);
        xl6OutputPuts(valueName, ctx->out);
        xl6OutputPuts(", &(", ctx->out);
        xl6CgenExpr(ctx, value);
        xl6OutputPuts("))", ctx->out);
        return;
    }
    xl6OutputPuts(valueName, ctx->out);
    xl6OutputPuts(" = ", ctx->out);
    xl6CgenExpr(ctx, value);
}

static void xl6CgenMatchExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* value;
    const XL6_AstNode* cases;
    const XL6_AstNode* typeNode;
    XL6_AstNode* ownedType;
    XL6_AstNode* resultType;
    XL6_AstNode* enumNode;
    XL6_CgenCoalesceSlot* slot;
    char valueName[64];
    size_t i;
    int terminal;

    if ( ctx == NULL || node == NULL || node->childCount < 2 ) {
        xl6OutputPuts("0", ctx != NULL ? ctx->out : NULL);
        return;
    }
    value = node->children[0];
    cases = node->children[1];
    typeNode = xl6CgenMatchEnumType(ctx, node, &ownedType);
    enumNode = typeNode != NULL ? xl6CgenFindEnum(ctx, typeNode->text) : NULL;
    resultType = xl6CgenCloneExprTypeForInference(ctx, node);
    slot = xl6CgenFindCoalesceSlot(ctx, node);
    if ( typeNode == NULL || resultType == NULL || slot == NULL ) {
        xl6CgenError(ctx, node, "match expression temporary was not prepared");
        xl6OutputPuts("0", ctx->out);
        xl6AstFree(ownedType);
        xl6AstFree(resultType);
        return;
    }
    xl6CgenCoalesceSlotName(valueName, sizeof(valueName), slot);

    xl6OutputPuts("((", ctx->out);
    xl6CgenMatchExprAssignValue(ctx, typeNode, value, valueName);
    xl6OutputPuts("), (", ctx->out);
    terminal = 0;
    for ( i = 0; cases != NULL && i < cases->childCount; ++i ) {
        const XL6_AstNode* matchCase = cases->children[i];
        const XL6_AstNode* pattern =
            matchCase != NULL && matchCase->childCount > 0 ?
                matchCase->children[0] :
                NULL;
        const XL6_AstNode* branch =
            matchCase != NULL && matchCase->childCount > 1 ?
                matchCase->children[1] :
                NULL;
        const XL6_AstNode* variant = xl6CgenMatchVariant(enumNode, pattern);

        if ( pattern != NULL && pattern->kind == XL6_AST_PATTERN_ELSE ) {
            xl6CgenMatchArmExpr(
                ctx,
                branch,
                enumNode,
                typeNode,
                variant,
                pattern,
                valueName);
            terminal = 1;
            break;
        }
        xl6OutputPutc('(', ctx->out);
        xl6CgenMatchCondition(
            ctx,
            enumNode,
            typeNode,
            pattern,
            valueName,
            "");
        xl6OutputPuts(") ? (", ctx->out);
        xl6CgenMatchArmExpr(
            ctx,
            branch,
            enumNode,
            typeNode,
            variant,
            pattern,
            valueName);
        xl6OutputPuts(") : ", ctx->out);
    }
    if ( !terminal ) {
        xl6OutputPuts("(xl6_runtime_fatal(3), (", ctx->out);
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, resultType), ctx->out);
        xl6OutputPuts("){0})", ctx->out);
    }
    xl6OutputPuts("))", ctx->out);

    xl6AstFree(ownedType);
    xl6AstFree(resultType);
}

static void xl6CgenMatchStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* value;
    const XL6_AstNode* cases;
    const XL6_AstNode* typeNode;
    XL6_AstNode* ownedType;
    XL6_AstNode* enumNode;
    char valueName[64];
    char dataName[64];
    char dataTypeName[544];
    size_t cleanupMark;
    size_t i;
    int id;
    int hasElse;

    if ( ctx == NULL || node == NULL || node->childCount < 2 ) {
        return;
    }
    value = node->children[0];
    cases = node->children[1];
    typeNode = xl6CgenMatchEnumType(ctx, node, &ownedType);
    enumNode = typeNode != NULL ? xl6CgenFindEnum(ctx, typeNode->text) : NULL;
    id = ctx->tempId++;
    snprintf(valueName, sizeof(valueName), "__xl6_match_value_%d", id);
    snprintf(dataName, sizeof(dataName), "__xl6_match_data_%d", id);
    cleanupMark = ctx->cleanupCount;
    hasElse = 0;

    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    if ( enumNode != NULL &&
         (enumNode->flags & XL6_AST_FLAG_NUMERIC_ENUM) == 0 ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "%s %s = ",
            xl6CgenTypeName(ctx, typeNode),
            valueName);
        xl6CgenTaggedEnumValueExpr(ctx, typeNode, value);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenAddTypedValueCleanup(ctx, valueName, typeNode);
        xl6CgenEnumDataTypeName(dataTypeName, sizeof(dataTypeName), typeNode);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "const %s* %s = &%s;\n", dataTypeName, dataName, valueName);
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "%s %s = ",
            xl6CgenTypeName(ctx, typeNode),
            valueName);
        xl6CgenExpr(ctx, value);
        xl6OutputPuts(";\n", ctx->out);
        dataName[0] = '\0';
    }

    for ( i = 0; cases != NULL && i < cases->childCount; ++i ) {
        const XL6_AstNode* matchCase = cases->children[i];
        const XL6_AstNode* pattern = matchCase != NULL && matchCase->childCount > 0 ?
            matchCase->children[0] :
            NULL;
        const XL6_AstNode* block = matchCase != NULL && matchCase->childCount > 1 ?
            matchCase->children[1] :
            NULL;
        const XL6_AstNode* variant = xl6CgenMatchVariant(enumNode, pattern);
        const XL6_AstNode* params = xl6CgenEnumVariantParams(variant);
        size_t varMark = ctx->varCount;
        size_t bindingCleanupMark = ctx->cleanupCount;
        size_t p;

        if ( pattern != NULL && pattern->kind == XL6_AST_PATTERN_ELSE ) {
            hasElse = 1;
        }

        xl6CgenIndent(ctx);
        if ( i == 0 ) {
            xl6OutputPuts("if (", ctx->out);
        } else {
            xl6OutputPuts("else if (", ctx->out);
        }
        xl6CgenMatchCondition(
            ctx,
            enumNode,
            typeNode,
            pattern,
            valueName,
            dataName);
        xl6OutputPuts(") {\n", ctx->out);
        ctx->indent += 1;
        for ( p = 0; pattern != NULL && params != NULL &&
                      p < pattern->childCount && p < params->childCount; ++p ) {
            const XL6_AstNode* binding = pattern->children[p];
            const XL6_AstNode* param = params->children[p];
            XL6_AstNode* fieldType = xl6CgenEnumConcreteFieldType(
                ctx,
                enumNode,
                typeNode,
                param->children[0]);
            if ( fieldType == NULL ) {
                continue;
            }
            xl6CgenOwnTypeNode(ctx, fieldType);
            xl6CgenMatchBindingInit(
                ctx,
                binding,
                fieldType,
                dataName,
                variant->text,
                param->text);
        }
        xl6CgenBlock(ctx, block);
        xl6CgenCloseScope(ctx, bindingCleanupMark);
        xl6CgenFreeVarsSince(ctx, varMark);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    }
    if ( !hasElse ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("else {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("xl6_runtime_fatal(3);\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    }
    xl6CgenCloseScope(ctx, cleanupMark);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6AstFree(ownedType);
}
