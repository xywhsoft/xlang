/*
    enum 后端统一规则：
    1. 数值 enum 保持底层整数的 C ABI。
    2. tagged enum 使用静态 tag + union payload，默认不分配堆内存。
    3. payload 使用真实 C 类型，XRT 元数据公开字段类型和偏移。
*/

static const XL6_AstNode* xl6CgenEnumVariants(const XL6_AstNode* enumNode)
{
    return enumNode != NULL && enumNode->childCount > 1 ?
        enumNode->children[1] :
        NULL;
}

static const XL6_AstNode* xl6CgenEnumVariantParams(const XL6_AstNode* variant)
{
    return variant != NULL && variant->childCount > 0 ?
        variant->children[0] :
        NULL;
}

static const XL6_AstNode* xl6CgenEnumTypeArgs(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->childCount > 0 &&
           typeNode->children[0] != NULL &&
           typeNode->children[0]->kind == XL6_AST_LIST ?
        typeNode->children[0] :
        NULL;
}

static void xl6CgenEnumDataTypeName(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* typeNode)
{
    xl6CgenEnumTypeRefName(buffer, capacity, typeNode);
}

static void xl6CgenEnumCtorSymbol(
    XL6_Output* out,
    const XL6_AstNode* typeNode,
    const char* variantName)
{
    char cTypeName[512];

    xl6CgenEnumTypeRefName(cTypeName, sizeof(cTypeName), typeNode);
    xl6OutputPuts("xl6_enum_ctor_", out);
    xl6OutputPuts(cTypeName, out);
    xl6OutputPuts("__", out);
    xl6CgenFputsCIdent(out, variantName);
}

static void xl6CgenEnumDataSymbol(
    XL6_Output* out,
    const XL6_AstNode* typeNode)
{
    char cTypeName[512];

    xl6CgenEnumTypeRefName(cTypeName, sizeof(cTypeName), typeNode);
    xl6OutputPuts("xl6_enum_data_", out);
    xl6OutputPuts(cTypeName, out);
}

static void xl6CgenEnumOpsSymbol(
    XL6_Output* out,
    const XL6_AstNode* typeNode,
    const char* suffix)
{
    char cTypeName[512];

    xl6CgenEnumTypeRefName(cTypeName, sizeof(cTypeName), typeNode);
    /* 运行时 type_ops 是生成单元私有符号，不能占用公开 C ABI 名字。 */
    xl6OutputPuts("xl6_enum_op_", out);
    xl6OutputPuts(suffix, out);
    xl6OutputPutc('_', out);
    xl6OutputPuts(cTypeName, out);
}

static XL6_AstNode* xl6CgenEnumMakeSimpleType(
    const XL6_AstNode* enumNode)
{
    XL6_AstNode* typeNode;

    if ( enumNode == NULL || enumNode->text == NULL ) {
        return NULL;
    }
    typeNode = xl6CgenNewSimpleType(enumNode->text, enumNode);
    if ( typeNode != NULL &&
         (enumNode->flags & XL6_AST_FLAG_NUMERIC_ENUM) == 0 ) {
        typeNode->flags |= XL6_AST_FLAG_TAGGED_ENUM;
    }
    return typeNode;
}

static void xl6CgenEnumEnterGeneric(
    XL6_CgenContext* ctx,
    const XL6_AstNode* enumNode,
    const XL6_AstNode* typeNode,
    const XL6_AstNode** previousParams,
    const XL6_AstNode** previousArgs)
{
    *previousParams = ctx->currentGenericParams;
    *previousArgs = ctx->currentGenericArgs;
    if ( xl6CgenEnumHasTypeParams(enumNode) ) {
        ctx->currentGenericParams = xl6CgenEnumTypeParams(enumNode);
        ctx->currentGenericArgs = xl6CgenEnumTypeArgs(typeNode);
    }
}

static void xl6CgenEnumLeaveGeneric(
    XL6_CgenContext* ctx,
    const XL6_AstNode* previousParams,
    const XL6_AstNode* previousArgs)
{
    ctx->currentGenericParams = previousParams;
    ctx->currentGenericArgs = previousArgs;
}

static XL6_AstNode* xl6CgenEnumConcreteFieldType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* enumNode,
    const XL6_AstNode* enumType,
    const XL6_AstNode* fieldType)
{
    const XL6_AstNode* previousParams;
    const XL6_AstNode* previousArgs;
    XL6_AstNode* result;

    xl6CgenEnumEnterGeneric(
        ctx,
        enumNode,
        enumType,
        &previousParams,
        &previousArgs);
    result = xl6CgenCloneCanonicalTypeNode(ctx, fieldType);
    xl6CgenEnumLeaveGeneric(ctx, previousParams, previousArgs);
    return result;
}

static void xl6CgenEnumTypeDescRef(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    char cTypeName[512];

    xl6CgenEnumTypeRefName(cTypeName, sizeof(cTypeName), typeNode);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("()", ctx->out);
}

static void xl6CgenEnumTypeDescProto(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    xl6OutputPuts("static const xl6_type_desc* ", ctx->out);
    xl6CgenEnumTypeDescRef(ctx, typeNode);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenEnumVariantValueSymbol(
    XL6_Output* out,
    const XL6_AstNode* typeNode,
    const char* variantName)
{
    char cTypeName[512];

    xl6CgenEnumTypeRefName(cTypeName, sizeof(cTypeName), typeNode);
    xl6OutputPuts("XL6_ENUM_", out);
    xl6OutputPuts(cTypeName, out);
    xl6OutputPuts("__", out);
    xl6CgenFputsCIdent(out, variantName);
}

static void xl6CgenEnumTagSymbol(
    XL6_Output* out,
    const XL6_AstNode* typeNode,
    const char* variantName)
{
    char cTypeName[512];

    xl6CgenEnumTypeRefName(cTypeName, sizeof(cTypeName), typeNode);
    xl6OutputPuts("XL6_ENUM_TAG_", out);
    xl6OutputPuts(cTypeName, out);
    xl6OutputPuts("__", out);
    xl6CgenFputsCIdent(out, variantName);
}

static void xl6CgenEnumFieldPath(
    XL6_Output* out,
    const char* root,
    const char* variantName,
    const char* fieldName)
{
    xl6OutputPuts(root, out);
    xl6OutputPuts(".payload.", out);
    xl6CgenFputsCIdent(out, variantName);
    xl6OutputPutc('.', out);
    xl6CgenFputsCIdent(out, fieldName);
}

static void xl6CgenEnumFieldPointer(
    XL6_Output* out,
    const char* root,
    const char* variantName,
    const char* fieldName)
{
    xl6OutputPuts("&(", out);
    xl6CgenEnumFieldPath(out, root, variantName, fieldName);
    xl6OutputPutc(')', out);
}

static void xl6CgenEnumEmitFieldCopy(
    XL6_CgenContext* ctx,
    const XL6_AstNode* fieldType,
    const char* dstRoot,
    const char* srcRoot,
    const char* variantName,
    const char* fieldName)
{
    char copyName[80];

    if ( xl6CgenIsXValueType(fieldType) ) {
        xl6OutputPuts("        ", ctx->out);
        xl6CgenEnumFieldPath(ctx->out, dstRoot, variantName, fieldName);
        xl6OutputPuts(" = ", ctx->out);
        xl6CgenEnumFieldPath(ctx->out, srcRoot, variantName, fieldName);
        xl6OutputPuts(" != 0 ? xl6_xrt_retain(", ctx->out);
        xl6CgenEnumFieldPath(ctx->out, srcRoot, variantName, fieldName);
        xl6OutputPuts(") : 0;\n", ctx->out);
        return;
    }
    if ( xl6CgenIsFunctionType(fieldType) ) {
        xl6OutputPuts("        ", ctx->out);
        xl6CgenEnumFieldPath(ctx->out, dstRoot, variantName, fieldName);
        xl6OutputPuts(" = xl6_function_copy_value(", ctx->out);
        xl6CgenEnumFieldPath(ctx->out, srcRoot, variantName, fieldName);
        xl6OutputPuts(");\n", ctx->out);
        return;
    }
    if ( xl6CgenNullableHasManagedInner(ctx, fieldType) ) {
        xl6CgenNullableCopyHelperName(copyName, sizeof(copyName), fieldType);
        xl6OutputPuts("        ", ctx->out);
        xl6CgenEnumFieldPath(ctx->out, dstRoot, variantName, fieldName);
        xl6OutputPrintf(ctx->out, " = %s(", copyName);
        xl6CgenEnumFieldPath(ctx->out, srcRoot, variantName, fieldName);
        xl6OutputPuts(");\n", ctx->out);
        return;
    }
    if ( xl6CgenIsWeakType(fieldType) ) {
        xl6OutputPuts("        xl6WeakCopy(", ctx->out);
        xl6CgenEnumFieldPointer(ctx->out, dstRoot, variantName, fieldName);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEnumFieldPointer(ctx->out, srcRoot, variantName, fieldName);
        xl6OutputPuts(");\n", ctx->out);
        return;
    }

    xl6OutputPuts("        {\n", ctx->out);
    xl6OutputPuts("            const xl6_type_desc* fieldType = ", ctx->out);
    xl6CgenEmitXrtTypeDescRef(ctx, fieldType);
    xl6OutputPuts(";\n", ctx->out);
    xl6OutputPuts("            if ( fieldType != 0 && fieldType->Ops != 0 && fieldType->Ops->copy != 0 ) {\n", ctx->out);
    xl6OutputPuts("                fieldType->Ops->copy(", ctx->out);
    xl6CgenEnumFieldPointer(ctx->out, dstRoot, variantName, fieldName);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenEnumFieldPointer(ctx->out, srcRoot, variantName, fieldName);
    xl6OutputPuts(");\n", ctx->out);
    xl6OutputPuts("            } else {\n", ctx->out);
    xl6OutputPuts("                memcpy(", ctx->out);
    xl6CgenEnumFieldPointer(ctx->out, dstRoot, variantName, fieldName);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenEnumFieldPointer(ctx->out, srcRoot, variantName, fieldName);
    xl6OutputPuts(", sizeof(", ctx->out);
    xl6CgenEnumFieldPath(ctx->out, dstRoot, variantName, fieldName);
    xl6OutputPuts("));\n", ctx->out);
    xl6OutputPuts("            }\n", ctx->out);
    xl6OutputPuts("        }\n", ctx->out);
}

static void xl6CgenEnumEmitFieldDrop(
    XL6_CgenContext* ctx,
    const XL6_AstNode* fieldType,
    const char* root,
    const char* variantName,
    const char* fieldName)
{
    char dropName[80];

    if ( xl6CgenIsXValueType(fieldType) ) {
        xl6OutputPuts("        if (", ctx->out);
        xl6CgenEnumFieldPath(ctx->out, root, variantName, fieldName);
        xl6OutputPuts(" != 0) { xrtValueRelease(", ctx->out);
        xl6CgenEnumFieldPath(ctx->out, root, variantName, fieldName);
        xl6OutputPuts("); ", ctx->out);
        xl6CgenEnumFieldPath(ctx->out, root, variantName, fieldName);
        xl6OutputPuts(" = 0; }\n", ctx->out);
        return;
    }
    if ( xl6CgenIsFunctionType(fieldType) ) {
        xl6OutputPuts("        xl6_function_release_value(", ctx->out);
        xl6CgenEnumFieldPath(ctx->out, root, variantName, fieldName);
        xl6OutputPuts(");\n", ctx->out);
        xl6OutputPuts("        memset(", ctx->out);
        xl6CgenEnumFieldPointer(ctx->out, root, variantName, fieldName);
        xl6OutputPuts(", 0, sizeof(", ctx->out);
        xl6CgenEnumFieldPath(ctx->out, root, variantName, fieldName);
        xl6OutputPuts("));\n", ctx->out);
        return;
    }
    if ( xl6CgenNullableHasManagedInner(ctx, fieldType) ) {
        xl6CgenNullableDropHelperName(dropName, sizeof(dropName), fieldType);
        xl6OutputPrintf(ctx->out, "        %s(", dropName);
        xl6CgenEnumFieldPointer(ctx->out, root, variantName, fieldName);
        xl6OutputPuts(");\n", ctx->out);
        return;
    }
    if ( xl6CgenIsWeakType(fieldType) ) {
        xl6OutputPuts("        xl6WeakUnit(", ctx->out);
        xl6CgenEnumFieldPointer(ctx->out, root, variantName, fieldName);
        xl6OutputPuts(");\n", ctx->out);
        return;
    }

    xl6OutputPuts("        {\n", ctx->out);
    xl6OutputPuts("            const xl6_type_desc* fieldType = ", ctx->out);
    xl6CgenEmitXrtTypeDescRef(ctx, fieldType);
    xl6OutputPuts(";\n", ctx->out);
    xl6OutputPuts("            if ( fieldType != 0 && fieldType->Ops != 0 && fieldType->Ops->drop != 0 ) {\n", ctx->out);
    xl6OutputPuts("                fieldType->Ops->drop(", ctx->out);
    xl6CgenEnumFieldPointer(ctx->out, root, variantName, fieldName);
    xl6OutputPuts(");\n", ctx->out);
    xl6OutputPuts("            }\n", ctx->out);
    xl6OutputPuts("        }\n", ctx->out);
}

static void xl6CgenEnumEmitTaggedOps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* enumNode,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* variants;
    char dataTypeName[544];
    size_t i;

    variants = xl6CgenEnumVariants(enumNode);
    xl6CgenEnumDataTypeName(dataTypeName, sizeof(dataTypeName), typeNode);

    xl6OutputPuts("static void ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "init");
    xl6OutputPrintf(ctx->out, "(void* pObj) { if (pObj != 0) { memset(pObj, 0, sizeof(%s)); } }\n", dataTypeName);

    xl6OutputPuts("static void ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "copy");
    xl6OutputPrintf(ctx->out, "(void* pDst, const void* pSrc)\n{\n    %s* dst = (%s*)pDst;\n    const %s* src = (const %s*)pSrc;\n", dataTypeName, dataTypeName, dataTypeName, dataTypeName);
    xl6OutputPuts("    if (dst == 0) { return; }\n    memset(dst, 0, sizeof(*dst));\n    if (src == 0) { return; }\n    dst->tag = src->tag;\n    switch (src->tag) {\n", ctx->out);
    for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
        const XL6_AstNode* variant = variants->children[i];
        const XL6_AstNode* params = xl6CgenEnumVariantParams(variant);
        size_t p;

        xl6OutputPuts("    case ", ctx->out);
        xl6CgenEnumTagSymbol(ctx->out, typeNode, variant->text);
        xl6OutputPuts(":\n", ctx->out);
        for ( p = 0; params != NULL && p < params->childCount; ++p ) {
            const XL6_AstNode* param = params->children[p];
            const XL6_AstNode* fieldType = param != NULL && param->childCount > 0 ?
                xl6CgenSubstituteGenericType(ctx, param->children[0]) :
                NULL;
            xl6CgenEnumEmitFieldCopy(
                ctx,
                fieldType,
                "(*dst)",
                "(*src)",
                variant->text,
                param->text);
        }
        xl6OutputPuts("        break;\n", ctx->out);
    }
    xl6OutputPuts("    case 0: break;\n    default: xl6_runtime_fatal(3); break;\n    }\n}\n", ctx->out);

    xl6OutputPuts("static void ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "move");
    xl6OutputPrintf(ctx->out, "(void* pDst, void* pSrc) { if (pDst != 0) { if (pSrc != 0) { memcpy(pDst, pSrc, sizeof(%s)); memset(pSrc, 0, sizeof(%s)); } else { memset(pDst, 0, sizeof(%s)); } } }\n", dataTypeName, dataTypeName, dataTypeName);

    xl6OutputPuts("static void ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "drop");
    xl6OutputPrintf(ctx->out, "(void* pObj)\n{\n    %s* value = (%s*)pObj;\n    if (value == 0) { return; }\n    switch (value->tag) {\n", dataTypeName, dataTypeName);
    for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
        const XL6_AstNode* variant = variants->children[i];
        const XL6_AstNode* params = xl6CgenEnumVariantParams(variant);
        size_t p;

        xl6OutputPuts("    case ", ctx->out);
        xl6CgenEnumTagSymbol(ctx->out, typeNode, variant->text);
        xl6OutputPuts(":\n", ctx->out);
        for ( p = 0; params != NULL && p < params->childCount; ++p ) {
            const XL6_AstNode* param = params->children[p];
            const XL6_AstNode* fieldType = param != NULL && param->childCount > 0 ?
                xl6CgenSubstituteGenericType(ctx, param->children[0]) :
                NULL;
            xl6CgenEnumEmitFieldDrop(
                ctx,
                fieldType,
                "(*value)",
                variant->text,
                param->text);
        }
        xl6OutputPuts("        break;\n", ctx->out);
    }
    xl6OutputPuts("    case 0: break;\n    default: xl6_runtime_fatal(3); break;\n    }\n    memset(value, 0, sizeof(*value));\n}\n", ctx->out);

    /*
        tagged enum 默认使用值相等。复杂字段把比较和哈希委托给各自
        TypeDesc；缺少操作时退回稳定的字节比较。
    */
    xl6OutputPuts("static int ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "compare");
    xl6OutputPrintf(ctx->out, "(const void* pA, const void* pB)\n{\n    const %s* a = (const %s*)pA;\n    const %s* b = (const %s*)pB;\n    if (a == 0 || b == 0) { return a == b ? 0 : (a == 0 ? -1 : 1); }\n    if (a->tag != b->tag) { return a->tag < b->tag ? -1 : 1; }\n    switch (a->tag) {\n", dataTypeName, dataTypeName, dataTypeName, dataTypeName);
    for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
        const XL6_AstNode* variant = variants->children[i];
        const XL6_AstNode* params = xl6CgenEnumVariantParams(variant);
        size_t p;

        xl6OutputPuts("    case ", ctx->out);
        xl6CgenEnumTagSymbol(ctx->out, typeNode, variant->text);
        xl6OutputPuts(":\n", ctx->out);
        for ( p = 0; params != NULL && p < params->childCount; ++p ) {
            const XL6_AstNode* param = params->children[p];
            const XL6_AstNode* fieldType = param != NULL && param->childCount > 0 ?
                xl6CgenSubstituteGenericType(ctx, param->children[0]) :
                NULL;

            if ( xl6CgenIsXValueType(fieldType) ) {
                xl6OutputPuts("        if (!xrtValueEqual(", ctx->out);
                xl6CgenEnumFieldPath(ctx->out, "(*a)", variant->text, param->text);
                xl6OutputPuts(", ", ctx->out);
                xl6CgenEnumFieldPath(ctx->out, "(*b)", variant->text, param->text);
                xl6OutputPuts(")) { return (uintptr_t)", ctx->out);
                xl6CgenEnumFieldPath(ctx->out, "(*a)", variant->text, param->text);
                xl6OutputPuts(" < (uintptr_t)", ctx->out);
                xl6CgenEnumFieldPath(ctx->out, "(*b)", variant->text, param->text);
                xl6OutputPuts(" ? -1 : 1; }\n", ctx->out);
            } else {
                xl6OutputPuts("        {\n            const xl6_type_desc* fieldType = ", ctx->out);
                xl6CgenEmitXrtTypeDescRef(ctx, fieldType);
                xl6OutputPuts(";\n            int result = fieldType != 0 && fieldType->Ops != 0 && fieldType->Ops->compare != 0 ? fieldType->Ops->compare(", ctx->out);
                xl6CgenEnumFieldPointer(ctx->out, "(*a)", variant->text, param->text);
                xl6OutputPuts(", ", ctx->out);
                xl6CgenEnumFieldPointer(ctx->out, "(*b)", variant->text, param->text);
                xl6OutputPuts(") : memcmp(", ctx->out);
                xl6CgenEnumFieldPointer(ctx->out, "(*a)", variant->text, param->text);
                xl6OutputPuts(", ", ctx->out);
                xl6CgenEnumFieldPointer(ctx->out, "(*b)", variant->text, param->text);
                xl6OutputPuts(", sizeof(", ctx->out);
                xl6CgenEnumFieldPath(ctx->out, "(*a)", variant->text, param->text);
                xl6OutputPuts("));\n            if (result != 0) { return result < 0 ? -1 : 1; }\n        }\n", ctx->out);
            }
        }
        xl6OutputPuts("        return 0;\n", ctx->out);
    }
    xl6OutputPuts("    case 0: return 0;\n    default: xl6_runtime_fatal(3); return 0;\n    }\n}\n", ctx->out);

    xl6OutputPuts("static uint64 ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "hash");
    xl6OutputPrintf(
        ctx->out,
        "(const void* pObj)\n{\n    const %s* value = (const %s*)pObj;\n    uint64 result;\n    if (value == 0) { return 0; }\n    result = xrtHash64(&value->tag, sizeof(value->tag));\n    switch (value->tag) {\n",
        dataTypeName,
        dataTypeName);
    for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
        const XL6_AstNode* variant = variants->children[i];
        const XL6_AstNode* params = xl6CgenEnumVariantParams(variant);
        size_t p;

        xl6OutputPuts("    case ", ctx->out);
        xl6CgenEnumTagSymbol(ctx->out, typeNode, variant->text);
        xl6OutputPuts(":\n", ctx->out);
        for ( p = 0; params != NULL && p < params->childCount; ++p ) {
            const XL6_AstNode* param = params->children[p];
            const XL6_AstNode* fieldType =
                param != NULL && param->childCount > 0 ?
                    xl6CgenSubstituteGenericType(ctx, param->children[0]) :
                    NULL;

            xl6OutputPuts("        {\n            const xl6_type_desc* fieldType = ", ctx->out);
            xl6CgenEmitXrtTypeDescRef(ctx, fieldType);
            xl6OutputPuts(";\n            uint64 fieldHash = xl6TypeHashValue(fieldType, ", ctx->out);
            xl6CgenEnumFieldPointer(
                ctx->out,
                "(*value)",
                variant->text,
                param->text);
            xl6OutputPuts(");\n            result ^= fieldHash + 0x9e3779b97f4a7c15ULL + (result << 6) + (result >> 2);\n        }\n", ctx->out);
        }
        xl6OutputPuts("        break;\n", ctx->out);
    }
    xl6OutputPuts("    case 0: break;\n    default: xl6_runtime_fatal(3); break;\n    }\n    return result;\n}\n", ctx->out);

    xl6OutputPuts("static xvalue* ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "box");
    xl6OutputPuts("(const void* pObj, const xl6_type_desc* pType) { return pObj != 0 ? xl6ValueRecord(pType, pObj) : xrtValueNull(); }\n", ctx->out);

    xl6OutputPuts("static bool ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "unbox");
    xl6OutputPuts("(xvalue* value, void* pOut, const xl6_type_desc* pType) { const xl6_type_desc* actual; const void* data; if (pOut == 0 || value == 0 || xl6_xrt_value_is_null(value)) { return false; } actual = xl6ValueType(value); if (actual == 0 || pType == 0 || !xl6TypeSame(actual, pType)) { return false; } data = xl6ValueRecordData(value); if (data == 0) { return false; } pType->Ops->copy(pOut, data); return true; }\n", ctx->out);

    xl6OutputPuts("static const xl6_type_ops ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "ops");
    xl6OutputPuts(" = {\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "init");
    xl6OutputPuts(",\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "copy");
    xl6OutputPuts(",\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "move");
    xl6OutputPuts(",\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "drop");
    xl6OutputPuts(",\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "compare");
    xl6OutputPuts(",\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "hash");
    xl6OutputPuts(",\n    0,\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "box");
    xl6OutputPuts(",\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "unbox");
    xl6OutputPuts(",\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "copy");
    xl6OutputPuts("\n};\n\n", ctx->out);

    /*
        C 的结构体传参只做位拷贝，不能直接承担托管 payload 的复制语义。
        所有 borrowed tagged enum 在进入 by-value 存储前统一经过此函数。
    */
    xl6OutputPuts("static ", ctx->out);
    xl6OutputPuts(dataTypeName, ctx->out);
    xl6OutputPutc(' ', ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "copy_value");
    xl6OutputPrintf(
        ctx->out,
        "(%s source)\n{\n    %s result;\n    memset(&result, 0, sizeof(result));\n    ",
        dataTypeName,
        dataTypeName);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "copy");
    xl6OutputPuts("(&result, &source);\n    return result;\n}\n\n", ctx->out);

    xl6OutputPuts("static xvalue* ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "box_value");
    xl6OutputPrintf(ctx->out, "(%s source) { return xl6TypeBoxValue(", dataTypeName);
    xl6CgenEnumTypeDescRef(ctx, typeNode);
    xl6OutputPuts(", &source); }\n\n", ctx->out);
}

static void xl6CgenEnumEmitNumericOps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* enumNode,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* backingType;
    char cTypeName[512];

    if ( ctx == NULL || enumNode == NULL || enumNode->childCount == 0 ||
         typeNode == NULL ) {
        return;
    }
    backingType = enumNode->children[0];
    xl6CgenEnumTypeRefName(cTypeName, sizeof(cTypeName), typeNode);

    xl6OutputPuts("static void ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "init");
    xl6OutputPrintf(ctx->out, "(void* pObj) { if (pObj != 0) { *(%s*)pObj = (%s)0; } }\n", cTypeName, cTypeName);

    xl6OutputPuts("static void ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "copy");
    xl6OutputPrintf(ctx->out, "(void* pDst, const void* pSrc) { if (pDst != 0) { *(%s*)pDst = pSrc != 0 ? *(const %s*)pSrc : (%s)0; } }\n", cTypeName, cTypeName, cTypeName);

    xl6OutputPuts("static void ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "move");
    xl6OutputPrintf(ctx->out, "(void* pDst, void* pSrc) { if (pDst != 0) { *(%s*)pDst = pSrc != 0 ? *(%s*)pSrc : (%s)0; } if (pSrc != 0) { *(%s*)pSrc = (%s)0; } }\n", cTypeName, cTypeName, cTypeName, cTypeName, cTypeName);

    xl6OutputPuts("static void ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "drop");
    xl6OutputPuts("(void* pObj) { (void)pObj; }\n", ctx->out);

    xl6OutputPuts("static int ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "compare");
    xl6OutputPrintf(ctx->out, "(const void* pA, const void* pB) { %s a = pA != 0 ? *(const %s*)pA : (%s)0; %s b = pB != 0 ? *(const %s*)pB : (%s)0; return a < b ? -1 : (a > b ? 1 : 0); }\n", cTypeName, cTypeName, cTypeName, cTypeName, cTypeName, cTypeName);

    xl6OutputPuts("static uint64 ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "hash");
    xl6OutputPrintf(ctx->out, "(const void* pObj) { return pObj != 0 ? xrtHash64(pObj, sizeof(%s)) : 0; }\n", cTypeName);

	xl6OutputPuts("static xvalue* ", ctx->out);
	xl6CgenEnumOpsSymbol(ctx->out, typeNode, "box");
	xl6OutputPuts("(const void* pObj, const xl6_type_desc* pType) { return pObj != 0 ? xl6ValueRecord(pType, pObj) : xrtValueNull(); }\n", ctx->out);

	xl6OutputPuts("static bool ", ctx->out);
	xl6CgenEnumOpsSymbol(ctx->out, typeNode, "unbox");
	xl6OutputPuts("(xvalue* value, void* pOut, const xl6_type_desc* pType) { const xl6_type_desc* actual; const void* data; if (pOut == 0 || value == 0 || xl6_xrt_value_is_null(value)) { return false; } actual = xl6ValueType(value); if (actual == 0 || pType == 0 || !xl6TypeSame(actual, pType)) { return false; } data = xl6ValueRecordData(value); if (data == 0) { return false; } pType->Ops->copy(pOut, data); return true; }\n", ctx->out);

    xl6OutputPuts("static const xl6_type_ops ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "ops");
    xl6OutputPuts(" = {\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "init");
    xl6OutputPuts(",\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "copy");
    xl6OutputPuts(",\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "move");
    xl6OutputPuts(",\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "drop");
    xl6OutputPuts(",\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "compare");
    xl6OutputPuts(",\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "hash");
    xl6OutputPuts(",\n    0,\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "box");
    xl6OutputPuts(",\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "unbox");
    xl6OutputPuts(",\n    ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "copy");
    xl6OutputPuts("\n};\n", ctx->out);

    xl6OutputPuts("static xvalue* ", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "box_value");
    xl6OutputPrintf(ctx->out, "(%s source) { return xl6TypeBoxValue(", cTypeName);
    xl6CgenEnumTypeDescRef(ctx, typeNode);
    xl6OutputPuts(", &source); }\n\n", ctx->out);
}

static void xl6CgenTaggedEnumValueExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* expr)
{
    XL6_HirOwnership ownership;

    if ( ctx == NULL || targetType == NULL || expr == NULL ) {
        xl6OutputPuts("((", ctx != NULL ? ctx->out : NULL);
        if ( ctx != NULL ) {
            xl6OutputPuts(xl6CgenTypeName(ctx, targetType), ctx->out);
            xl6OutputPuts("){0})", ctx->out);
        }
        return;
    }
    ownership = xl6CgenHirOwnership(ctx, expr);
    if ( ownership == XL6_HIR_OWNERSHIP_MOVE ) {
        xl6CgenExpr(ctx, expr);
        return;
    }
    xl6CgenEnumOpsSymbol(ctx->out, targetType, "copy_value");
    xl6OutputPutc('(', ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenEnumEmitMetadata(
    XL6_CgenContext* ctx,
    const XL6_AstNode* enumNode,
    const XL6_AstNode* typeNode,
    int tagged)
{
    const XL6_AstNode* variants;
    char cTypeName[512];
    char dataTypeName[544];
    char typeText[512];
    unsigned long long typeId;
    size_t i;

    variants = xl6CgenEnumVariants(enumNode);
    xl6CgenEnumTypeRefName(cTypeName, sizeof(cTypeName), typeNode);
    xl6CgenEnumDataTypeName(dataTypeName, sizeof(dataTypeName), typeNode);
    typeText[0] = '\0';
    xl6CgenTypeText(ctx, typeNode, typeText, sizeof(typeText));
    typeId = xl6CgenTypeIdForTypeNode(ctx, typeNode);

    for ( i = 0; tagged && variants != NULL && i < variants->childCount; ++i ) {
        const XL6_AstNode* variant = variants->children[i];
        const XL6_AstNode* params = xl6CgenEnumVariantParams(variant);
        if ( params != NULL && params->childCount > 0 ) {
            xl6OutputPrintf(ctx->out, "static xl6_enum_field xl6_enum_fields_%s_%u[%uu];\n", cTypeName, (unsigned)i, (unsigned)params->childCount);
        }
    }
    xl6OutputPrintf(ctx->out, "static xl6_enum_variant xl6_enum_variants_%s[%uu];\n", cTypeName, (unsigned)(variants != NULL ? variants->childCount : 0));
    xl6OutputPrintf(ctx->out, "static xl6_enum_info xl6_enum_info_%s;\n", cTypeName);
    xl6OutputPrintf(ctx->out, "static xl6_type_desc xl6_enum_desc_%s = {\n", cTypeName);
    xl6OutputPrintf(ctx->out, "        0x%08XU,\n", (unsigned int)(typeId & 0xFFFFFFFFULL));
    xl6OutputPuts("        XL6_XRT_TYPE_KIND_ENUM,\n        ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, typeText);
    xl6OutputPuts(",\n", ctx->out);
    xl6OutputPrintf(ctx->out, "        %uu,\n        ", (unsigned)strlen(typeText));
    xl6CgenCStringLiteral(ctx->out, cTypeName);
    xl6OutputPuts(",\n", ctx->out);
    xl6OutputPrintf(ctx->out, "        %uu,\n", (unsigned)strlen(cTypeName));
    xl6OutputPrintf(ctx->out, "        sizeof(%s),\n        _Alignof(%s),\n        ", tagged ? dataTypeName : cTypeName, tagged ? dataTypeName : cTypeName);
    xl6OutputPuts("&", ctx->out);
    xl6CgenEnumOpsSymbol(ctx->out, typeNode, "ops");
    xl6OutputPuts(",\n        0,\n        0,\n        0\n};\n", ctx->out);
    xl6OutputPrintf(ctx->out, "static xonce xl6_enum_once_%s = XRT_ONCE_INIT;\n", cTypeName);
	xl6OutputPrintf(ctx->out, "static bool xl6_enum_init_metadata_%s(ptr pContext)\n{\n    (void)pContext;\n", cTypeName);
    for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
        const XL6_AstNode* variant = variants->children[i];
        const XL6_AstNode* params = tagged ? xl6CgenEnumVariantParams(variant) : NULL;
        size_t p;

        if ( tagged ) {
            for ( p = 0; params != NULL && p < params->childCount; ++p ) {
                const XL6_AstNode* param = params->children[p];
                const XL6_AstNode* fieldType = param != NULL && param->childCount > 0 ?
                    xl6CgenSubstituteGenericType(ctx, param->children[0]) :
                    NULL;
                xl6OutputPrintf(ctx->out, "    xl6_enum_fields_%s_%u[%u].Name = ", cTypeName, (unsigned)i, (unsigned)p);
                xl6CgenCStringLiteral(ctx->out, param->text);
                xl6OutputPuts(";\n", ctx->out);
                xl6OutputPrintf(ctx->out, "    xl6_enum_fields_%s_%u[%u].NameSize = %uu;\n", cTypeName, (unsigned)i, (unsigned)p, (unsigned)strlen(param->text));
                xl6OutputPrintf(ctx->out, "    xl6_enum_fields_%s_%u[%u].Type = ", cTypeName, (unsigned)i, (unsigned)p);
                xl6CgenEmitXrtTypeDescRef(ctx, fieldType);
                xl6OutputPuts(";\n", ctx->out);
                xl6OutputPrintf(ctx->out, "    xl6_enum_fields_%s_%u[%u].Offset = offsetof(%s, payload.", cTypeName, (unsigned)i, (unsigned)p, dataTypeName);
                xl6CgenFputsCIdent(ctx->out, variant->text);
                xl6OutputPutc('.', ctx->out);
                xl6CgenFputsCIdent(ctx->out, param->text);
                xl6OutputPuts(");\n", ctx->out);
            }
        }
        xl6OutputPrintf(ctx->out, "    xl6_enum_variants_%s[%u].Name = ", cTypeName, (unsigned)i);
        xl6CgenCStringLiteral(ctx->out, variant->text);
        xl6OutputPuts(";\n", ctx->out);
        xl6OutputPrintf(ctx->out, "    xl6_enum_variants_%s[%u].NameSize = %uu;\n", cTypeName, (unsigned)i, (unsigned)strlen(variant->text));
        xl6OutputPrintf(ctx->out, "    xl6_enum_variants_%s[%u].Value = ", cTypeName, (unsigned)i);
        if ( tagged ) {
            xl6CgenEnumTagSymbol(ctx->out, typeNode, variant->text);
        } else {
            xl6OutputPuts("(uint64)(", ctx->out);
            xl6CgenEnumVariantValueSymbol(ctx->out, typeNode, variant->text);
            xl6OutputPutc(')', ctx->out);
        }
        xl6OutputPuts(";\n", ctx->out);
        xl6OutputPrintf(ctx->out, "    xl6_enum_variants_%s[%u].FieldCount = %uu;\n", cTypeName, (unsigned)i, (unsigned)(params != NULL ? params->childCount : 0));
        xl6OutputPrintf(ctx->out, "    xl6_enum_variants_%s[%u].Fields = ", cTypeName, (unsigned)i);
        if ( params != NULL && params->childCount > 0 ) {
            xl6OutputPrintf(ctx->out, "xl6_enum_fields_%s_%u", cTypeName, (unsigned)i);
        } else {
            xl6OutputPuts("0", ctx->out);
        }
        xl6OutputPuts(";\n", ctx->out);
        xl6OutputPrintf(ctx->out, "    xl6_enum_variants_%s[%u].Flags = %s;\n", cTypeName, (unsigned)i, params != NULL && params->childCount > 0 ? "XL6_ENUM_VARIANT_HAS_PAYLOAD" : "0u");
    }
    xl6OutputPrintf(ctx->out, "    xl6_enum_info_%s.Type = &xl6_enum_desc_%s;\n", cTypeName, cTypeName);
    xl6OutputPrintf(ctx->out, "    xl6_enum_info_%s.BackingType = ", cTypeName);
    if ( tagged ) {
        xl6OutputPuts("0;\n", ctx->out);
    } else {
        xl6CgenEmitXrtTypeDescRef(ctx, enumNode->children[0]);
        xl6OutputPuts(";\n", ctx->out);
    }
    xl6OutputPrintf(ctx->out, "    xl6_enum_info_%s.VariantCount = %uu;\n", cTypeName, (unsigned)(variants != NULL ? variants->childCount : 0));
    xl6OutputPrintf(ctx->out, "    xl6_enum_info_%s.Variants = xl6_enum_variants_%s;\n", cTypeName, cTypeName);
    xl6OutputPrintf(ctx->out, "    xl6_enum_info_%s.Flags = %s;\n", cTypeName, tagged ? "XL6_ENUM_FLAG_TAGGED" : "XL6_ENUM_FLAG_NUMERIC");
	xl6OutputPrintf(ctx->out, "    xl6_enum_desc_%s.Extra = &xl6_enum_info_%s;\n    return true;\n}\n", cTypeName, cTypeName);
    xl6OutputPuts("static const xl6_type_desc* ", ctx->out);
    xl6CgenEnumTypeDescRef(ctx, typeNode);
    xl6OutputPrintf(
        ctx->out,
		"\n{\n    (void)xrtOnce(&xl6_enum_once_%s, xl6_enum_init_metadata_%s, 0);\n    return &xl6_enum_desc_%s;\n}\n\n",
        cTypeName,
        cTypeName,
        cTypeName);
}

static void xl6CgenEnumEmitTaggedConstructors(
    XL6_CgenContext* ctx,
    const XL6_AstNode* enumNode,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* variants;
    char cTypeName[512];
    char dataTypeName[544];
    size_t i;

    variants = xl6CgenEnumVariants(enumNode);
    xl6CgenEnumTypeRefName(cTypeName, sizeof(cTypeName), typeNode);
    xl6CgenEnumDataTypeName(dataTypeName, sizeof(dataTypeName), typeNode);

    for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
        const XL6_AstNode* variant = variants->children[i];
        const XL6_AstNode* params = xl6CgenEnumVariantParams(variant);
        size_t p;

        xl6OutputPuts("static ", ctx->out);
        xl6OutputPuts(cTypeName, ctx->out);
        xl6OutputPutc(' ', ctx->out);
        xl6CgenEnumCtorSymbol(ctx->out, typeNode, variant->text);
        xl6OutputPutc('(', ctx->out);
        if ( params == NULL || params->childCount == 0 ) {
            xl6OutputPuts("void", ctx->out);
        } else {
            for ( p = 0; p < params->childCount; ++p ) {
                const XL6_AstNode* param = params->children[p];
                if ( p > 0 ) {
                    xl6OutputPuts(", ", ctx->out);
                }
                xl6OutputPuts(
                    xl6CgenParamCType(ctx, xl6CgenSubstituteGenericType(ctx, param->children[0])),
                    ctx->out);
                xl6OutputPutc(' ', ctx->out);
                xl6CgenFputsCIdent(ctx->out, param->text);
            }
        }
        xl6OutputPrintf(ctx->out, ")\n{\n    %s data;\n    memset(&data, 0, sizeof(data));\n    data.tag = ", dataTypeName);
        xl6CgenEnumTagSymbol(ctx->out, typeNode, variant->text);
        xl6OutputPuts(";\n", ctx->out);
        for ( p = 0; params != NULL && p < params->childCount; ++p ) {
            const XL6_AstNode* param = params->children[p];
            const XL6_AstNode* fieldType = xl6CgenSubstituteGenericType(ctx, param->children[0]);
            if ( xl6CgenIsXValueType(fieldType) ) {
                xl6OutputPuts("    ", ctx->out);
                xl6CgenEnumFieldPath(ctx->out, "data", variant->text, param->text);
                xl6OutputPuts(" = ", ctx->out);
                xl6CgenFputsCIdent(ctx->out, param->text);
                xl6OutputPuts(" != 0 ? xl6_xrt_retain(", ctx->out);
                xl6CgenFputsCIdent(ctx->out, param->text);
                xl6OutputPuts(") : 0;\n", ctx->out);
            } else if ( xl6CgenIsFunctionType(fieldType) ) {
                xl6OutputPuts("    ", ctx->out);
                xl6CgenEnumFieldPath(ctx->out, "data", variant->text, param->text);
                xl6OutputPuts(" = xl6_function_copy_value(", ctx->out);
                xl6CgenFputsCIdent(ctx->out, param->text);
                xl6OutputPuts(");\n", ctx->out);
            } else if ( xl6CgenNullableHasManagedInner(ctx, fieldType) ) {
                char copyName[80];

                xl6CgenNullableCopyHelperName(copyName, sizeof(copyName), fieldType);
                xl6OutputPuts("    ", ctx->out);
                xl6CgenEnumFieldPath(ctx->out, "data", variant->text, param->text);
                xl6OutputPrintf(ctx->out, " = %s(", copyName);
                xl6CgenFputsCIdent(ctx->out, param->text);
                xl6OutputPuts(");\n", ctx->out);
            } else if ( xl6CgenIsWeakType(fieldType) ) {
                xl6OutputPuts("    xl6WeakCopy(", ctx->out);
                xl6CgenEnumFieldPointer(ctx->out, "data", variant->text, param->text);
                xl6OutputPuts(", &", ctx->out);
                xl6CgenFputsCIdent(ctx->out, param->text);
                xl6OutputPuts(");\n", ctx->out);
            } else {
                xl6OutputPuts("    {\n        const xl6_type_desc* fieldType = ", ctx->out);
                xl6CgenEmitXrtTypeDescRef(ctx, fieldType);
                xl6OutputPuts(";\n        if (fieldType != 0 && fieldType->Ops != 0 && fieldType->Ops->copy != 0) { fieldType->Ops->copy(", ctx->out);
                xl6CgenEnumFieldPointer(ctx->out, "data", variant->text, param->text);
                xl6OutputPuts(", &", ctx->out);
                xl6CgenFputsCIdent(ctx->out, param->text);
                xl6OutputPuts("); } else { memcpy(", ctx->out);
                xl6CgenEnumFieldPointer(ctx->out, "data", variant->text, param->text);
                xl6OutputPuts(", &", ctx->out);
                xl6CgenFputsCIdent(ctx->out, param->text);
                xl6OutputPuts(", sizeof(", ctx->out);
                xl6CgenEnumFieldPath(ctx->out, "data", variant->text, param->text);
                xl6OutputPuts(")); }\n    }\n", ctx->out);
            }
        }
        xl6OutputPuts("    return data;\n}\n\n", ctx->out);
    }
}

static void xl6CgenEnumEmitPublicApi(
    XL6_CgenContext* ctx,
    const XL6_AstNode* enumNode,
    const XL6_AstNode* typeNode,
    int tagged)
{
    const XL6_AstNode* variants;
    char cTypeName[512];
    size_t i;

    if ( ctx == NULL || enumNode == NULL || typeNode == NULL ||
         (enumNode->flags & XL6_AST_FLAG_PUBLIC) == 0 ) {
        return;
    }
    variants = xl6CgenEnumVariants(enumNode);
    xl6CgenEnumTypeRefName(cTypeName, sizeof(cTypeName), typeNode);

    xl6OutputPrintf(
        ctx->out,
        "XL6_C_API const xl6_type_desc* xl6_enum_type_%s(void) { return ",
        cTypeName);
    xl6CgenEnumTypeDescRef(ctx, typeNode);
    xl6OutputPuts("; }\n", ctx->out);
    if ( !tagged ) {
        xl6OutputPutc('\n', ctx->out);
        return;
    }

    xl6OutputPrintf(
        ctx->out,
        "XL6_C_API bool xl6_enum_copy_%s(%s* out, const %s* source) { if (out == 0 || source == 0) { return false; } memset(out, 0, sizeof(*out)); return xl6TypeCopyValue(",
        cTypeName,
        cTypeName,
        cTypeName);
    xl6CgenEnumTypeDescRef(ctx, typeNode);
    xl6OutputPuts(", out, source); }\n", ctx->out);
    xl6OutputPrintf(
        ctx->out,
        "XL6_C_API bool xl6_enum_move_%s(%s* out, %s* source) { if (out == 0 || source == 0) { return false; } memset(out, 0, sizeof(*out)); return xl6TypeMoveValue(",
        cTypeName,
        cTypeName,
        cTypeName);
    xl6CgenEnumTypeDescRef(ctx, typeNode);
    xl6OutputPuts(", out, source); }\n", ctx->out);
    xl6OutputPrintf(
        ctx->out,
        "XL6_C_API void xl6_enum_drop_%s(%s* value) { if (value != 0) { xl6TypeDropValue(",
        cTypeName,
        cTypeName);
    xl6CgenEnumTypeDescRef(ctx, typeNode);
    xl6OutputPuts(", value); } }\n", ctx->out);

    for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
        const XL6_AstNode* variant = variants->children[i];
        const XL6_AstNode* params = xl6CgenEnumVariantParams(variant);
        size_t p;

        xl6OutputPrintf(ctx->out, "XL6_C_API bool xl6_enum_make_%s__", cTypeName);
        xl6CgenFputsCIdent(ctx->out, variant->text);
        xl6OutputPrintf(ctx->out, "(%s* out", cTypeName);
        for ( p = 0; params != NULL && p < params->childCount; ++p ) {
            const XL6_AstNode* param = params->children[p];
            const XL6_AstNode* fieldType =
                xl6CgenSubstituteGenericType(ctx, param->children[0]);

            xl6OutputPuts(", ", ctx->out);
            xl6OutputPuts(xl6CgenParamCType(ctx, fieldType), ctx->out);
            xl6OutputPutc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, param->text);
        }
        xl6OutputPuts(") { if (out == 0) { return false; } memset(out, 0, sizeof(*out)); *out = ", ctx->out);
        xl6CgenEnumCtorSymbol(ctx->out, typeNode, variant->text);
        xl6OutputPutc('(', ctx->out);
        for ( p = 0; params != NULL && p < params->childCount; ++p ) {
            if ( p > 0 ) {
                xl6OutputPuts(", ", ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, params->children[p]->text);
        }
        xl6OutputPuts("); return true; }\n", ctx->out);
    }
    xl6OutputPutc('\n', ctx->out);
}

static void xl6CgenEnumEmitOne(
    XL6_CgenContext* ctx,
    const XL6_AstNode* enumNode,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* previousParams;
    const XL6_AstNode* previousArgs;
    const XL6_AstNode* variants;
    char cTypeName[512];
    char dataTypeName[544];
    size_t i;

    if ( ctx == NULL || enumNode == NULL || typeNode == NULL ) {
        return;
    }
    xl6CgenEnumEnterGeneric(
        ctx,
        enumNode,
        typeNode,
        &previousParams,
        &previousArgs);
    variants = xl6CgenEnumVariants(enumNode);
    xl6CgenEnumTypeRefName(cTypeName, sizeof(cTypeName), typeNode);

    if ( (enumNode->flags & XL6_AST_FLAG_NUMERIC_ENUM) != 0 ) {
        xl6OutputPrintf(ctx->out, "typedef %s %s;\n", xl6CgenTypeName(ctx, enumNode->children[0]), cTypeName);
        for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
            const XL6_AstNode* variant = variants->children[i];
            xl6OutputPuts("#define ", ctx->out);
            xl6CgenEnumVariantValueSymbol(ctx->out, typeNode, variant->text);
            xl6OutputPrintf(ctx->out, " ((%s)(", cTypeName);
            if ( variant->childCount > 0 ) {
                xl6CgenExpr(ctx, variant->children[0]);
            } else {
                xl6OutputPuts("0", ctx->out);
            }
            xl6OutputPuts("))\n", ctx->out);
        }
        xl6OutputPutc('\n', ctx->out);
        xl6CgenEnumEmitNumericOps(ctx, enumNode, typeNode);
        xl6CgenEnumEmitMetadata(ctx, enumNode, typeNode, 0);
        xl6CgenEnumEmitPublicApi(ctx, enumNode, typeNode, 0);
        xl6CgenEnumLeaveGeneric(ctx, previousParams, previousArgs);
        return;
    }

    xl6CgenEnumDataTypeName(dataTypeName, sizeof(dataTypeName), typeNode);
    for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
        xl6OutputPuts("#define ", ctx->out);
        xl6CgenEnumTagSymbol(ctx->out, typeNode, variants->children[i]->text);
        /*
            0 保留给未初始化/已移动值。这样 {0} 始终可以安全 drop，
            也能在运行时识别损坏的外部 ABI 数据。
        */
        xl6OutputPrintf(ctx->out, " %uu\n", (unsigned)(i + 1u));
    }
    xl6OutputPrintf(ctx->out, "typedef struct %s {\n    uint32_t tag;\n    union {\n", dataTypeName);
    for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
        const XL6_AstNode* variant = variants->children[i];
        const XL6_AstNode* params = xl6CgenEnumVariantParams(variant);
        size_t p;

        if ( params == NULL || params->childCount == 0 ) {
            continue;
        }
        xl6OutputPuts("        struct {\n", ctx->out);
        for ( p = 0; p < params->childCount; ++p ) {
            const XL6_AstNode* param = params->children[p];
            xl6OutputPuts("            ", ctx->out);
            xl6OutputPuts(
                xl6CgenValueStorageCType(
                    ctx,
                    xl6CgenSubstituteGenericType(ctx, param->children[0])),
                ctx->out);
            xl6OutputPutc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, param->text);
            xl6OutputPuts(";\n", ctx->out);
        }
        xl6OutputPuts("        } ", ctx->out);
        xl6CgenFputsCIdent(ctx->out, variant->text);
        xl6OutputPuts(";\n", ctx->out);
    }
    xl6OutputPuts("        unsigned char __empty;\n    } payload;\n} ", ctx->out);
    xl6OutputPuts(dataTypeName, ctx->out);
    xl6OutputPuts(";\n\n", ctx->out);

    xl6CgenEnumEmitTaggedOps(ctx, enumNode, typeNode);
    xl6CgenEnumEmitMetadata(ctx, enumNode, typeNode, 1);
    xl6CgenEnumEmitTaggedConstructors(ctx, enumNode, typeNode);
    xl6CgenEnumEmitPublicApi(ctx, enumNode, typeNode, 1);
    xl6CgenEnumLeaveGeneric(ctx, previousParams, previousArgs);
}

static void xl6CgenEnumDecls(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }

    /*
        所有 descriptor 原型先出现，允许递归 tagged enum 和相互引用。
    */
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* enumNode = ctx->root->children[i];
        XL6_AstNode* typeNode;

        if ( enumNode == NULL || enumNode->kind != XL6_AST_ENUM ||
             xl6CgenEnumHasTypeParams(enumNode) ) {
            continue;
        }
        typeNode = xl6CgenEnumMakeSimpleType(enumNode);
        if ( typeNode != NULL ) {
            xl6CgenEnumTypeDescProto(ctx, typeNode);
            xl6AstFree(typeNode);
        }
    }
    for ( i = 0; i < ctx->genericEnumInstanceCount; ++i ) {
        xl6CgenEnumTypeDescProto(
            ctx,
            ctx->genericEnumInstances[i].typeNode);
    }
    xl6OutputPutc('\n', ctx->out);

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* enumNode = ctx->root->children[i];
        XL6_AstNode* typeNode;

        if ( enumNode == NULL || enumNode->kind != XL6_AST_ENUM ||
             xl6CgenEnumHasTypeParams(enumNode) ) {
            continue;
        }
        typeNode = xl6CgenEnumMakeSimpleType(enumNode);
        if ( typeNode != NULL ) {
            xl6CgenEnumEmitOne(ctx, enumNode, typeNode);
            xl6AstFree(typeNode);
        }
    }
    for ( i = 0; i < ctx->genericEnumInstanceCount; ++i ) {
        xl6CgenEnumEmitOne(
            ctx,
            ctx->genericEnumInstances[i].enumNode,
            ctx->genericEnumInstances[i].typeNode);
    }
}

